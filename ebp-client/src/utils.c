/*
 * utils.c
 * Contains various utility functions to manage various linked lists,
 * free memory from these lists,etc
 *
 * Copyright (C) 2010, eBrain.
 *
 * Author: Jatin Golani
 * 
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
 * 02111-1307, USA.
 */

#include "ebp.h"


/** Add a new user to the User (struct _user) linked list. 
 *
 *  Called by process_useronline_msg() and process_useronline_avahi_msg() when a new user comes online.
 *
 */
User* add_user(User *head,const char* version,const char* name,const char *ssh_login_user,uint32_t ip)
{
    User *ptr = NULL;
    char *strip = NULL;
    struct in_addr in;
    User* temp = NULL;

    in.s_addr = ip;  //jeetu - this is wrong. ip should actually be the ip of the client sending request
    strip = inet_ntoa(in); 

    //! First searches an existing User linked list (if gUserListHead != NULL) for a matching ip.
    //! If found the function returns with a NULL. 
    temp = head;
    while(temp != NULL)
         {
         if(temp->ip == ip)
           return NULL;
         temp = temp->next;
         }

    //! Finally,inserts the following into the new linked list: 
    //! - name (as stated in ebp.conf)
    //! - ssh_login_user (the username that the OpenSSH client will need to connect back to this host)
    //! - version (eBrainPool client version)
    //! - ip (IPv4 address of the user)
    ptr = (User *) malloc (sizeof(User));
    strncpy(ptr->name,name,20); //jeetu - hardcoded size
    strncpy(ptr->ssh_login_user,ssh_login_user,256);
    strncpy(ptr->version,version,6);
    ptr->ip = ip;
    ptr->next = head;

    return ptr;
}


/** Delete a user from the User (struct _user) linked list.
 *
 *  Called by show_user_offline()
 *  returns a User* to the next item in the linked list or NULL if there are no other items.
 */
User* del_user(User **head,User* deluser)
{
    User **cur;
    for(cur = head;*cur; )
       {
       User *entry = *cur;
       if(entry == deluser)
         {
         *cur = entry->next;
         free(entry);
         }
       else
         cur = &entry->next;
       }
}




/** Adds a new connection to the client into the NewConnData (struct _newconndata) linked list.
 *
 *  Called by connlistener_thread()
 */
NewConnData *add_newconn(int newsockfd,uint32_t ip)
{

    NewConnData *ptr = (NewConnData *) malloc(sizeof(NewConnData));
    ptr->newsockfd = newsockfd;
    ptr->ip = ip;

    return ptr;
}


/** Deletes a connection from the NewConnData (struct _newconndata) linked list.
 *
 *  Called by newconnrequests_thread() to delete the connection after it has finished processing it.
 *  returns a NewConnData* to the next item in the linked list or NULL if there are no other items. 
 */
NewConnData *del_newconn(NewConnData *conndata)
{
    free(conndata);
    return NULL;
}



/** Adds details of LaunchApp request to the LaunchDialogQueue (struct _launchdialogqueue) linked list.
 *
 *  Called by process_launchapp_req()
 */
LaunchDialogQueue *add_launchdialog_queue(char *username,char *appname,uint32_t ip)
{
   LaunchDialogQueue* temp = NULL;
   LaunchDialogQueue* current_node = NULL;

   // If we have to start a new linked list (gFirstLaunchDialog == NULL) or add this linked list
   // to the end of the existing linked list,then allocate memory and setup pointers accordingly.

   // first entry		
   if(gFirstLaunchDialog == NULL)
     {
     // memory freed in main() by call to freeLaunchDialogQueue
     current_node = (LaunchDialogQueue *) malloc(sizeof(LaunchDialogQueue));
     current_node->prev = NULL;
     current_node->next = NULL;
     gFirstLaunchDialog = current_node;
     gLastLaunchDialog = current_node;
     }
   else
     {	
     // memory freed in main() by call to freeLaunchDialogQueue
     current_node = gLastLaunchDialog;
     temp = current_node;
     current_node->next = (LaunchDialogQueue *) malloc(sizeof(LaunchDialogQueue));
     current_node = current_node->next;
     current_node->prev = temp;
     current_node->next = NULL;
     gLastLaunchDialog = current_node;
     }

   //! Inserts the following into the linked list:
   //! - username 
   //! - appname (Application name the user wishes to launch)
   //! - ip (IPv4 address of the client connecting in)
   strncpy(current_node->username,username,90);
   strncpy(current_node->appname,appname,90);
   current_node->ip = ip;

   //! returns pointer to the newly created linked list item (LaunchDialogQueue *).
   return current_node;

}



/** Adds sent launch application (LaunchApp) requests to LaunchAppQueue (struct _launchappqueue)
 * 
 *  Called by send_launchapp_req()
 *
 *  To be used on receipt of response to verify authenticity of the request.
 *  NOTE : As on 18/5/2012 the LaunchAppQueue (product of this function) is not being looked into to verify the response.
 *         This may (or may not) be implemented in the future.
 */
LaunchAppQueue* add_to_launch_queue(LaunchAppQueue *head,char *appname,uint32_t ip,int reqid)
{
    LaunchAppQueue *ptr = (LaunchAppQueue *) malloc(sizeof(LaunchAppQueue));

    //! Inserts the following into the linked list:
    //! - appname (Name of application to be launched)
    //! - ip (IPv4 address 	
    strncpy(ptr->appname,appname,20); //jeetu - hardcoded size  
    ptr->ip = ip;
    ptr->reqid = reqid;
    ptr->next = head;

    return ptr;

}

/** Gets a list of applications installed on the host system that can be shared. 
 *
 *  Called by main()
 *  
 *  Looks in /usr/share/applications for applications installed. Reads the 
 *  .desktop files and retrieves the Exec field values.
 *
 *  returns a char* list in the format [app1 exec]:[app2 exec]:...
 */
char* get_installed_apps(int* count,int* blocksize)
{	
    int n = 0;
    int i = 0;
    struct dirent **namelist = NULL;
    int size = 0;
    char *buf = malloc(1);
    char file[256];
    file[0] = '\0';
    
    gchar *retval;
    GKeyFile *key_file;
    key_file = g_key_file_new (); 
    GError   *error;
    
    // memory should get freed in main() with free(appsdata.apps)
    buf[0] = '\0';
    
    // calls filter() which helps find .desktop files in the directory. 	
    n = scandir("/usr/share/applications", &namelist, filter, alphasort);
    if(n < 0)
      perror("list_installed_apps: error scandir");
    else 
      {
      while(i < n)
        {
    
        error = NULL;
        strcpy(file,"/usr/share/applications/");
        snprintf(file,25+strlen(namelist[i]->d_name),"/usr/share/applications/%s",namelist[i]->d_name);
        if(!g_key_file_load_from_file (key_file,  file, 0, &error))
          {
          printf("Failed to load \"%s\": %s\n",  namelist[i]->d_name, error->message);
          }
        else
          {
          retval =  g_key_file_get_locale_string(key_file, "Desktop Entry", "Exec", NULL, &error);
          //! Currently we are not handling arguments, split at first whitespace
          //! TODO: policy for handling args, across remote and
          //! code to implement the same, separate *.desktop & *-file.desktop
          retval = strtok(retval, " ");
          if(retval!= NULL) 
            {
            size = size + strlen(retval) + 2;
            buf = realloc(buf, size);
                     
            strncat(buf, retval, strlen(retval));
            strncat(buf,":",1);
            }
          }           
      	free(namelist[i]);
      	i++;
        } 
      free(namelist);
      }	

    g_key_file_free(key_file);	  
    *blocksize = strlen(buf);
    *count = n;
    return(buf);
}


/** Used by scandir() to filter out and return .desktop files
 *
 *  called by get_installed_apps()
 */
int filter(const struct dirent *dir)
{
    if(strstr(dir->d_name,".desktop") != NULL)  
      return 1;
    else
      return 0;
}


/** Sends request to remote client to send out the list of applications installed on the host.
 *
 *  called by show_user_online()
 *
 *  Connects to remote client. Sends out a GetApps request handled by the remote client in
 *  it's newconnrequests_thread().
 *  returns 1 on error else 0.
 */
int get_remoteuser_apps(User *UserNode)
{
    int ret = 0;
    int comm_socket = 0; 
    int n = 0;
  
    ret = connect_to_client(UserNode->ip,&comm_socket);
    if(ret == 0)
      {
      n = write(comm_socket,"GetApps:",9);
      if(n < 0) 
        perror("get_remoteuser_apps: ERROR writing to socket");
      n = recv(comm_socket,UserNode->apps_buffer,5000,MSG_WAITALL); //! TODO: apps_buffer should be dynamically expanded
      if(n < 0)
        { 
        perror("get_remoteuser_apps: ERROR reading from socket");
        ret = 1;
        }
      UserNode->apps_buffer[n] = '\0';  
      }

    close(comm_socket);
    return ret;
}

/** Establishes a connection to the client specified by ip.
 *
 *  called by send_launchapp_req(), launchdlg_approved(), check_client_status_thread(), process_useronline_msg()
 *            send_launchapp_req().
 *
 *  returns the value returned by the call to connect()
 */
int connect_to_client(uint32_t ip,int *comm_socket)
{
    struct sockaddr_in s_in;
    int ret;

    *comm_socket = socket(AF_INET,SOCK_STREAM, 0);

    memset(&s_in,0,sizeof(s_in));
    s_in.sin_family = AF_INET;
    s_in.sin_port = htons(CLIENT_COMM_PORT);
    s_in.sin_addr.s_addr = ip; //should already be sent in network byte order by our olsr plugin

    ret = connect(*comm_socket,(struct sockaddr *) &s_in,sizeof(s_in));
    return(ret);
}
		

/** Frees memory allocated for the User (struct _user) linked list. 
 *
 *  First frees memory allocated for the apps_buffer member of struct _user and then frees the
 *  memory for the linked list element. It follows the pointers backwards freeing each linked list.
 *
 *  called by main() 
 */ 
/*
void freeusermem(void)
{
    User *temp = gLastUserNode;
    User *prev = NULL;

    while(temp != NULL)
         {
         prev = temp->prev;
         free(temp->apps_buffer);
         free(temp);
         temp = prev;
         }		 		 		 
}
*/

/** Frees memory allocated for the LaunchAppQueue (struct _LaunchAppQueue) linked list.
 *
 *  Walks backwards from the last element added to the linked list - referenced by
 *  gLastLaunchAppQueue. Frees memory for each linked list till there are none.
 *
 *  called by main()
 */
/*
void freeLaunchAppQueue(void)
{
    LaunchAppQueue* temp;
    LaunchAppQueue* prev;
	
    temp = gLastLaunchAppQueue;
    while(temp != NULL)
         {
         prev = temp->prev;
         free(temp);
         temp = prev;
         }
}
*/

/** Frees memory allocated for the LaunchDialogQueue (struct _LaunchDialogQueue) linked list.
 *
 *  Walks backwards from the last element added to the linked list - referenced by
 *  gLastLaunchDialog. Frees memory for each linked list till there are none.
 * 
 *  called by main()
 */
void freeLaunchDialogQueue(void)
{
    LaunchDialogQueue* temp;
    LaunchDialogQueue* prev;
	
    temp = gLastLaunchDialog;
    while(temp != NULL)
         {
         prev = temp->prev;
         free(temp);
         temp = prev;
         }
}

/** Retrieves details of the network interfaces on the local system.
 *
 *  Calls getifaddrs() to create a linked list (ifaddr) of the network interfaces on the local system. 
 *  avahi_resolver_found() later checks the ifaddr function and compares if the host discovered is 
 *  simply the avahi daemon on the local system or a new unique host on the network.
 *
 *  called by main()
 */
int getlocaladdrs()
{

    if(getifaddrs(&ifaddr) == -1) 
      {
      perror("getifaddrs");
      return 0;
      }

    return 1;    
}


/** Reads the ebp.conf file and retrieves config parameters specified.
 *
 *  returns 0 on error else 1.
 */ 
int readconfigfile(void)
{
    GKeyFile     *key_file;
    GError       *error;
    const char *configfile = "./ebp.conf";  //! TODO: for now looking for the config file in the same dir as the exec; should be /etc
 
    key_file = g_key_file_new (); 
    error = NULL;
    
    config_entry_username = NULL;
    config_container_ip = NULL;
    
    if(!g_key_file_load_from_file (key_file,  configfile, 0, &error))
      {
      fprintf (stderr,"\nFailed to load config file: %s\n",  error->message);
      g_error_free (error);
      return 0;
      }

    error = NULL;
    config_entry_username = g_key_file_get_value (key_file, "General", "Username", NULL);
    config_container_ssh_user = g_key_file_get_value (key_file, "Lxc Container", "container_ssh_user", NULL);
    config_container_ip = g_key_file_get_value (key_file, "Lxc Container", "container_ip", NULL);
    config_container_sshd_port = g_key_file_get_value (key_file, "Lxc Container", "container_sshd_port", NULL);
    config_host_forwarded_port = g_key_file_get_value (key_file, "Lxc Container", "host_forwarded_port", NULL);
    config_connectivity_protocol = g_key_file_get_value (key_file, "Connectivity", "connect_protocol", NULL);    

    if(   config_entry_username == NULL 
       || config_container_ip == NULL 
       || config_container_sshd_port == NULL 
       || config_host_forwarded_port == NULL
       || config_connectivity_protocol == NULL)
      {
      fprintf(stderr,"\nHmm. What are you trying to pull on me?\nError reading config file.\n");
      return 0;
      }

    return 1;
}



/** Pipes STDIN,STDOUT and STDERR and launches program.
 *
 *  Currently used to pipe and call either the OpenSSH server (sshd) or client (ssh).
 *
 *  called by start_server() and process_launchreq_accepted()
 */
void pipe_to_program(char *path, char **args, int *in, int *out, int *err)
{
	int c_in = 0, c_out = 0, c_err = 0;
	int pin[2], pout[2], perr[2];

	if ((pipe(pin) == -1) || (pipe(pout) == -1) || (pipe(perr) == -1))
		//fatal("pipe: %s", strerror(errno));
	   {
	   fprintf(stderr,"fork: %s\n", strerror(errno));
	   _exit(1);
	   }
	*in = pin[0];
	*out = pout[1];
	*err = perr[0];
	c_in = pout[0];
	c_out = pin[1];
	c_err = perr[1];

	if ((childpid = fork()) == -1)
		//fatal("fork: %s", strerror(errno));
           {
	   fprintf(stderr,"fork: %s\n", strerror(errno));
	   _exit(1);
           }

	else if (childpid == 0) {
		if ((dup2(c_in, STDIN_FILENO) == -1) ||
		    (dup2(c_out, STDOUT_FILENO) == -1) ||
		    (dup2(c_err, STDERR_FILENO) == -1)) {
			fprintf(stderr, "dup2: %s\n", strerror(errno));
			_exit(1);
		}
		close(*in);
		close(*out);
		close(*err);
		close(c_in);
		close(c_out);
		close(c_err);

		/*
		 * The underlying ssh is in the same process group, so we must
		 * ignore SIGINT if we want to gracefully abort commands,
		 * otherwise the signal will make it to the ssh process and
		 * kill it too.  Contrawise, since sftp sends SIGTERMs to the
		 * underlying ssh, it must *not* ignore that signal.
		 */
		signal(SIGINT, SIG_IGN);
		signal(SIGTERM, SIG_DFL);
		execvp(path, args);
		fprintf(stderr, "exec: %s: %s\n", path, strerror(errno));
		_exit(1);
	}

	signal(SIGTERM, killchild);
	signal(SIGINT, killchild);
	signal(SIGHUP, killchild);
	close(c_in);
	close(c_out);
	close(c_err);
}


/** Signal handler called in response to SIGTERM,SIGINT and SIGHUP signals.
 *
 *  The signal handler is setup in pipe_to_program()
 */ 
void killchild(int signo)
{
	if (childpid > 1) {
		kill(childpid, SIGTERM);
		waitpid(childpid, NULL, 0);
	}

	_exit(1);
}

