/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.c
 * Copyright (C) Jatin Golani 2011 <ebrain@ebrain.in>
 * 
ebp is free software: you can redistribute it and/or modify it
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

int main(int argc, char *argv[])
{ 
    GtkWidget *logo = NULL;
    GtkWidget *label = NULL;
    GtkWidget *vbox = NULL;
    GtkWidget *hbox = NULL;
    GtkWidget *scrolledwindow = NULL;
    char buf[302];
    GdkRGBA rgba = {1.0,1.0,1.0,1.0};

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

    // allocate a memory block and retrieve list of apps installed on the system 
    appsdata.apps = get_installed_apps(&appsdata.count,&appsdata.blocksize); //jeetu - can pass pointer to Appsdata instead

    // get addresses of interfaces on the local system (localhost)
    getlocaladdrs(&ifaddr);

    avahi_setup();

    // jeetu - assuming for now olsr is running; add code to check for or launch olsr.
   
    // message thread that ultimately (thru listener_child) processes:
    // - olsr plugin notification for a user coming online
    // - GetApps request to send list of applications installed on current host
    // - LaunchApp request by a remote host to launch a new app
    // - LaunchAppReqAccepted message showing that remote host has accepted launch request
    g_thread_create(connlistener_thread, NULL, FALSE, NULL);    

    // thread to constantly check whether a user is still reachable i.e. online
    g_thread_create(check_client_status_thread, NULL, FALSE, NULL);

    gdk_threads_enter();
    gtk_main();
    gdk_threads_leave();

    close(sockfd);
    free(appsdata.apps); 
    freeusermem();
    freeLaunchAppQueue();
    freeLaunchDialogQueue();
    return 0;
}


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

    g_signal_connect(GTK_TREE_VIEW(treeview),"row-activated",G_CALLBACK(on_treeview_row_activated),NULL);

    return 0;
}

//
// connlistener_thread()
//
// main message processing thread that:
// - endlessly listens in on a socket
// - accepts a new connection
// - spawns a new thread to process the new connection
//
gpointer connlistener_thread(gpointer user_data)
{
    int newsockfd=0, portno=0;
    socklen_t clilen;    
    struct sockaddr_in serv_addr, cli_addr;
    fd_set rfds;
    fd_set afds;
    int nfds = 0;
    int reuse=0,ret=0;
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
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
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
           // new thread spawned to read data from a new connection and process it
           g_thread_create(newconnrequests_thread, newconndata, FALSE, NULL);
           }
         i++;
         }    	 
    return NULL;  	 
}

// newconnrequests_thread()
//
// thread launched by the connlistener thread that processes:
// - olsr plugin notification for a user coming online
// - GetApps request to send list of applications installed on current host
// - LaunchApp request by a remote host to launch a new app
// - LaunchAppReqAccepted message showing that remote host has accepted launch request
//
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

    // processes olsr plugin notification when a user comes online    
    if(buffer[0] != 0)
      {     
      strncpy(str,buffer,256);
      token = strtok_r(str,":",&saveptr1);
/*      if(strcmp(token,"eBrainOnline") == 0)
        {
        process_useronline_msg(buffer);
        }
*/
      // sends a list of applications installed on this host  
      if(strcmp(token,"GetApps") == 0)
        {
	//jeetu - for now sending the complete block of application name
	//without letting the other end know the actual datasize of data
	//to expect. 
        n = send(data->newsockfd,appsdata.apps,appsdata.blocksize,MSG_NOSIGNAL);
        if(n < 0) 
          perror("newconnrequests_thread: ERROR writing to socket");
        }
      
      // called when a remote user wishes to launch an application from this host   
      if(strcmp(token,"LaunchApp") == 0)
        {
        process_launchapp_req(buffer,data);
        }
        
      // called when the remote user has OKed our app launch request 
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


gpointer check_client_status_thread(gpointer user_data)
{
    User* temp;
    int ret;
    int comm_socket;

    while(1)
         {
         g_usleep(10000);
         if(gFirstUserNode != NULL)
           {
           temp = gFirstUserNode;
           while(temp != NULL)
                {
                ret = connect_to_client(temp->ip,&comm_socket);
                if(ret == -1)
                  {
                  /* can't connect client is probably no longer
                     alive; delete it from the list*/
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

    //check to see if remote client is reachable.
    //This takes care of a bug wherein user keeps flickering
    //in the local client when remote client is unreachable but remote olsr is up. 	  
    ret = connect_to_client(ip,&comm_socket);
    if(ret != -1)
      {
      G_LOCK(user);
      gCurrentUserNode = add_user(version,username,ip);
      G_UNLOCK(user);
      if(gCurrentUserNode != NULL)
        {
        show_user_online(gCurrentUserNode);
        }
      }

    return 0;
}



int process_useronline_avahi_msg(const char *ip_str, const char *username, const char *version)
{
    int ret = 0;
    int comm_socket = 0;
    uint32_t ip = 0;
    struct in_addr in;
    
    inet_aton(ip_str,&in);
    printf("\nprocess_useronline_avahi_msg: in.s_addr = %d\n",in.s_addr);

    //check to see if remote client is reachable.
    //This takes care of a bug wherein user keeps flickering
    //in the local client when remote client is unreachable but remote olsr is up. 	  
    ret = connect_to_client(ip,&comm_socket);
    if(ret != -1)
      {
      G_LOCK(user);
      gCurrentUserNode = add_user(version,username,in.s_addr);
      G_UNLOCK(user);
      if(gCurrentUserNode != NULL)
        {
        show_user_online(gCurrentUserNode);
        }
      }

    return 0;
}



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
    UserNode->apps_buffer = malloc(5010); //jeetu - memory to be freed; should be dynamically expanded block
    // retrieve and show applications shared by the user
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
           temp = del_user(UserNode); /* temp == new current node */
           G_UNLOCK(user);
           break;
           }
         valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(treestore),&iter);
         }

    return FALSE;
}


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
    send_launchapp_req(user,name);
    }
}


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

    G_LOCK(launchappqueue);	
    gCurrentLaunchAppQueue = add_to_launch_queue(appname,UserNode->ip,requestid);
    G_UNLOCK(launchappqueue);

    requestid++;    
	
    ret = connect_to_client(UserNode->ip,&comm_socket);
    if(ret == 0)
      {
      snprintf(buf,285,"LaunchApp:%s",appname); //jeetu - hardcoded size
      n = write(comm_socket,buf,strlen(buf));
      if(n < 0) 
        perror("send_launchapp_req: ERROR writing to socket");       
      close(comm_socket);  
      }	
}


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

    //Search our user's list and find username from the IP
    temp = gFirstUserNode;	
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

    launchdialogqueue = add_launchdialog_queue(username,appname,data->ip);
    g_idle_add(launch_approve_dialog,launchdialogqueue);
		
    return 0;
}



gboolean launch_approve_dialog(gpointer data)
{
    GtkWidget *dialog;
    LaunchDialogQueue *launchdialogqueue = (LaunchDialogQueue *) data;

    dialog = gtk_message_dialog_new(GTK_WINDOW(window),GTK_DIALOG_DESTROY_WITH_PARENT,GTK_MESSAGE_QUESTION,GTK_BUTTONS_YES_NO,
                               "User %s wishes to run the application %s off your system.\nAllow?",launchdialogqueue->username,launchdialogqueue->appname);
    g_signal_connect(dialog,"response",G_CALLBACK(launch_approve_dialog_response),launchdialogqueue);
    gtk_widget_show_all(dialog);

    return FALSE;
}



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
             //todo - send a rejection signal to the initiating client
             break;
          default:
             break;
          }

    gtk_widget_destroy(dialog);
}




void launchdlg_approved(char *appname,uint32_t ip)
{
    int comm_socket = 0;
    int ret = 0;
    char buf[300];	
    int n = 0;

    g_thread_create(start_server,&ip,FALSE,NULL);

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



gpointer start_server(gpointer ptr_ip)
{
    char temp[256];
    char str_ip[21];
    struct in_addr in;
    int ip = 0;

    ip = *(int *)ptr_ip;

    in.s_addr = ip;
    strncpy(str_ip,inet_ntoa(in),20);

    temp[0] = '\0';	

    strncat(temp,"~/.ebp/start_srv ",18); //jeetu - hardcoded 
    strncat(temp,str_ip,20);
    system(temp); 

    return NULL;
}


int process_launchreq_accepted(NewConnData *data)
{
    char temp[350];
    char appname[257];
    char ip[21];
    int j = 0;
    char *str1 = NULL,*token = NULL;
    char *saveptr1 = NULL;
    struct in_addr in;
    LaunchAppQueue *queue;
    char buf[300];
		  
    appname[0] = '\0';
    temp[0] = '\0';
    ip[0] = '\0';
 
    in.s_addr = data->ip;  
    strncpy(ip,inet_ntoa(in),20);		

    strcpy(buf,data->buffer);
    for(j = 1, str1 = buf; ; j++, str1 = NULL)  
       {
       token = strtok_r(str1, ":", &saveptr1);
       if(token == NULL)
         break;        
       printf("%d: %s\n", j, token); 
       if(j == 2) // second argument
         strcpy(appname,token);
       }	

    //check to see if the request was indeed sent
    queue = gFirstLaunchAppQueue;
    while(queue != NULL)
         {
//         if(queue->ip != data->cli_addr.sin_addr.s_addr)
//           printf("\nprocess_launchreq_accepted: queue->ip != data->cli_addr.sin_addr.s_addr \n");
//         if(strncmp(queue->appname,appname,20) == 0 && queue->ip == data->cli_addr.sin_addr.s_addr)
//           {
           temp[0] = '\0';
           sleep(3);
           printf("\nprocess_launchreq_accepted: running ~/.ebp/connect_fromclient\n");
           strncat(temp,"~/.ebp/connect_fromclient ",27);
           strncat(temp,ip,20);
           strncat(temp," ",1);
           strncat(temp,appname,50);
           printf("\nprocess_launchreq_accepted: temp = %s\n",temp);
           system(temp); 
//           }
         queue = queue->next;	  	     
         }  	  

    return 0;   
}
