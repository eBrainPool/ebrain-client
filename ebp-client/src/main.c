/*
 * main.c contains main().  
 * The starting point of the application
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <errno.h>

#include <gtk/gtk.h>

#include "ebp.h"

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

    // jeetu - assuming for now olsr is running; add code to check for or launch olsr.
   
    // message thread that ultimately (thru listener_child) processes:
    // - olsr plugin notification for a user coming online
    // - GetApps request to send list of applications installed on current host
    // - LaunchApp request by a remote host to launch a new app
    // - LaunchAppReqAccepted message showing that remote host has accepted launch request
//    newconndata = (NewConnData **) g_new0(NewConnData,1);
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
  GdkRGBA rgba = {1.0,1.0,1.0,1.0};

//  gtk_widget_override_background_color(treeview,GTK_STATE_NORMAL, &rgba);

  col = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(col, "eBrained Applications");
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);

  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(col, renderer, TRUE);
  gtk_tree_view_column_add_attribute(col, renderer, 
      "text", NAME);

  gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(treestore));
  g_object_unref(treestore); 

//  selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
  g_signal_connect(GTK_TREE_VIEW(treeview),"row-activated",G_CALLBACK(on_treeview_row_activated),NULL);

  return 0;
}

// main message processing thread that:
// - endlessly listens in on a socket
// - accepts a new connection
// - spawns a new thread to process the new connection
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
    NewConnData *newconndata;
      
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
      	   
           newconndata = add_newconn(newsockfd,cli_addr);
           //printf("\nconnlistener_thread: new thread spawned here i = %d\n",i);
           // new thread spawned to read data from a new connection and process it
           g_thread_create(newconnrequests_thread, newconndata, FALSE, NULL);
           }
         i++;
         }    	 
    return NULL;  	 
}

// thread launched by the connlistener thread that processes:
// - olsr plugin notification for a user coming online
// - GetApps request to send list of applications installed on current host
// - LaunchApp request by a remote host to launch a new app
// - LaunchAppReqAccepted message showing that remote host has accepted launch request
gpointer newconnrequests_thread(gpointer user_data)
{
    char buffer[260];
    int n = 0;
    NewConnData *data = NULL;
    char str[260];
    char *token = NULL;
    char *saveptr1 = NULL;
	
    data = user_data;	

    bzero(str,260);	
    bzero(buffer,260);
    n = read(data->newsockfd,buffer,256);
    if(n < 0)
      { 
      perror("listener_child: ERROR reading from socket");
      close(data->newsockfd);
      return NULL;
      }
    
    if(buffer[0] != 0)
      {     
      strncpy(str,buffer,256);
      token = strtok_r(str,":",&saveptr1);
      if(strcmp(token,"eBrainOnline") == 0)
        {
        process_useronline_msg(buffer);
        }
      // sends a list of applications installed on this host  
      if(strcmp(token,"GetApps") == 0)
        {
        printf("\nnewconnrequests_thread: GetApps received\n");
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
        printf("\nnewconnrequests_thread: LaunchApp received\n");
        process_launchapp_req(buffer,data);
        }
        
      // called when the remote user has OKed our app launch request 
      if(strcmp(token,"LaunchAppReqAccepted") == 0)
        {
        printf("\nnewconnrequests_thread: LaunchAppReqAccepted received\n");
        strncpy(data->buffer,buffer,300);
        //process_launchreq_accepted(buffer,data);
        g_thread_create(process_launchreq_accepted,data,FALSE,NULL);
        }           
      }
    
    close(data->newsockfd);
//    g_free(data); // should free memory pointed by data and passed in by newconndata
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
                  //show_user_offline(temp);
                  //printf("\ncheck_client_status_thread: temp->name = %s offline\n",temp->name);
                  g_idle_add(show_user_offline,temp);
                  close(comm_socket);
                  break;
                  //G_LOCK(user);
                  //temp = del_user(temp); /* temp == new current node */
                  //G_UNLOCK(user);		    	  
                  }
                else
                  temp = temp->next;
                close(comm_socket);
                }
           }
         }

    return NULL;
}




char* get_installed_apps(int* count,int* blocksize)
{	
    int n = 0;
    int i = 0;
    char *chr;
    struct dirent **namelist = NULL;
    int size = 0;
    char *buf;
    
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
           chr = strrchr(namelist[i]->d_name,'.');
           *chr = '\0';
      	   size = size + strlen(namelist[i]->d_name) + 8;
      	   buf = realloc(buf,size);
           strncat(buf,namelist[i]->d_name,strlen(namelist[i]->d_name)); 
      	   strncat(buf,":",1);
      	   free(namelist[i]);
	   i++;
           }
      free(namelist);
      }	
	  
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



int process_useronline_msg(char *buf)
{
    int j=0;
    char *str1=NULL,*token = NULL,*str2 = NULL,*subtoken = NULL;
    char *saveptr1 = NULL,*saveptr2 = NULL;
    char username[20];
    char version[6];
    uint32_t ip = 0;
    User *temp = NULL;
    int ret = 0;
    int comm_socket = 0;

    username[0] = '\0';
    version[0] = '\0';
    
    //printf("\nIn process_useronline_msg: buf = %s\n",buf);
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



User* add_user(char* version,char* name,uint32_t ip)
{
    User* temp = NULL;
    User* current_node = NULL;

    //printf("\nIn add_user\n");
 
    temp = gFirstUserNode;
    while(temp != NULL)
         {
         //printf("\nadd_user: in loop (temp !=NULL) temp->ip = %d",temp->ip);
         if(temp->ip == ip)
           return NULL;
         temp = temp->next;
         }

    // first entry		
    if(gFirstUserNode == NULL)
      {
      //printf("\nadd_user: gFirstUserNode == NULL\n");
      // memory freed either during del_user or at the end in freeusermem
      current_node = (User *) malloc(sizeof(User));
      current_node->prev = NULL;
      current_node->next = NULL;
      gFirstUserNode = current_node;
      gLastUserNode = current_node;
      }
    else
      {	
      //printf("\nadd_user: gFirstUserNode != NULL\n");	  
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



NewConnData *add_newconn(int newsockfd,struct sockaddr_in cli_addr)
{
    NewConnData* temp = NULL;
    NewConnData* current_node = NULL;

    // first entry		
    if(gFirstConn == NULL)
      {
      //printf("\nadd_newconn: gFirstConn == NULL\n");
      // memory freed in newconnrequests_thread with call to del_newconn
      current_node = (NewConnData *) malloc(sizeof(NewConnData));
      current_node->prev = NULL;
      current_node->next = NULL;
      gFirstConn = current_node;
      gLastConn = current_node;
      }
    else
      {	
      //printf("\nadd_newconn: gFirstConn != NULL\n");	  
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
    current_node->cli_addr = cli_addr;
	
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
     //printf("\nadd_launchdialog_queue: gFirstLaunchDialog == NULL\n");
     // memory freed in main() by call to freeLaunchDialogQueue
     current_node = (LaunchDialogQueue *) malloc(sizeof(LaunchDialogQueue));
     current_node->prev = NULL;
     current_node->next = NULL;
     gFirstLaunchDialog = current_node;
     gLastLaunchDialog = current_node;
     }
   else
     {	
     //printf("\nadd_launchDialog_queue: gFirstLaunchDialog != NULL\n");	  
     // memory freed in main() by call to freeLaunchDialogQueue
     current_node = gLastLaunchDialog;
     temp = current_node;
     current_node->next = (LaunchDialogQueue *) malloc(sizeof(LaunchDialogQueue));
     current_node = current_node->next;
     current_node->prev = temp;
     current_node->next = NULL;
     gLastLaunchDialog = current_node;
     }

   current_node->username = username;
   current_node->appname = appname;
   current_node->ip = ip;
	
   return current_node;

}



int show_user_online(User *UserNode)
{
    GtkTreeIter toplevel, child;
    int j = 0;
    char *str1 = NULL,*token = NULL;
    int ret = 0;
    char *saveptr1 = NULL;    
    
    //printf("\nIn show_user_online\n");
//    G_LOCK(treeview);
 //   gtk_widget_override_background_color(treeview,GTK_STATE_NORMAL, NULL);
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




int get_remoteuser_apps(User *UserNode)
{
    int ret = 0;
    int comm_socket = 0; 
    int n = 0;
    char *saveptr1 = NULL;  
  
    ret = connect_to_client(UserNode->ip,&comm_socket);
    if(ret == 0)
      {
      //printf("\nget_remoteuser_apps: connect_to_client succeeds\n");
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

    //printf("\nget_remoteuser_apps: UserNode->apps_buffer = %s\n", UserNode->apps_buffer);
    close(comm_socket);
    return ret;
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
    //printf("\non_treeview_row_activated: selected name = %s user->name = %s user->ip = %d\n",name,user->name,user->ip);
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

    printf("\nIn send_launchapp_req: UserNode->ip = %d, appname = %s\n",UserNode->ip,appname);

    in.s_addr = UserNode->ip;  //jeetu - this is wrong. ip should actually be the ip of the client sending request
    ip = inet_ntoa(in);	
    buf[0] = '\0';

    G_LOCK(launchappqueue);	
    gCurrentLaunchAppQueue = add_to_launch_queue(appname,UserNode->ip,requestid);
    G_UNLOCK(launchappqueue);

    requestid++;    
	
    ret = connect_to_client(UserNode->ip,&comm_socket);
    if(ret == 0)
      {
      snprintf(buf,285,"LaunchApp:%s:%s",appname,ip); //jeetu - hardcoded size
      n = write(comm_socket,buf,strlen(buf));
      if(n < 0) 
        perror("send_launchapp_req: ERROR writing to socket");       
      close(comm_socket);  
      }	
}


LaunchAppQueue* add_to_launch_queue(char *appname,int ip,int reqid)
{
    LaunchAppQueue* temp;
    LaunchAppQueue* current_node;

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



int process_launchapp_req(char *buf,NewConnData *data)
{
    char appname[257];
    char ip[21];
    int j;
    char *str1 = NULL,*token = NULL;
    char *saveptr1 = NULL;
    User *temp;
    char username[20];
    LaunchDialogQueue *launchdialogqueue = NULL;
	
    appname[0] = '\0';
    ip[0] = '\0';
    username[0] = '\0';

    for(j = 1, str1 = buf; ; j++, str1 = NULL) 
       {
       token = strtok_r(str1, ":", &saveptr1);
       if(token == NULL)
         break;         
       if(j == 2) // second argument
         strncpy(appname,token,256);
       if(j == 3) // third argument
       	 strncpy(ip,token,20);
       }	
	
    //Search our user's list and find username from the IP
    temp = gFirstUserNode;	
    while(temp != NULL)
         {
         if(temp->ip == data->cli_addr.sin_addr.s_addr)
           {
           strncpy(username,temp->name,19);
           break;
           }
         temp = temp->next;
         }

    printf("\nprocess_launchapp_req: username = %s\n",username);	
    // Get User Approval to let remote user launch app 
    launchdialogqueue = add_launchdialog_queue(username,appname,data->cli_addr.sin_addr.s_addr);
    g_idle_add(launch_approve_dialog,launchdialogqueue);
		
    return 0;
}



void set_usernode_locked(User *destnode, User* srcnode)
{
    G_LOCK(user);
    destnode = srcnode;
    G_UNLOCK(user);     
}


/*
void set_launchappqueue_locked(LaunchAppQueue *dest,LaunchAppQueue *src)
{
    G_LOCK(launchappqueue);
    dest = src;
    G_UNLOCK(launchappqueue);
} 
*/

gboolean launch_approve_dialog(gpointer data)
{
    GtkWidget *dialog;
    gint result;
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
             printf("\nlaunch_approve_dialog_response: YESSSSSSSSSSS!!!\n");
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

    // jeetu - possible bug; ~/.ebp/start_srv can't launch multiple sshd's to listen on the same port
    // therefore this script may fail beyond the first launch
    //start_server(ip);
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



//void start_server(uint32_t ip)
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


gpointer process_launchreq_accepted(gpointer user_data)
{
    char temp[350];
    char appname[257];
    char ip[21];
    int j = 0;
    char *str1 = NULL,*token = NULL;
    char *saveptr1 = NULL;
    struct in_addr in;
    LaunchAppQueue *queue;
    NewConnData *data = (NewConnData *) user_data;
		  
    appname[0] = '\0';
    temp[0] = '\0';
    ip[0] = '\0';
 
    in.s_addr = data->cli_addr.sin_addr.s_addr;  
    strncpy(ip,inet_ntoa(in),20);		  

    //for(j = 1, str1 = buffer; ; j++, str1 = NULL)
    for(j = 1, str1 = data->buffer; ; j++, str1 = NULL)  
       {
       token = strtok_r(str1, ":", &saveptr1);
       if(token == NULL)
         break;         
       if(j == 2) // second argument
         strncpy(appname,token,256);
       }	
	  
    //check to see if the request was indeed sent
    queue = gFirstLaunchAppQueue;
    while(queue != NULL)
         {
         if(strncmp(queue->appname,appname,20) == 0 && queue->ip == data->cli_addr.sin_addr.s_addr)
           {
           temp[0] = '\0';
           sleep(3);
           printf("\nprocess_launchreq_accepted: running ~/.ebp/connect_fromclient\n");
           strncat(temp,"~/.ebp/connect_fromclient ",27);
           strncat(temp,ip,20);
           strncat(temp," ",1);
           strncat(temp,appname,50);
           system(temp); 
           }
         queue = queue->next;	  	     
         }  	  

    return NULL;   
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


