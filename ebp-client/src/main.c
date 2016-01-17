/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) Jatin Golani 2011 <ebrain@ebrain.in>
 * 
 * ebp is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * ebp is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <glib/gi18n.h>

#include "ebp.h"
#include "avahi-discovery.h"

G_LOCK_DEFINE(treeview);
G_LOCK_DEFINE(treestore);
G_LOCK_DEFINE(user);
G_LOCK_DEFINE(launchappqueue);
G_LOCK_DEFINE(launchdialogqueue);
G_LOCK_DEFINE(newconndata);

/** entry point for the eBrainPool client.
 *
 *  Sets up and initializes the main UI elements. 
 */
int main(int argc, char *argv[])
{ 
    GtkWidget *logo = NULL;
    GtkWidget *label = NULL;
    GtkWidget *vbox = NULL;
    GtkWidget *hbox = NULL;
    GtkWidget *scrolledwindow = NULL;
    char buf[302];
    GdkRGBA rgba = {1.0,1.0,1.0,1.0};
    gUserListHead = NULL;    

    g_thread_init (NULL);
    gdk_threads_init();
    gtk_init (&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_size_request(window,WINDOW_WIDTH,WINDOW_HEIGHT);
    gtk_window_set_title(GTK_WINDOW(window),"eBrainPool");
    gtk_widget_override_background_color(window,GTK_STATE_NORMAL, &rgba);
    g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

    vbox = gtk_vbox_new(FALSE,2);
    hbox = gtk_hbox_new(FALSE,2);
    
    logo = gtk_image_new_from_file("./ebrain_logo_1.png");
    gtk_box_pack_start(GTK_BOX(hbox),logo,FALSE,FALSE,1);

    snprintf(buf,300,"eBrainPool allows you to run\nsoftware from other\nnearby eBrainers shown\nin the list below.");
    label = gtk_label_new(buf);
    gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,TRUE,1);

    gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,TRUE,1);

    treeview = gtk_tree_view_new();
    treestore = gtk_tree_store_new(NUM_COLS,G_TYPE_STRING,G_TYPE_POINTER);
    init_treeview(treeview,treestore);
      
    scrolledwindow = gtk_scrolled_window_new(NULL,NULL);
    gtk_container_add(GTK_CONTAINER(scrolledwindow),treeview);
    gtk_box_pack_start(GTK_BOX(vbox),scrolledwindow,TRUE,TRUE,1);

    gtk_container_add(GTK_CONTAINER(window), vbox);    

    gtk_widget_show_all(window);

    //! get the username running this process,this is sent to remote users for ssh connects back to this user.
    ssh_login_userdetails = getpwuid(getuid());
    printf("\nssh_login_userdetails->pw_name = %s\n",ssh_login_userdetails->pw_name);

    //! Reads in values from the config file
    if(readconfigfile() == 0)
      return 0;   

    //! Launches the lxc isolation sandbox container.
    launch_container();
    sleep(5);  //! TODO: temporary fix to wait for sandbox container to come up before proceeding to establish a SSH connection to this container.

    //! allocate a memory block and retrieve list of apps installed on the system 
    appsdata.apps = get_installed_apps(&appsdata.count,&appsdata.blocksize); //jeetu - can pass pointer to Appsdata instead

    //! get addresses of interfaces on the local system (localhost)
    getlocaladdrs(&ifaddr);

    avahi_setup();

    //! Spawns a thread to setup ssh port forwarding between host and container.
    //! Therefore ssh client on host connects to server in container and sets up 
    //! a port on host. Any remote ebp client connects to this port via ssh and
    //! is forwarded to the sshd in the container. Application requests will then
    //! be X forwarded from within the container.

    //! TODO: Ideally a port should only be created if the user allows a user to access
    //! the machine. With a perpetual port created as is done here there is a chance
    //! a user with the right keys can bypass the ebp client and still have apps X forwarded.
    g_thread_create(create_port_to_container_thread,NULL,FALSE,NULL);
   
    //! spawns message thread that ultimately (thru listener_child) processes:
    //! - olsr plugin notification for a user coming online
    //! - GetApps request to send list of applications installed on current host
    //! - LaunchApp request by a remote host to launch a new app
    //! - LaunchAppReqAccepted message showing that remote host has accepted launch request
    g_thread_create(connlistener_thread, NULL, FALSE, NULL);    

    //! spawns thread to constantly check whether a user is still reachable i.e. online
    g_thread_create(check_client_status_thread, NULL, FALSE, NULL);

    gdk_threads_enter();
    gtk_main();
    gdk_threads_leave();

    stop_container();   
    close(sockfd);
    free(appsdata.apps); 
//    freeusermem();
    freeLaunchAppQueue();
    freeLaunchDialogQueue();
    return 0;
}

/** Initialize GTK treeview control to display users and their apps.
 *
 */
int init_treeview(GtkWidget *treeview,GtkTreeStore *treestore)
{
    GtkTreeViewColumn *col = NULL;
    GtkCellRenderer *renderer = NULL;
//    GdkRGBA rgba = {1.0,1.0,1.0,1.0};

//    gtk_widget_override_background_color(treeview,GTK_STATE_NORMAL, &rgba);

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "eBrained Applications");
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer, 
      "text", NAME);

    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(treestore));
    g_object_unref(treestore); 

    //! Connects the "row-activate" treeview signal to call on_treeview_row_activate().
    g_signal_connect(GTK_TREE_VIEW(treeview),"row-activated",G_CALLBACK(on_treeview_row_activated),NULL);

    return 0;
}

/** main message processing thread spawned by main()
 *
 *  This thread:
 *  - endlessly listens in on a socket
 *  - accepts a new connection
 *  - spawns a new thread to process the new connection
 */
gpointer connlistener_thread(gpointer user_data)
{
    int newsockfd=0, portno=0;
    socklen_t clilen;    
    struct sockaddr_in serv_addr, cli_addr;
    fd_set rfds;
    fd_set afds;
    int nfds = 0;
    int reuse=0;
    int i = 0;    
    NewConnData *newconndata = NULL;
      
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) 
       perror("listener: ERROR opening socket");
       
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = CLIENT_COMM_PORT;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    /* Enable address reuse */
    reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
      perror("connlistener_thread: ERROR on binding");      
    
    listen(sockfd,5); 
    
    clilen = sizeof(cli_addr);    
    nfds = getdtablesize();
    while(1) 
         {
         newsockfd = 0;
         FD_ZERO(&afds);
         FD_SET(sockfd, &afds);
	     memcpy(&rfds, &afds, sizeof(rfds));
         if(select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0)
            perror("connlistener_thread: ERROR on select");

         if(FD_ISSET(sockfd, &rfds))
           {
           newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);            
           if(newsockfd < 0) 
      	     perror("connlistener_thread: ERROR on accept");
      	   
           newconndata = add_newconn(newsockfd,cli_addr.sin_addr.s_addr);
           //! the new thread is spawned to read data from a new connection and process it
           g_thread_create(newconnrequests_thread, newconndata, FALSE, NULL);
           }
         i++;
         }    	 
    return NULL;  	 
}

//TODO: Make sure that all of threads created are killed at program exit


/** thread launched by connlistener_thread() to process notification / requests.
 *
 *  This thread processes:
 *  - olsr plugin notification for a user coming online (currently disabled)
 *  - GetApps request to send list of applications installed on current host
 *  - LaunchApp request by a remote host to launch a new app
 *  - LaunchAppReqAccepted message showing that remote host has accepted launch request
 */
gpointer newconnrequests_thread(gpointer user_data)
{
    char buffer[260];
    int n = 0;
    NewConnData *data = NULL;
    char str[260];
    char *token = NULL;
    char *saveptr1 = NULL;
	
    data = (NewConnData *) user_data;	

    bzero(str,260);	
    bzero(buffer,260);
    n = read(data->newsockfd,buffer,256);
    if(n < 0)
      { 
      perror("listener_child: ERROR reading from socket");
      close(data->newsockfd);
      return NULL;
      }

    //! processes eBrainOnline olsr plugin notification when a user comes online (currently disabled)   
    if(buffer[0] != 0)
      {     
      strncpy(str,buffer,256);
      token = strtok_r(str,":",&saveptr1);
/*      if(strcmp(token,"eBrainOnline") == 0)
        {
        process_useronline_msg(buffer);
        }
*/
      //! GetApps request sends a list of applications installed on this host  
      if(strcmp(token,"GetApps") == 0)
        {
	    //jeetu - for now sending the complete block of application name
	    //without letting the other end know the actual datasize of data
	    //to expect. 
        n = send(data->newsockfd,appsdata.apps,appsdata.blocksize,MSG_NOSIGNAL);
        if(n < 0) 
          perror("newconnrequests_thread: ERROR writing to socket");
        }
      
      //! LaunchApp request processed when a remote user wishes to launch an application from this host.   
      if(strcmp(token,"LaunchApp") == 0)
        {
        process_launchapp_req(buffer,data);
        }
        
      //! LaunchAppReqAccepted request processed when the remote user has OKed our app launch request. 
      if(strcmp(token,"LaunchAppReqAccepted") == 0)
        {
        strncpy(data->buffer,buffer,strlen(buffer)+1);
        process_launchreq_accepted(data);
        }           
      }
    
    close(data->newsockfd);
    G_LOCK(newconndata);
    del_newconn(data);
    G_UNLOCK(newconndata);

    return NULL;
}

/** thread spawned by main() to constantly check if a user can be reached and should be shown as Online.
 *
 */
gpointer check_client_status_thread(gpointer user_data)
{
    User* temp;
    int ret;
    int comm_socket;

    while(1)
         {
         g_usleep(10000);
         if(gUserListHead != NULL)
           {
           temp = gUserListHead;
           while(temp != NULL)
                {
                //! tries to connect to the remote user
                ret = connect_to_client(temp->ip,&comm_socket);
                if(ret == -1)
                  {
                  //! If it can't connect,the client is probably no longer
                  //!   alive; delete it from the list
                  g_idle_add(show_user_offline,temp);
                  close(comm_socket);
                  break;
                  }
                else
                  temp = temp->next;
                close(comm_socket);
                }
           }
         }

    return NULL;
}


/** processes the eBrainOnline olsr plugin notification. (currently disabled)
 *
 *  Called via newconnrequests_thread() to process the eBrainOnline plugin notification,
 *  and show a user online.
 */
/*
int process_useronline_msg(char *buf)
{
    int j=0;
    char *str1=NULL,*token = NULL,*str2 = NULL,*subtoken = NULL;
    char *saveptr1 = NULL,*saveptr2 = NULL;
    char username[20];
    char version[6];
    uint32_t ip = 0;
    int ret = 0;
    int comm_socket = 0;

    username[0] = '\0';
    version[0] = '\0';

    printf("\nprocess_useronline_msg: buf = %s\n",buf);
    
    // jeetu - explore possibility to replace this with sscanf        
    for(j = 1, str1 = buf; ; j++, str1 = NULL) 
       {
       token = strtok_r(str1, ":", &saveptr1);
       if(token == NULL)
         break;

       for(str2 = token; ; str2 = NULL) 
          {
          subtoken = strtok_r(str2, "==", &saveptr2);          
          if(subtoken == NULL)
            break;                    
          if(strcmp(subtoken,"Username") == 0)
            {
            str2 = NULL;
            subtoken = strtok_r(str2, "==", &saveptr2);
            if(subtoken == NULL)
              break;  
            strncpy(username,subtoken,20);
            }           
          if(strcmp(subtoken,"Version") == 0)
            {
            str2 = NULL;
            subtoken = strtok_r(str2, "==", &saveptr2);
            if(subtoken == NULL)
              break;  
            strncpy(version,subtoken,6);
            }           
          if(strcmp(subtoken,"IP") == 0)
            {
            str2 = NULL;
            subtoken = strtok_r(str2, "==", &saveptr2);
            if(subtoken == NULL)
              break;  
            ip = atoi(subtoken);
            }            
          }
       }

    //! Also checks to see if remote client is reachable.
    //! This takes care of a bug wherein user keeps flickering
    //! in the local client when remote client is unreachable but remote olsr is up. 	  
    ret = connect_to_client(ip,&comm_socket);
    if(ret != -1)
      {
      G_LOCK(user);
      gCurrentUserNode = add_user(version,username,"",ip);
      G_UNLOCK(user);
      if(gCurrentUserNode != NULL)
        {
        show_user_online(gCurrentUserNode);
        }
      }

    return 0;
}
*/

/** Shows a new client discovered by Avahi as online.
 *
 *  called by avahi_resolver_found() when it discovers a new client.
 */
int process_useronline_avahi_msg(const char *ip_str, const char *username, const char *ssh_login_user,const char *version)
{
    int ret = 0;
    int comm_socket = 0;
    uint32_t ip = 0;
    struct in_addr in;
    
    inet_aton(ip_str,&in);
    printf("\nprocess_useronline_avahi_msg: in.s_addr = %d\n",in.s_addr);

    //! checks to see if remote client is reachable and if so shows it as Online.
    ret = connect_to_client(ip,&comm_socket);
    if(ret != -1)
      {
      G_LOCK(user);
      gUserListHead = add_user(gUserListHead,version,username,ssh_login_user,in.s_addr);
      G_UNLOCK(user);
      if(gUserListHead != NULL)
        show_user_online(gUserListHead);
      }

    return 0;
}


/** Displays a new user and the applications shared.
 *
 *  called by process_useronline_avahi_msg() and process_useronline_msg(). 
 */
int show_user_online(User *UserNode)
{
    GtkTreeIter toplevel, child;
    int j = 0;
    char *str1 = NULL,*token = NULL;
    int ret = 0;
    char *saveptr1 = NULL;    
    
//    G_LOCK(treeview);
//    gtk_widget_override_background_color(treeview,GTK_STATE_NORMAL, NULL);
//    G_UNLOCK(treeview);

    G_LOCK(treestore);
    gtk_tree_store_append(treestore, &toplevel, NULL);
    gtk_tree_store_set(treestore, &toplevel,NAME,UserNode->name,-1);
    gtk_tree_store_set(treestore,&toplevel,USERNODE,UserNode,-1);
    G_UNLOCK(treestore);

    // no need for a mutex here since Usernode should point to a different memory block
    // for each thread anyway, no shared memory region to protect.
    UserNode->apps_buffer = malloc(5010); //!< TODO: memory to be freed; should be dynamically expanded block
    //! retrieves and shows applications shared by the user in the treeview.
    ret = get_remoteuser_apps(UserNode);
    if(ret == 0)
      {
      for(j = 1, str1 = UserNode->apps_buffer; ; j++, str1 = NULL) 
         {
         token = strtok_r(str1, ":", &saveptr1);
         if(token == NULL)
           break;
         G_LOCK(treestore);
         gtk_tree_store_append(treestore, &child, &toplevel);
         gtk_tree_store_set(treestore, &child,NAME,token,-1);
         gtk_tree_store_set(treestore,&child,USERNODE,UserNode,-1);
         G_UNLOCK(treestore);
         }
      }

    return 0;
}

/** Shows user as offline by deleting from the treeview.
 *
 *  called by check_client_status_thread().
 *  Deletes an unreachable client (and consequently its applications) from the treeview and the User linked list.
 */
gboolean show_user_offline(gpointer user_data)
{
    GtkTreeIter iter;
    User *temp = NULL;
    gboolean valid;
    User *UserNode = (User *) user_data;

    valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(treestore),&iter);
    while(valid)
         {
         gtk_tree_model_get(GTK_TREE_MODEL(treestore),&iter,USERNODE,&temp,-1);
         // if the ip of user at this node matches the ip of the user who
         // can no longer be contacted, remove him from the tree model
         printf("\nshow_user_offline: temp->name = %s temp->ip = %d UserNode->ip = %d\n",temp->name,temp->ip,UserNode->ip);
         if(temp->ip == UserNode->ip)
           {
           G_LOCK(treestore);
           gtk_tree_store_remove(treestore,&iter);
           G_UNLOCK(treestore);
           G_LOCK(user);
           temp = del_user(&gUserListHead,UserNode); /* temp == new current node */
           G_UNLOCK(user);
           break;
           }
         valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(treestore),&iter);
         }

    return FALSE;
}

/** Process the "row-activated" GTK treeview signal.
 *
 *  Signal handler setup in init_treeview().
 *
 *  Called when a user clicks on an application to launch it from the remote host.
 */
void on_treeview_row_activated(GtkWidget *widget,GtkTreePath *path,GtkTreeViewColumn *column,gpointer user_data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeSelection *selection = NULL;
  char *name;
  User *user;

  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

  if(gtk_tree_selection_get_selected(selection, &model, &iter)) 
    {
    gtk_tree_model_get(model, &iter, NAME, &name,  -1);
    gtk_tree_model_get(model, &iter, USERNODE, &user,  -1);
    //! calls send_launchapp_req() to send request to remote user to launch application.
    send_launchapp_req(user,name);
    }
}

/** Sends request to remote user to launch application.
 *
 *  Called by on_treeview_row_activated()
 */
void send_launchapp_req(User *UserNode,char *appname)
{
    char *ip = NULL;
    struct in_addr in;
    int comm_socket = 0;
    int ret = 0;
    char buf[300];
    int n = 0;

    in.s_addr = UserNode->ip;  //jeetu - this is wrong. ip should actually be the ip of the client sending request
    ip = inet_ntoa(in);	
    buf[0] = '\0';
    printf("\nIn send_launchapp_req: (uint32_t)UserNode->ip = %d ip = %s, appname = %s\n",UserNode->ip,ip,appname);

    //! Adds the request sent to a launch queue. To be used on receipt of response to verify authenticity of the request.
    G_LOCK(launchappqueue);	
    gCurrentLaunchAppQueue = add_to_launch_queue(appname,UserNode->ip,requestid);
    G_UNLOCK(launchappqueue);

    requestid++;    
	
    //! Connects to remote host.
    ret = connect_to_client(UserNode->ip,&comm_socket);
    if(ret == 0)
      {
      //! Sends LaunchApp request along with application name to remote client.
      snprintf(buf,285,"LaunchApp:%s",appname); //jeetu - hardcoded size
      n = write(comm_socket,buf,strlen(buf));
      if(n < 0) 
        perror("send_launchapp_req: ERROR writing to socket");       
      close(comm_socket);  
      }	
}

/** Processes a received LaunchApp request from a remote client.
 *
 *  The LaunchApp request suggests that a client wishes to run an application from the current host.
 *  The format of this request is LaunchApp:Appname where Appname is the application name.
 */
int process_launchapp_req(char *buf,NewConnData *data)
{
    char appname[257];
    int j;
    char *str1 = NULL,*token = NULL;
    char *saveptr1 = NULL;
    User *temp;
    char username[20];
    LaunchDialogQueue *launchdialogqueue = NULL;
    char *strdataip = NULL, *strtempip = NULL;
    struct in_addr in,in2;
	
    appname[0] = '\0';
    username[0] = '\0';

    in.s_addr = data->ip;
    strdataip = inet_ntoa(in);	

    for(j = 1, str1 = buf; ; j++, str1 = NULL) 
       {
       token = strtok_r(str1, ":", &saveptr1);
       if(token == NULL)
         break;         
       if(j == 2) // second argument
         strncpy(appname,token,256);
       }	

    //! Search the UserNode user's list and find username from the IP
    temp = gUserListHead;	
    while(temp != NULL)
         {
         in2.s_addr = temp->ip;
         strtempip = inet_ntoa(in2);
         if(strcmp(strtempip,strdataip) == 0)
           {
           strncpy(username,temp->name,19);
           break;
           }
         temp = temp->next;
         }

    // From what I understand (can't remember exactly why I did this ;) ), however apparently GTK+
    // elements such as dialogs should be handled in the same thread as the initializing thread,
    // usually in main(). Therefore data from other threads needs to be passed back to this main 
    // thread wherein the dialog is presented to the user. This cross-thread data exchange is
    // facilated by the launchdialogqueue structure.

    launchdialogqueue = add_launchdialog_queue(username,appname,data->ip);

    //! Presents dialog box to user seeking approval.
    g_idle_add(launch_approve_dialog,launchdialogqueue);
		
    return 0;
}


/** Shows dialog box seeking approval for request from a user to launch application
 *
 *  Called by process_launchapp_req()
 */
gboolean launch_approve_dialog(gpointer data)
{
    GtkWidget *dialog;
    LaunchDialogQueue *launchdialogqueue = (LaunchDialogQueue *) data;

    dialog = gtk_message_dialog_new(GTK_WINDOW(window),GTK_DIALOG_DESTROY_WITH_PARENT,GTK_MESSAGE_QUESTION,GTK_BUTTONS_YES_NO,
                               "User %s wishes to run the application %s off your system.\nAllow?",launchdialogqueue->username,launchdialogqueue->appname);
    //! Connects the "response" signal of the dialog box to launch_approve_dialog_response().
    g_signal_connect(dialog,"response",G_CALLBACK(launch_approve_dialog_response),launchdialogqueue);
    gtk_widget_show_all(dialog);

    return FALSE;
}


/** Processes response (Yes/No) from dialog box seeking approval to launch application.
 *
 */
void launch_approve_dialog_response(GtkWidget *dialog,gint response_id, gpointer user_data)
{
    LaunchDialogQueue *launchdialogqueue = (LaunchDialogQueue *) user_data;

    switch(response_id)
          {
          case GTK_RESPONSE_YES:
             printf("\nlaunch_approve_dialog_response: YESSSSSSSSSSS!!! launchdialogqueue->appname = %s launchdialogqueue->ip = %d\n",launchdialogqueue->appname,launchdialogqueue->ip);
             launchdlg_approved(launchdialogqueue->appname,launchdialogqueue->ip);
             break;
          case GTK_RESPONSE_NO:
             //! todo - send a rejection signal to the initiating client
             break;
          default:
             break;
          }

    gtk_widget_destroy(dialog);
}



/** Handles the condition when user has approved request to launch an application.
 *
 */
void launchdlg_approved(char *appname,uint32_t ip)
{
    int comm_socket = 0;
    int ret = 0;
    char buf[300];	
    int n = 0;

    //! Spawns a thread to start an instance of the OpenSSH server for this request.
    //! This server instance will X forward the application requested by remote client.
    //g_thread_create(start_server,&ip,FALSE,NULL);
    //g_thread_create(create_port_to_container,&ip,FALSE,NULL);

    //! Connect back to the client that had sent the LaunchApp request and send it
    //! a LaunchAppReqAccepted message along with the application name. This signifies
    //! that our host is willing to honour their request and has launched the SSH server.
    ret = connect_to_client(ip,&comm_socket);
    if(ret == 0)
      {
      snprintf(buf,285,"LaunchAppReqAccepted:%s",appname);
      n = write(comm_socket,buf,strlen(buf));
      if(n < 0) 
        perror("launchdlg_approved: ERROR writing to socket");        
      close(comm_socket);
      }
}


//TODO: Make sure that all of threads created are killed at program exit

/** Thread to start a new instance of the OpenSSH server.
 *
 *  This thread is spawned by launchdlg_approved() 
 */
gpointer start_server(gpointer ptr_ip)
{
    int piped_in = 0, piped_out = 0, piped_err = 0;
    char onechar;
    //! Arguments to the OpenSSH server (see code) are currently hardcoded.Eventually will be taken in from main ebp.conf file.
    char *args[] = { 
                        "/usr/sbin/sshd",
                        "-e","-D","-d",
                        "-o", "Port 20101",
                        "-o", "HostKey ~/.ebp/ssh_host_rsa_key",
                        "-o", "HostKey ~/.ebp/ssh_host_dsa_key",
                        "-o", "UsePrivilegeSeparation yes",
                        "-o", "KeyRegenerationInterval 3600",
                        "-o", "ServerKeyBits 768",
                        "-o", "SyslogFacility AUTH",
                        "-o", "LoginGraceTime 600",
                        "-o", "PermitRootLogin no",
                        "-o", "StrictModes yes",
                        "-o", "RSAAuthentication yes",
                        "-o", "PubkeyAuthentication yes",
                        "-o", "AuthorizedKeysFile ~/.ebp/authorized_keys",
                        "-o", "RhostsRSAAuthentication no",
                        "-o", "HostbasedAuthentication no",
                        "-o", "IgnoreUserKnownHosts yes",
                        "-o", "PermitEmptyPasswords no",
                        "-o", "ChallengeResponseAuthentication no",
                        "-o", "PasswordAuthentication no",
                        "-o", "X11Forwarding yes",
                        "-o", "X11DisplayOffset 10",
                        "-o", "PrintMotd no",
                        "-o", "PrintLastLog yes",
                        "-o", "TCPKeepAlive yes",
                        "-o", "UsePAM no",
                        "-o", "PermitTunnel yes",
                        NULL 
                   };

    //! Pipe STDIN,STDOUT and STDERR and launch /usr/sbin/sshd (currently hardcoded)
    pipe_to_program("/usr/sbin/sshd",args,&piped_in,&piped_out,&piped_err);
    
    //! TODO:
    //! for now reading from the piped stderr of the child one char at a time
    //! eventually need to put in mechanism to determine and deal with 
    //! success/failure of the child program.
    while(read(piped_err,&onechar,1) > 0)
         {
         printf("%c",onechar);
         }

    return NULL;
}



/** Thread to establish local port forwarding to the container.
 *
 *  Uses OpenSSH client to connect to the sshd in the container and establish port forwarding
 *  from host to container. This allows external ebp clients to connect to the host ip and have applications
 *  run within the container without having any networking access directly into the container.
 *
 *  This thread is spawned by launchdlg_approved() 
 */
gpointer create_port_to_container_thread(gpointer user_data)
{
    int piped_in = 0, piped_out = 0, piped_err = 0;
    int j = 0,i = 0;
    char onechar;

    //! Arguments to the OpenSSH client (see code) are currently hardcoded.Eventually will be taken in from main ebp.conf file.
    char *args1[] = {
                   "/usr/bin/ssh",
                   "-v",
                   //"-o", "Host *",
                   "-o", "RSAAuthentication no",
                   "-o", "PasswordAuthentication no",
                   "-o", "IdentityFile ~/.ebp/clientkeys",
                   "-o", "Port 22",
                   "-o", "Ciphers arcfour256,arcfour128",
                   "-o", "StrictHostKeyChecking no",
                   "-o", "UserKnownHostsFile ~/.ebp/known_hosts",
                   "-o", "Compression yes",
                   "-o", "CompressionLevel 7",
                   "-o", "SendEnv LANG LC_*",
                   "-o", "HashKnownHosts no",
                   "-o", "GSSAPIAuthentication no",
                   "-o", "GSSAPIDelegateCredentials no", 
                   "-g","-L",
                   ""              
                   "",
                   "",
                   NULL 
                   };
    char args2[3][300];

    args2[0][0] = '\0';
    args2[1][0] = '\0';
    args2[2][0] = '\0';

    sprintf(args2[0],"%s:%s:%s",config_host_forwarded_port,config_container_ip,config_container_sshd_port);  
    sprintf(args2[1],"%s@%s",config_container_ssh_user,config_container_ip);
    sprintf(args2[2],"/bin/cat");                    //! keep alive this connection without giving a bash prompt.

    // include arguments for user@host to connect to and the appname    
    j = 0; 
    while(args1[j] != NULL)
         {
         // substitute the empty placeholder strings ("") in args1 with the correct values
         if(strcmp(args1[j],"") == 0)
           {
           args1[j] = args2[i];
           i++;
           }
         printf("args1[j] = %s\n",args1[j]);
         j++;
         }

    //! Pipe STDIN,STDOUT and STDERR and launch /usr/sbin/ssh (currently hardcoded)
    pipe_to_program("/usr/bin/ssh",args1,&piped_in,&piped_out,&piped_err);
    
    //! TODO:
    //! for now reading from the piped stderr of the child one char at a time
    //! eventually need to put in mechanism to determine and deal with 
    //! success/failure of the child program.
    while(read(piped_err,&onechar,1) > 0)
         {
         printf("%c",onechar);
         }

    return NULL;
}





/** Processes the LaunchAppReqAccepted message.
 *
 *  The LaunchAppReqAccepted message signifies that the remote user is willing to launch
 *  the application we requested.
 *
 *  Calls the relevant function depending on whether the user has set SSH-X or X2go as their
 *  preferred connectivity protocol.
 */
int process_launchreq_accepted(NewConnData *data)
{
    if(strcmp(config_connectivity_protocol,"ssh-x") == 0)
      launch_using_sshx(data);
    if(strcmp(config_connectivity_protocol,"x2go") == 0)
      launch_using_x2go(data);  

    return 0;
}




/** Launches application using x2go.
 *
 *  Called by process_launchreq_accepted if user's preferred connectivity option is set to z2go
 *
 *  This function uses the x2go python client - pyhoca-cli.
 */
int launch_using_x2go(NewConnData *data)
{
    char ip[21];
    int j = 0,i = 0;
    char *saveptr1 = NULL;
    struct in_addr in;
    LaunchAppQueue *queue;
    char buf[300];
    int piped_in = 0, piped_out = 0, piped_err = 0;
    char onechar;
    User *userlist;
    char ssh_login_user[257];

    char *str1 = NULL,*token = NULL;

	//! Arguments to the x2go pyhocal-cli client are currently hardcoded.Eventually will be 	taken in from main ebp.conf file.
    char *args1[] = {
				    "/usr/bin/pyhoca-cli",
                    "-q","adsl",
                    "-g","800x600",
                    "-t","application",
                    "-u","ebp",
                    "--password","ebp",
                    "-p","20101",
                    "--server",
                    "",
                    "-c",
                    "",
                    NULL 
                    };
    char args2[2][300];
		  
    ip[0] = '\0';
    args2[0][0] = '\0';

    in.s_addr = data->ip;  
    strncpy(ip,inet_ntoa(in),20);

    //! Determines username and ip;formats the [username]@[ip] string to be used by ssh client. 
    userlist = gUserListHead;
    while(userlist != NULL)
         {
         if(userlist->ip == data->ip)
           {          
           printf("\nlaunch_using_x2go: userlist->name = %s userlist->ssh_login_user = %s\n",userlist->name,userlist->ssh_login_user);
           // jeetu - user temporaily hardcoded;eventually the container user the remote ssh
		   // needs to connect to could be passed along as part of the avahi service message TXT
		   // field or in some other way.
           sprintf(args2[0],"%s",ip); 
           break;
           }
         userlist = userlist->next;
         }    

    if(args2[0][0] == '\0')
      {
      printf("\nlaunch_using_x2go: something went wrong args2[0][0] == '\0'\n");
      return 0;
      } 

    strcpy(buf,data->buffer);
    for(j = 1, str1 = buf; ; j++, str1 = NULL)  
       {
       token = strtok_r(str1, ":", &saveptr1);
       if(token == NULL)
         break;        
       printf("%d: %s\n", j, token); 
       if(j == 2) // second argument
         strcpy(args2[1],token);
       }	

    // include arguments for user@host to connect to and the appname    
    j = 0; 
    while(args1[j] != NULL)
         {
         // substitute the empty placeholder strings ("") in args1 with the correct values
         if(strcmp(args1[j],"") == 0)
           {
           args1[j] = args2[i];
           i++;
           }
         printf("args1[j] = %s\n",args1[j]);
         j++;
         }

    //! verfies within our LaunchAppQueue that the request had indeed been sent by this host.
    //! TODO: This verification is currently commented out.

    //! Finally calls the x2go pyhoca-cli client to connect to remote host and run desired application.
    queue = gFirstLaunchAppQueue;
    while(queue != NULL)
         {
//       if(queue->ip != data->cli_addr.sin_addr.s_addr)
//         printf("\nprocess_launchreq_accepted: queue->ip != data->cli_addr.sin_addr.s_addr \n");
//       if(strncmp(queue->appname,appname,20) == 0 && queue->ip == data->cli_addr.sin_addr.s_addr)
//         {
           sleep(3);
           //! stdin,stdout and stderr are piped and /usr/bin/ssh (currently hardcoded) is launched.
           pipe_to_program("/usr/bin/pyhoca-cli",args1,&piped_in,&piped_out,&piped_err);

           //! TODO:
           //! for now reading from the piped stderr of the child one char at a time
           //! eventually need to put in mechanism to determine and deal with 
           //! success/failure of the child program. 
           while(read(piped_err,&onechar,1) > 0)
                {
                printf("%c",onechar);
                }
//         }
         queue = queue->next;
         }

    return 0;   


}



/** Launches application using SSH with X Forwarding.
 *
 *  Called by process_launchreq_accepted if user's preferred connectivity option is set to ssh-x
 *
 *  This function uses the OpenSSH client to connect to the server.
 */
int launch_using_sshx(NewConnData *data)
{

    char ip[21];
    int j = 0,i = 0;
    char *saveptr1 = NULL;
    struct in_addr in;
    LaunchAppQueue *queue;
    char buf[300];
    int piped_in = 0, piped_out = 0, piped_err = 0;
    char onechar;
    User *userlist;
    char ssh_login_user[257];

    char *str1 = NULL,*token = NULL;
    //! Arguments to the OpenSSH client (see code) are currently hardcoded.Eventually will be taken in from main ebp.conf file.
    char *args1[] = {
                   "/usr/bin/ssh",
                   "-v", "-X",
                   "-o", "Host *",
                   "-o", "RSAAuthentication no",
                   "-o", "PasswordAuthentication no",
                   "-o", "IdentityFile ~/.ebp/clientkeys",
                   "-o", "Port 20101",  // TODO - temporarily hardcoded,should be dynamically 											// determined based on port on which server is listening.
                   "-o", "Ciphers arcfour256,arcfour128",
                   "-o", "StrictHostKeyChecking no",
                   "-o", "UserKnownHostsFile ~/.ebp/known_hosts",
                   "-o", "Compression yes",
                   "-o", "CompressionLevel 7",
                   "-o", "SendEnv LANG LC_*",
                   "-o", "HashKnownHosts no",
                   "-o", "GSSAPIAuthentication no",
                   "-o", "GSSAPIDelegateCredentials no",
                   "",
                   "",
                   NULL 
                   };
    char args2[2][300];
		  
    ip[0] = '\0';
    args2[0][0] = '\0';

    in.s_addr = data->ip;  
    strncpy(ip,inet_ntoa(in),20);		

    //! Determines username and ip;formats the [username]@[ip] string to be used by ssh client. 
    userlist = gUserListHead;
    while(userlist != NULL)
         {
         if(userlist->ip == data->ip)
           {          
           printf("\nlaunch_using_sshx: userlist->name = %s userlist->ssh_login_user = %s\n",userlist->name,userlist->ssh_login_user);
           // jeetu - user temporaily hardcoded;eventually the container user the remote ssh
		   // needs to connect to could be passed along as part of the avahi service message TXT
		   // field or in some other way.
           sprintf(args2[0],"%s",ip); 
           break;
           }
         userlist = userlist->next;
         }    
    
    if(args2[0][0] == '\0')
      {
      printf("\nlaunch_using_sshx: something went wrong args2[0][0] == '\0'\n");
      return 0;
      } 

    strcpy(buf,data->buffer);
    for(j = 1, str1 = buf; ; j++, str1 = NULL)  
       {
       token = strtok_r(str1, ":", &saveptr1);
       if(token == NULL)
         break;        
       printf("%d: %s\n", j, token); 
       if(j == 2) // second argument
         strcpy(args2[1],token);
       }	

    // include arguments for user@host to connect to and the appname    
    j = 0; 
    while(args1[j] != NULL)
         {
         // substitute the empty placeholder strings ("") in args1 with the correct values
         if(strcmp(args1[j],"") == 0)
           {
           args1[j] = args2[i];
           i++;
           }
         printf("args1[j] = %s\n",args1[j]);
         j++;
         }

    
    //! verfies within our LaunchAppQueue that the request had indeed been sent by this host.
    //! TODO: This verification is currently commented out.

    //! Finally calls the SSH client to connect to remote host and X forward desired application.
    queue = gFirstLaunchAppQueue;
    while(queue != NULL)
         {
//       if(queue->ip != data->cli_addr.sin_addr.s_addr)
//         printf("\nprocess_launchreq_accepted: queue->ip != data->cli_addr.sin_addr.s_addr \n");
//       if(strncmp(queue->appname,appname,20) == 0 && queue->ip == data->cli_addr.sin_addr.s_addr)
//         {
           sleep(3);
           //! stdin,stdout and stderr are piped and /usr/bin/ssh (currently hardcoded) is launched.
           pipe_to_program("/usr/bin/ssh",args1,&piped_in,&piped_out,&piped_err);

           //! TODO:
           //! for now reading from the piped stderr of the child one char at a time
           //! eventually need to put in mechanism to determine and deal with 
           //! success/failure of the child program. 
           while(read(piped_err,&onechar,1) > 0)
                {
                printf("%c",onechar);
                }
//         }
         queue = queue->next;
         }

    return 0;   


}




/** Launches the eBrainPool lxc container sandbox.
 *
 */
int launch_container(void)
{
    // Setup container struct.
    //!TODO: container name temporarily hardcoded should be read from config file.
    container = lxc_container_new("ebp-ubuntu-container", NULL); 
    if(!container) 
      {
      fprintf(stderr, "Failed to setup lxc_container struct\n");
      lxc_container_put(container);
      return 1;
      }

    if(!container->is_defined(container)) 
      {
      fprintf(stderr, "Container does not exist\n");
      lxc_container_put(container);
      return 1;
      }

    // Start the container 
    if(!container->start(container, 0, NULL)) 
      {
      fprintf(stderr, "Failed to start the container\n");
      lxc_container_put(container);
      return 1;
      }    

    return 0;
}


/** Stops the eBrainPool lxc container sandbox.
 *
 */
int stop_container(void)
{
    // Stop the container 
    if(!container->shutdown(container, 30)) 
      {
      printf("Failed to cleanly shutdown the container, forcing.\n");
      if(!container->stop(container)) 
        {
        fprintf(stderr, "Failed to kill the container.\n");
        lxc_container_put(container);
        return 1;
        }
      }

    return 0;
}
