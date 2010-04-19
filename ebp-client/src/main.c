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

#include <clutter/clutter.h>
#include <nbtk/nbtk.h>

#include "ebp.h"

int 
main (int argc, char *argv[])
{ 
    ClutterActor *stage = NULL;
    NbtkWidget *box = NULL;
    NbtkWidget *scroll = NULL;
    ListenerThreadData *data;
    ClutterActor *logo = NULL;
    NbtkLabel *label;
    char buf[300];

    g_thread_init (NULL);
    clutter_threads_init ();
    clutter_init (&argc, &argv);	

    stage = clutter_stage_get_default ();
    clutter_actor_set_size (stage, STAGE_WIDTH, STAGE_HEIGHT);
    clutter_stage_set_title((ClutterStage *)stage,"eBrain");
    clutter_stage_set_user_resizable (CLUTTER_STAGE (stage), TRUE);
  	
    //jeetu - hard coded
    logo = clutter_texture_new_from_file("./ebrain_logo_1.png",NULL);
    clutter_container_add_actor (CLUTTER_CONTAINER (stage), CLUTTER_ACTOR(logo));
    clutter_actor_show (logo);	  
    
    snprintf(buf,300,"eBrain allows you to run\nsoftware from other\nnearby eBrainers shown\nin the list below.");
    label = (NbtkLabel *) nbtk_label_new(buf);
    clutter_actor_set_position((ClutterActor *)label,90,10);	
    clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                                   CLUTTER_ACTOR (label));	
  	
    scroll = (NbtkWidget *) nbtk_scroll_view_new ();
    clutter_actor_set_position (CLUTTER_ACTOR (scroll), SCROLLVIEW_XPOS, SCROLLVIEW_YPOS);
    clutter_container_add_actor (CLUTTER_CONTAINER (stage),
                               CLUTTER_ACTOR (scroll));
    clutter_actor_set_size (CLUTTER_ACTOR (scroll), SCROLLVIEW_WIDTH, SCROLLVIEW_HEIGHT);

    box = nbtk_box_layout_new();	
    clutter_container_add_actor (CLUTTER_CONTAINER (scroll), (ClutterActor*) box); 
    nbtk_box_layout_set_vertical((NbtkBoxLayout *) box, TRUE);   

    clutter_actor_show (stage);  	  
  	
  	// jeetu - assuming for now olsr is running; add code to launch olsr.
  	
    data = thread_data_new();
    data->stage = g_object_ref(stage);
    data->box = g_object_ref(box);
    data->newsockfd = -1;
    // message thread that ultimately (thru listener_child) processes:
    // - olsr plugin notification for a user coming online
    // - GetApps request to send list of applications installed on current host
    // - LaunchApp request by a remote host to launch a new app
    // - LaunchAppReqAccepted message showing that remote host has accepted launch request
    g_thread_create(listener, data, FALSE, NULL);

    // thread to constantly check whether a user is still reachable i.e. online
    g_thread_create(check_client_status, data, FALSE, NULL);
  	
    appsdata.apps = list_installed_apps(&appsdata.count,&appsdata.blocksize);
    	
    clutter_threads_enter();
    clutter_main ();	
    clutter_threads_leave();
	
    close(data->sockfd);
    free(appsdata.apps); 
    freeusermem();
    freeLaunchAppQueue();

    return EXIT_SUCCESS;
}


// main message processing thread that:
// - endlessly listens in on a socket
// - accepts a new connection
// - spawns a new thread to process the new connection
static gpointer listener(gpointer user_data)
{
    int newsockfd, portno;
    socklen_t clilen;    
    struct sockaddr_in serv_addr, cli_addr;
    ListenerThreadData *data;
    fd_set rfds;
    fd_set afds;
    int nfds;
    int reuse,ret;
    
    data = user_data;
    g_static_private_set (&thread_data, data, NULL);
      
    data->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(data->sockfd < 0) 
       perror("listener: ERROR opening socket");
       
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 2010;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);
    
    /* Enable address reuse */
    reuse = 1;
    ret = setsockopt(data->sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    if(bind(data->sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
      perror("listener: ERROR on binding");      
    
    listen(data->sockfd,5); 
    
    clilen = sizeof(cli_addr);    
    nfds = getdtablesize();
    FD_ZERO(&afds);
    FD_SET(data->sockfd, &afds);
    while(1) 
         {
	     memcpy(&rfds, &afds, sizeof(rfds));
         if(select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0) < 0)
            perror("listener: ERROR on select");

         if(FD_ISSET(data->sockfd, &rfds))
		   {
           newsockfd = accept(data->sockfd, (struct sockaddr *) &cli_addr, &clilen);            
           if(newsockfd < 0) 
      	     perror("listener: ERROR on accept");
      	   
           data->newsockfd = newsockfd;
           data->cli_addr = cli_addr;
           g_thread_create(listener_child, data, FALSE, NULL);
           }
         }    	 
    return NULL;  	 
}

// thread launched by the listener thread that processes:
// - olsr plugin notification for a user coming online
// - GetApps request to send list of applications installed on current host
// - LaunchApp request by a remote host to launch a new app
// - LaunchAppReqAccepted message showing that remote host has accepted launch request
static gpointer listener_child(gpointer user_data)
{
    char buffer[301];
    int n;
    ListenerThreadData *data;
    char str[256];
    char *token;
    char *saveptr1 = NULL;
	
    data = user_data;	
    g_static_private_set (&listener_child_data, data, NULL);
	
    bzero(buffer,256);
    n = read(data->newsockfd,buffer,255);
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
        strncpy(data->buffer,buffer,256);
        clutter_threads_add_idle_full (G_PRIORITY_DEFAULT + 30,
                                 	process_useronline_msg,
                                    data, NULL);
        }
      // sends a list of applications installed on this host  
      if(strcmp(token,"GetApps") == 0)
        {
	    //jeetu - for now sending the complete block of application name
	    //without letting the other end know the actual datasize of data
	    //to expect. 
        n = send(data->newsockfd,appsdata.apps,appsdata.blocksize,MSG_NOSIGNAL);
        if(n < 0) 
          perror("listener_child: ERROR writing to socket");
        }
      
      // called when a remote user wishes to launch an application from this host   
      if(strcmp(token,"LaunchApp") == 0)
        {
        strncpy(data->buffer,buffer,300);
        clutter_threads_add_idle_full (G_PRIORITY_DEFAULT + 30,
                                 	process_launchapp_req,
                                 	data, NULL);
        }
        
      // called when the remote user has OKed our app launch request 
      if(strcmp(token,"LaunchAppReqAccepted") == 0)
        {
        strncpy(data->buffer,buffer,300);
        g_thread_create(LaunchAppFromHost, data, FALSE, NULL);
        }           
      }
    close(data->newsockfd);
    return NULL;
}


static gboolean process_useronline_msg (gpointer user_data)
{
    ListenerThreadData *data = user_data;
    int j;
    char *str1,*token,*str2,*subtoken;
    char *saveptr1 = NULL,*saveptr2 = NULL;
    char username[20];
    char version[6];
    uint32_t ip = 0;
    struct user* temp;
    int ret = 0;
    int comm_socket = 0;
            
    for(j = 1, str1 = data->buffer; ; j++, str1 = NULL) 
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
	
 	// search if this user already exists in our list
    temp = gFirstUserNode;
    while(temp != NULL)
         {
         if(temp->ip == ip)
           break;
         temp = temp->next;
         }
		 
    // if this is a new user
    if(temp == NULL)
      {
      //check to see if remote client is reachable.
      //This takes care of a bug wherein user keeps flickering
      //in the local client when remote client is unreachable but remote olsr is up. 	  
      ret = connect_to_client(ip,&comm_socket);
      if(ret != -1)
        {
        gCurrentUserNode = add_user(1000,version,username,ip);
        show_user_online(data,gCurrentUserNode);
        }
      }
    return FALSE;
}


int show_user_online(ListenerThreadData *data,struct user *UserNode)
{
    gfloat width,height; 

    UserNode->expander = nbtk_expander_new ();
    nbtk_expander_set_label (NBTK_EXPANDER (UserNode->expander), UserNode->name);

    clutter_container_add_actor (CLUTTER_CONTAINER (data->box),
                               CLUTTER_ACTOR (UserNode->expander));                              
                               
    clutter_actor_get_size (CLUTTER_ACTOR(UserNode->expander), &width, &height);

    g_signal_connect (UserNode->expander, "expand-complete",
                    G_CALLBACK (expand_complete_cb), UserNode);

    UserNode->scroll = (NbtkWidget *) nbtk_scroll_view_new ();
    clutter_container_add_actor (CLUTTER_CONTAINER (UserNode->expander),
                               CLUTTER_ACTOR (UserNode->scroll));
    clutter_actor_set_size (CLUTTER_ACTOR (UserNode->scroll), 220, 240);

    UserNode->grid = nbtk_grid_new ();
    clutter_container_add_actor (CLUTTER_CONTAINER (UserNode->scroll),
                               CLUTTER_ACTOR (UserNode->grid));
	return 0;
}


static gboolean show_user_offline(gpointer user_data)
{
    OfflineUserData* offline;
	
    offline = user_data;
    clutter_container_remove_actor(CLUTTER_CONTAINER(offline->uidata->box),
                                   CLUTTER_ACTOR(offline->offlineuser->expander));							   
    return(FALSE);
}

/*
 * Flow of control for launching an application -
 *
 * Local Host:
 * - expand_complete_cb() - shows list of apps available on remote host
 *   - send_launchapp_req() 
 *
 * Remote Host:
 * - process_launchapp_req()
 *   - launch_approve_dialog()
 *     - launchdlg_approved() 
 *       - calls start_server() to start ssh server
 *	     - sends LaunchAppReqAccepted signal to local host
 *
 * Local Host: 
 * - LaunchAppFromHost() connects to the ssh server and launches app
 *
 */


// Called when an expander for an online user is clicked and completes expanding.
static void
expand_complete_cb (NbtkExpander  *expander,
                    struct user* UserNode)
{
    gboolean expanded;
    gfloat width, height;
    int ret = 0;
    int comm_socket = 0; 
    int n = 0;
    char *buffer = NULL;
    int j = 0;
    char *str1,*token;
    char *saveptr1 = NULL;  
  
    expanded = nbtk_expander_get_expanded (expander); 
    clutter_actor_get_size (CLUTTER_ACTOR(expander), &width, &height);	     
  	            
    if(expanded)
      {
      // for now retrieve the application list from the remote host
      // each time the expander is opened. This should be cached.
      buffer = malloc(5000); //jeetu - hardcoded memory block
      ret = connect_to_client(UserNode->ip,&comm_socket);
      if(ret == 0)
        {
        n = write(comm_socket,"GetApps:",9);
        if(n < 0) 
          perror("expand_complete_cb: ERROR writing to socket");
        n = recv(comm_socket,buffer,5000,MSG_WAITALL);
        if(n < 0) 
          perror("expand_complete_cb: ERROR reading from socket");
        buffer[n] = '\0';  
        }
      close(comm_socket);
    
      // displays buttons for each app in the remote host    
      for(j = 1, str1 = buffer; ; j++, str1 = NULL) 
         {
         NbtkWidget *button;
         gchar *label;

         token = strtok_r(str1, ":", &saveptr1);
         if(token == NULL)
           break;
         label = g_strdup_printf ("%s",token);
         button = nbtk_button_new_with_label (label);
         clutter_container_add_actor (CLUTTER_CONTAINER (UserNode->grid),
                                   CLUTTER_ACTOR (button));
	     // launchapp() is called if an app button is clicked                                                
         g_signal_connect(button, "clicked",
                          G_CALLBACK (send_launchapp_req), UserNode);
         g_free (label);
         }
    
      free(buffer);
      }              	
}

static void send_launchapp_req(NbtkButton *button,struct user* UserNode)
{
    char *ip = NULL;
    struct in_addr in;
    int comm_socket = 0;
    int ret = 0;
    char buf[300];
    gchar *appname;
	int n = 0;
	char temp[350];

    in.s_addr = UserNode->ip;  //jeetu - this is wrong. ip should actually be the ip of the client sending request
    ip = inet_ntoa(in);	
    buf[0] = '\0';
    temp[0] = '\0';	
	
    appname = (char *)nbtk_button_get_label(button);
    gCurrentLaunchAppQueue = add_to_launch_queue(appname,UserNode->ip,requestid);
    requestid++;    
	
    ret = connect_to_client(UserNode->ip,&comm_socket);
    if(ret == 0)
      {
      snprintf(buf,285,"LaunchApp:%s:%s",appname,ip);
      n = write(comm_socket,buf,strlen(buf));
      if(n < 0) 
        perror("send_launchapp_req: ERROR writing to socket");       
      close(comm_socket);  
      }	
}

// called when the client receives a request from a remote host to launcha an app
static gboolean process_launchapp_req(gpointer user_data)
{
    ListenerThreadData *data = user_data;
    char appname[257];
    char ip[21];
    int j;
    char *str1 = NULL,*token = NULL;
    char *saveptr1 = NULL;
    struct user *temp;
    char username[20];
	
    appname[0] = '\0';
    ip[0] = '\0';
    username[0] = '\0';

    for(j = 1, str1 = data->buffer; ; j++, str1 = NULL) 
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
	
    // Get User Approval to let remote user launch app 
    launch_approve_dialog(username,appname,data->cli_addr.sin_addr.s_addr);
		
    return(FALSE);
}


gboolean launch_approve_dialog(char *name,char *appname,uint32_t ip)
{
    ClutterActor *dialog;
    NbtkWidget *button_yes;
    NbtkWidget *button_no;
    NbtkLabel *msg;
    char buf[300];
    LaunchApprDlgData *data;
	
    buf[0] = '\0';
    dialog = clutter_stage_new();
    clutter_actor_set_size(dialog,360,100);
    clutter_stage_set_title((ClutterStage *) dialog,"eBrain - Approval Required");
    snprintf(buf,300,"User %s wishes to run an application off your system.\nAllow?",name);
    msg = (NbtkLabel *) nbtk_label_new(buf);
    clutter_actor_set_position((ClutterActor *)msg,10,10);	
    clutter_container_add_actor (CLUTTER_CONTAINER (dialog),
                                   CLUTTER_ACTOR (msg));
	
    button_yes = nbtk_button_new_with_label ("Yes");
    clutter_actor_set_position((ClutterActor *)button_yes,140,50);
    clutter_container_add_actor (CLUTTER_CONTAINER (dialog),
                                   CLUTTER_ACTOR (button_yes));
                                   
    button_no = nbtk_button_new_with_label ("No");
    clutter_actor_set_position((ClutterActor *)button_no,190,50);
    clutter_container_add_actor (CLUTTER_CONTAINER (dialog),
                                   CLUTTER_ACTOR (button_no));
    
    data = malloc(sizeof(LaunchApprDlgData));
    
    data->dialog = dialog;
    strcpy(data->appname,appname);
    data->ip = ip;
    
    clutter_actor_show(dialog);
       
    g_signal_connect(button_yes, "clicked",
                     G_CALLBACK (launchdlg_approved), data);
    g_signal_connect(button_no, "clicked",
                     G_CALLBACK (launchdlg_rejected), data);                
       	
    return FALSE;
}

static void launchdlg_approved(NbtkButton *button,LaunchApprDlgData* data)
{
    int comm_socket = 0;
    int ret = 0;
    char buf[300];	
    int n = 0;
	
    g_thread_create(start_server, data, FALSE, NULL);
	
    ret = connect_to_client(data->ip,&comm_socket);
    if(ret == 0)
      {
      snprintf(buf,285,"LaunchAppReqAccepted:%s",data->appname);
      n = write(comm_socket,buf,strlen(buf));
      if(n < 0) 
        perror("launchdlg_approved: ERROR writing to socket");        
      close(comm_socket);
      }
      
    clutter_actor_destroy((ClutterActor *) data->dialog);
    free(data);
}

static gpointer start_server(gpointer user_data)
{
    char temp[256];
    char ip[21];
    struct in_addr in;
	
    LaunchApprDlgData *data = user_data;

    in.s_addr = data->ip;
    strncpy(ip,inet_ntoa(in),20);

    temp[0] = '\0';	

    strncat(temp,"~/.ebp/start_srv ",18); //jeetu - hardcoded 
    strncat(temp,ip,20);
    system(temp); 
    
    return NULL;	
}


static void launchdlg_rejected(NbtkButton *button,LaunchApprDlgData* data)
{
    clutter_actor_destroy((ClutterActor *) data->dialog);
    free(data); 
}


// called when the remote user has OKed request to launch app and started the ssh server
static gpointer LaunchAppFromHost(gpointer user_data)
{
      char temp[350];
      ListenerThreadData *data = user_data;
      char appname[257];
      char ip[21];
      int j;
      char *str1 = NULL,*token = NULL;
      char *saveptr1 = NULL;
      struct in_addr in;
      struct LaunchAppQueue *queue;
		  
      appname[0] = '\0';
      temp[0] = '\0';
      ip[0] = '\0';
 
      in.s_addr = data->cli_addr.sin_addr.s_addr;  
      strncpy(ip,inet_ntoa(in),20);		  

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
      	     sleep(3);
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

static ListenerThreadData * thread_data_new (void)
{
    ListenerThreadData *data;

    data = g_new0 (ListenerThreadData, 1);

    return data;
}

struct user* add_user(int index, char* version,char* name,uint32_t ip)
{
    struct user* temp;
    struct user* current_node;

    // first entry		
    if(gFirstUserNode == NULL)
      {
      // memory freed either during del_user or at the end in freeusermem
      current_node = (struct user *) malloc(sizeof(struct user));
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
      current_node->next = (struct user *) malloc(sizeof(struct user));
      current_node = current_node->next;
      current_node->prev = temp;
      current_node->next = NULL;
      gLastUserNode = current_node;
      }

    current_node->index = index;
    strncpy(current_node->name,name,20); //jeetu - hardcoded size
    strncpy(current_node->version,version,6);
    current_node->ip = ip;
	
    return current_node;
}


struct user* del_user(struct user* deluser)
{
    struct user* prevuser;
    struct user* nextuser;
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

static gpointer check_client_status(gpointer user_data)
{
    struct user* temp;
    int ret;
    ListenerThreadData* data;
    OfflineUserData offline;
    int comm_socket;

    data = user_data;
    offline.uidata = data;

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
                  offline.offlineuser = temp;
                  clutter_threads_add_idle_full (G_PRIORITY_DEFAULT + 30,
                                                show_user_offline,
                                                &offline, NULL);
                  temp = del_user(temp); /* temp == new current node */		    	  
                  }
                else
                  temp = temp->next;
                close(comm_socket);
                }
           }
          }

    return NULL;
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

char* list_installed_apps(int* count,int* blocksize)
{	
    int n = 0;
    int i = 0;
    char *chr;
    struct dirent **namelist = NULL;
    char *buf;
    int size = 0;
    
    //memory freed in main() when application quits
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


struct LaunchAppQueue* add_to_launch_queue(char *appname,int ip,int reqid)
{
    struct LaunchAppQueue* temp;
    struct LaunchAppQueue* current_node;

    // first entry		
    if(gFirstLaunchAppQueue == NULL)
      {
      // memory allocation freed in freeLaunchAppQueue ()
      current_node = (struct LaunchAppQueue *) malloc(sizeof(struct LaunchAppQueue));
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
      current_node->next = (struct LaunchAppQueue *) malloc(sizeof(struct LaunchAppQueue));
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


void freeusermem(void)
{
    struct user *temp = gLastUserNode;
    struct user *prev = NULL;

    while(temp != NULL)
         {
         prev = temp->prev;
         free(temp);
         temp = prev;
         }		 		 		 
}

void freeLaunchAppQueue(void)
{
    struct LaunchAppQueue* temp;
    struct LaunchAppQueue* prev;
	
    temp = gLastLaunchAppQueue;
    while(temp != NULL)
         {
         prev = temp->prev;
         free(temp);
         temp = prev;
         }
}


