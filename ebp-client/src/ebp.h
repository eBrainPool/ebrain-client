/*
 * ebp.h
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

#ifndef _EBP_H
#define _EBP_H

enum
{
  NAME = 0,
  USERNODE = 1,
  NUM_COLS
};

#define WINDOW_WIDTH 460
#define WINDOW_HEIGHT 300
#define CLIENT_COMM_PORT 2010

//typedef struct
//    {
//    GtkWidget *treeview;
//    GtkTreeStore *treestore;
//    int sockfd;
//    int newsockfd;
//    struct sockaddr_in cli_addr;
//    char buffer[300];
//    } ConnListenerThreadData;

typedef struct _newconndata
    {
    int newsockfd;
    struct sockaddr_in cli_addr;
    char buffer[310];
    struct _newconndata *prev;
    struct _newconndata *next;
    } NewConnData;

typedef struct
    {
    char *apps;
    int count;
    int blocksize;
    } AppsData;

typedef struct _user 
    {
    char name[20];
    char version[6];
    uint32_t ip;
    char *apps_buffer;
    unsigned int noofapps;
    struct _user *prev;
    struct _user *next;	
    } User;

typedef struct _LaunchAppQueue
    {
    gchar appname[300];
    int ip;
    int reqid;
    struct _LaunchAppQueue *prev;
    struct _LaunchAppQueue *next;
    } LaunchAppQueue;

typedef struct _LaunchDialogQueue
    {
    char *username;
    char *appname;
    uint32_t ip;
    struct _LaunchDialogQueue *prev;
    struct _LaunchDialogQueue *next;
    } LaunchDialogQueue;

GtkWidget *window = NULL;
GtkWidget *treeview = NULL;
GtkTreeStore *treestore = NULL;
int sockfd = 0;
AppsData appsdata;
User *gFirstUserNode = NULL;
User *gLastUserNode = NULL;
User *gCurrentUserNode = NULL;
LaunchAppQueue *gFirstLaunchAppQueue = NULL;
LaunchAppQueue *gLastLaunchAppQueue = NULL;
LaunchAppQueue *gCurrentLaunchAppQueue = NULL;
NewConnData *gFirstConn = NULL;
NewConnData *gLastConn = NULL;
NewConnData *gCurrentConn = NULL;
LaunchDialogQueue *gFirstLaunchDialog = NULL;
LaunchDialogQueue *gLastLaunchDialog = NULL;
LaunchDialogQueue *gCurrentLaunchDialog = NULL;
int requestid;

int init_treeview(GtkWidget *view,GtkTreeStore *treestore);
gpointer connlistener_thread(gpointer user_data);
gpointer newconnrequests_thread(gpointer user_data);
gpointer check_client_status_thread(gpointer user_data);
char* get_installed_apps(int* count,int* blocksize);
int filter(const struct dirent *dir);
int process_useronline_msg(char *buf);
int connect_to_client(uint32_t ip,int *comm_socket);
User* add_user(char* version,char* name,uint32_t ip);
User* del_user(User* deluser);
NewConnData *add_newconn(int newsockfd,struct sockaddr_in cli_addr);
LaunchDialogQueue *add_launchdialog_queue(char *username,char *appname,uint32_t ip);
int show_user_online(User *UserNode);
gboolean show_user_offline(gpointer user_data);
int get_remoteuser_apps(User *UserNode);
void send_launchapp_req(User *UserNode,char *appname);
LaunchAppQueue* add_to_launch_queue(char *appname,int ip,int reqid);
void set_launchappqueue_locked(LaunchAppQueue *dest,LaunchAppQueue *src);
void set_usernode_locked(User *destnode, User* srcnode);
int process_launchapp_req(char *buf,NewConnData *data);
gboolean launch_approve_dialog(gpointer data);
gpointer start_server(gpointer ptr_ip);
gpointer process_launchreq_accepted(gpointer user_data);
void freeusermem(void);
void freeLaunchAppQueue(void);
void freeLaunchDialogQueue(void);
void launchdlg_approved(char *appname,uint32_t ip);
void launch_approve_dialog_response(GtkWidget *dialog,gint response_id, gpointer user_data);
NewConnData *del_newconn(NewConnData *conndata);

//Signals
void on_treeview_row_activated(GtkWidget *widget,GtkTreePath *path,GtkTreeViewColumn *column,gpointer user_data);

#endif
