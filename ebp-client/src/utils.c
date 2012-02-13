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

User* add_user(const char* version,const char* name,uint32_t ip)
{
    User* temp = NULL;
    User* current_node = NULL;

    char *strip = NULL;
    struct in_addr in;
    in.s_addr = ip;  //jeetu - this is wrong. ip should actually be the ip of the client sending request
    strip = inet_ntoa(in);	
 
    temp = gFirstUserNode;
    while(temp != NULL)
         {
         if(temp->ip == ip)
           return NULL;
         temp = temp->next;
         }

    // first entry		
    if(gFirstUserNode == NULL)
      {
      // memory freed either during del_user or at the end in freeusermem
      current_node = (User *) malloc(sizeof(User));
      current_node->prev = NULL;
      current_node->next = NULL;
      gFirstUserNode = current_node;
      gLastUserNode = current_node;
      }
    else
      {	
      // memory freed either during del_user or at the end in freeusermem
      current_node = gLastUserNode;
      temp = current_node;
      current_node->next = (User *) malloc(sizeof(User));
      current_node = current_node->next;
      current_node->prev = temp;
      current_node->next = NULL;
      gLastUserNode = current_node;
      }

    strncpy(current_node->name,name,20); //jeetu - hardcoded size
    strncpy(current_node->version,version,6);
    current_node->ip = ip;

    printf("\nadd_user: current_node->name = %s current_node->ip = %d strip = %s\n",current_node->name, current_node->ip,strip);
	
    return current_node;
}



User* del_user(User* deluser)
{
    User* prevuser;
    User* nextuser;
    prevuser = deluser->prev;
    nextuser = deluser->next;
    if(prevuser == NULL && nextuser == NULL)
      {
      gFirstUserNode = NULL;
      gLastUserNode = NULL;
      gCurrentUserNode = NULL;
      free(deluser);
      return(NULL);
      }
    if(prevuser == NULL)
      {
      nextuser->prev = NULL;
      gFirstUserNode = nextuser;
      free(deluser);
      return(nextuser);
      }
    if(nextuser == NULL)
      {
      prevuser->next = NULL;
      gLastUserNode = prevuser;
      free(deluser);
      return(prevuser);
      }  
    if(prevuser != NULL && nextuser != NULL)
      {
      prevuser->next = nextuser;	
      nextuser->prev = prevuser;
      free(deluser);	
      return(nextuser);  
      }  

    return(NULL);
}



NewConnData *add_newconn(int newsockfd,uint32_t ip)
{
    NewConnData* temp = NULL;
    NewConnData* current_node = NULL;

    // first entry		
    if(gFirstConn == NULL)
      {
      // memory freed in newconnrequests_thread with call to del_newconn
      current_node = (NewConnData *) malloc(sizeof(NewConnData));
      current_node->prev = NULL;
      current_node->next = NULL;
      gFirstConn = current_node;
      gLastConn = current_node;
      }
    else
      {	
      // memory freed in newconnrequests_thread with call to del_newconn
      current_node = gLastConn;
      temp = current_node;
      current_node->next = (NewConnData *) malloc(sizeof(NewConnData));
      current_node = current_node->next;
      current_node->prev = temp;
      current_node->next = NULL;
      gLastConn = current_node;
      }

    current_node->newsockfd = newsockfd;
    current_node->ip = ip;
	
    return current_node;

}



NewConnData *del_newconn(NewConnData *conndata)
{
    NewConnData* prevconn;
    NewConnData* nextconn;
    prevconn = conndata->prev;
    nextconn = conndata->next;
    if(prevconn == NULL && nextconn == NULL)
      {
      gFirstConn = NULL;
      gLastConn = NULL;
      gCurrentConn = NULL;
      free(conndata);
      return(NULL);
      }
    if(prevconn == NULL)
      {
      nextconn->prev = NULL;
      gFirstConn = nextconn;
      free(conndata);
      return(nextconn);
      }
    if(nextconn == NULL)
      {
      prevconn->next = NULL;
      gLastConn = prevconn;
      free(conndata);
      return(prevconn);
      }  
    if(prevconn != NULL && nextconn != NULL)
      {
      prevconn->next = nextconn;	
      nextconn->prev = prevconn;
      free(conndata);	
      return(nextconn);  
      }  

    return(NULL);

}


LaunchDialogQueue *add_launchdialog_queue(char *username,char *appname,uint32_t ip)
{
   LaunchDialogQueue* temp = NULL;
   LaunchDialogQueue* current_node = NULL;

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

   strncpy(current_node->username,username,90);
   strncpy(current_node->appname,appname,90);
   current_node->ip = ip;
	
   return current_node;

}


LaunchAppQueue* add_to_launch_queue(char *appname,uint32_t ip,int reqid)
{
    LaunchAppQueue* temp;
    LaunchAppQueue* current_node;

    printf("\nadd_to_launch_queue: ip = %d",ip);

    // on_treeview_row_activated and thus this function should be called
    // only by the main thread as I understand it, therefore not protecting
    // with a mutex.

    // first entry		
    if(gFirstLaunchAppQueue == NULL)
      {
      // memory allocation freed in freeLaunchAppQueue ()
      current_node = (LaunchAppQueue *) malloc(sizeof(LaunchAppQueue));
      current_node->prev = NULL;
      current_node->next = NULL;
      gFirstLaunchAppQueue = current_node;
      gLastLaunchAppQueue = current_node;
      }
    else
      {		  
      // memory allocation freed in freeLaunchAppQueue ()
      current_node = gLastLaunchAppQueue;
      temp = current_node;
      current_node->next = (LaunchAppQueue *) malloc(sizeof(LaunchAppQueue));
      current_node = current_node->next;
      current_node->prev = temp;
      current_node->next = NULL;
      gLastLaunchAppQueue = current_node;
      }
	
    strncpy(current_node->appname,appname,20); //jeetu - hardcoded size  
    current_node->ip = ip;
    current_node->reqid = reqid;

    return current_node;
}


char* get_installed_apps(int* count,int* blocksize)
{	
    int n = 0;
    int i = 0;
    struct dirent **namelist = NULL;
    int size = 0;
    char *buf;
    char file[256];
    file[0] = '\0';
    
    gchar *retval;
    GKeyFile *key_file;
    key_file = g_key_file_new (); 
    GError   *error;
    
    // memory should get freed in main() with free(appsdata.apps)
    buf = malloc(1);
    buf[0] = '\0';
	
    n = scandir("/usr/share/applications", &namelist, filter, alphasort);
    if(n < 0)
      perror("list_installed_apps: error scandir");
    else 
      {
      while(i < n)
        {
        size = size + strlen(namelist[i]->d_name) + 8;
        buf = realloc(buf,size);
    
        error = NULL;
        strcpy(file,"/usr/share/applications/");
        strncat(file, namelist[i]->d_name, strlen(namelist[i]->d_name)); 
        if(!g_key_file_load_from_file (key_file,  file, 0, &error))
          {
          printf("Failed to load \"%s\": %s\n",  namelist[i]->d_name, error->message);
          }
        else
          {
          retval =  g_key_file_get_locale_string(key_file, "Desktop Entry", "Exec", NULL, &error);
          // we are not handling arguments, split at first whitespace
          // TODO: policy for handling args, across remote
          // code to implement the same, separate *.desktop & *-file.desktop
          retval = strtok(retval, " ");
          if(retval!= NULL) 
            {
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



int filter(const struct dirent *dir)
{
    if(strstr(dir->d_name,".desktop") != NULL)  
      return 1;
    else
      return 0;
}



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
      n = recv(comm_socket,UserNode->apps_buffer,5000,MSG_WAITALL); //jeetu - apps_buffer should be dynamically expanded
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


int getlocaladdrs()
{
//    struct ifaddrs *ifa;
//    int family, s;
//    char host[NI_MAXHOST];

    if(getifaddrs(&ifaddr) == -1) 
      {
      perror("getifaddrs");
      return 0;
      }

//for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
//               if (ifa->ifa_addr == NULL)
//                   continue;
//
//               family = ifa->ifa_addr->sa_family;
//
//               /* Display interface name and family (including symbolic
//                  form of the latter for the common families) */
//
//               printf("%s  address family: %d%s\n",
//                       ifa->ifa_name, family,
//                       (family == AF_PACKET) ? " (AF_PACKET)" :
//                       (family == AF_INET) ?   " (AF_INET)" :
//                       (family == AF_INET6) ?  " (AF_INET6)" : "");
///* For an AF_INET* interface address, display the address */
//
//               if (family == AF_INET || family == AF_INET6) {
//                   s = getnameinfo(ifa->ifa_addr,
//                           (family == AF_INET) ? sizeof(struct sockaddr_in) :
//                                                 sizeof(struct sockaddr_in6),
//                           host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
//                   if (s != 0) {
//                       printf("getnameinfo() failed: %s\n", gai_strerror(s));
//                       exit(EXIT_FAILURE);
//                   }
//                   printf("\taddress: <%s>\n", host);
//               }
//           }


    return 1;    
}



int readconfigfile()
{
    GKeyFile     *key_file;
    GError       *error;
    const char *configfile = "./ebp.conf";  //jeetu - for now looking for the config file in the same dir as the exec; should be /etc
 
    key_file = g_key_file_new (); 
    error = NULL;
    
    if(!g_key_file_load_from_file (key_file,  configfile, 0, &error))
      {
      fprintf (stderr,"\nFailed to load config file: %s\n",  error->message);
      g_error_free (error);
      return 0;
      }

    error = NULL;
    config_entry_username = g_key_file_get_value (key_file, "General", "Username", &error);
    if(config_entry_username == NULL)
      {
      fprintf(stderr,"\nHmm. What are you trying to pull on me?\nError reading config file. %s.\n",error->message);
      g_error_free(error);
      return 0;
      }

    return 1;
}



