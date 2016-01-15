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
#include <ifaddrs.h>
#include <netdb.h>
#include <pwd.h>
#include <gtk/gtk.h>
#include <lxc/lxccontainer.h>

/** enums needed by the treeview and treestore model.
 * 
 *  These enums are used to create a UI tree with remote username,
 *  expanding the username tree shows a list of applications on that remote host. 
 */
enum
{
  NAME = 0,       //!< treestore enum to hold the remote userNAME.
  USERNODE = 1,   //!< treestore enum to hold the UserNode (User *) for the user in the linked list.
  NUM_COLS        //!< Used to set a treestore with 2 columns, one for each of the above.
};

#define WINDOW_WIDTH 460       //!< Client window width.
#define WINDOW_HEIGHT 300      //!< Client widow height.
#define CLIENT_COMM_PORT 2010  //!< Port no. on which this server listens on. TODO: should eventually be set in ebp.conf file.

//-----------------
// Structures
//-----------------

/** Linked list structure to hold details of the new connection to this host.
 *
 */
typedef struct _newconndata
    {
    int newsockfd;               //!< socket file descriptor for the new connection returned by accept(). 
    char buffer[310];            //!< buffer (currently unused) TODO: take this off.
    uint32_t ip;                 //!< IPv4 address of the peer connecting in.
    struct _newconndata *prev;   //!< linked list pointer pointing to previous item in list.
    struct _newconndata *next;   //!< linked list pointer pointing to next item in list.
    } NewConnData;


/** Structure to hold list of applications on the current system.
 *
 */ 
typedef struct
    {
    char *apps;       //!< char * buffer to hold applications in the format [app1 executable]:[app2 executable]:...
    int count;        //!< no. of applications.
    int blocksize;    //!< size in bytes of the apps buffer.
    } AppsData;


/** Linked list structure to hold details of other users discovered.
 *
 *  TODO: Verify buffer sizes of the members or do away with hard-coded buffer sizes totally.
 */
typedef struct _user 
    {
    char name[20];               //!< username or alias set in ebp.conf as would be displayed to other users.
    char ssh_login_user[257];    //!< username ssh clients need to login to this host.
    char version[6];             //!< client version information (currently not used).
    uint32_t ip;                 //!< IPv4 address of this user.
    char *apps_buffer;           //!< buffer to hold list of applications on this remote host.
    unsigned int noofapps;       //!< No. of applications on this remote host.
    struct _user *prev;          //!< linked list pointer pointing to previous item in list.
    struct _user *next;	         //!< linked list pointer pointing to next item in list.
    } User;


/** Linked list to hold list of app launch requests sent by our client.
 *
 *  This will (supposedly) be useful in verifying that a response received is
 *  for a valid LaunchApp request that was initiated by this client.
 */
typedef struct _LaunchAppQueue
    {
    gchar appname[300];             //!< application name requested to be launched.
    uint32_t ip;                    //!< ip address of the remote host request has been sent to.
    int reqid;                      //!< request id.
    struct _LaunchAppQueue *prev;   //!< linked list pointer pointing to previous item in list.
    struct _LaunchAppQueue *next;   //!< linked list pointer pointing to next item in list.
    } LaunchAppQueue;


/** Linked list to hold details of requests requests from other hosts to launch apps from this host.
 *
 *  From what I understand (can't remember exactly why I did this ;) ), however apparently GTK+
 *  elements such as dialogs should be handled in the same thread as the initializing thread,
 *  usually in main(). Therefore data from other threads needs to be passed back to this main 
 *  thread wherein the dialog is presented to the user. This cross-thread data exchange is
 *  facilated by this structure.
 *
 *  TODO: Verify buffer sizes of the members or do away with hard-coded buffer sizes totally.
 */
typedef struct _LaunchDialogQueue
    {
    char username[100];                    //!< username of remote client (as set in their ebp.conf) sending the request.
    char appname[100];                     //!< application name on this host which the remote user wishes to run.
    uint32_t ip;                           //!< IPv4 address of remote host connecting in.
    struct _LaunchDialogQueue *prev;       //!< linked list pointer pointing to previous item in list.
    struct _LaunchDialogQueue *next;       //!< linked list pointer pointing to next item in list.
    } LaunchDialogQueue;

//--------------------
// Global Variables
//--------------------

GtkWidget *window;                           //!< Pointer to the main top-level window.
GtkWidget *treeview;                         
GtkTreeStore *treestore;
int sockfd;                                  //!< Global socket file descriptor,set in connlistener_thread() and closed on exit in main().
AppsData appsdata;
//User *gFirstUserNode;                        //!< global pointer to first element in the UserNode linked list.
//User *gLastUserNode;                         //!< global pointer to last element in the UserNode linked list.
//User *gCurrentUserNode;                      //!< global pointer to current element in the UserNode linked list.

User *gUserListHead;

LaunchAppQueue *gFirstLaunchAppQueue;        //!< global pointer to first element in the LaunchAppQueue linked list.  
LaunchAppQueue *gLastLaunchAppQueue;         //!< global pointer to last element in the LaunchAppQueue linked list.  
LaunchAppQueue *gCurrentLaunchAppQueue;      //!< global pointer to current element in the LaunchAppQueue linked list.  
NewConnData *gFirstConn;                     //!< global pointer to first element in the NewConnData linked list.  
NewConnData *gLastConn;                      //!< global pointer to last element in the NewConnData linked list.  
NewConnData *gCurrentConn;                   //!< global pointer to current element in the NewConnData linked list.  
LaunchDialogQueue *gFirstLaunchDialog;       //!< global pointer to first element in the LaunchDialogQueue linked list.  
LaunchDialogQueue *gLastLaunchDialog;        //!< global pointer to last element in the LaunchDialogQueue linked list.  
LaunchDialogQueue *gCurrentLaunchDialog;     //!< global pointer to current element in the LaunchDialogQueue linked list.  
int requestid;
struct ifaddrs *ifaddr;                      //!< details of network interfaces on current system (localhost).
struct lxc_container *container;             //!< structure of the lxc container used as the isolation sandbox.

//config file values
gchar *config_entry_username;                //!< username as defined in from ebp.conf
gchar *config_container_ip;                  //!< ipv4 address of lxc container on the host as read in from ebp.conf
gchar *config_container_sshd_port;           //!< port sshd listens on in the lxc container as read in from ebp.conf
gchar *config_host_forwarded_port;           //!< port on the host that is forwarded to the sshd on container as read in from ebp.conf
gchar *config_container_ssh_user;            //!< username in container the remote hosts ssh needs to login to as read in from ebp.conf
gchar *config_connectivity_protocol;         //!< protocol to use to connect to remote host - ssh-x, x2go

int childpid;                                //!< used by pipe_to_program() when it forks and launches piped OpenSSH server/client,etc.
struct passwd *ssh_login_userdetails;        //!< details of user launching this client,remote ssh clients need to login back to this user.

//-----------------------
// Function Declarations
//-----------------------

int init_treeview(GtkWidget *view,GtkTreeStore *treestore);
char* get_installed_apps(int* count,int* blocksize);
int filter(const struct dirent *dir);
int process_useronline_msg(char *buf);
int connect_to_client(uint32_t ip,int *comm_socket);
//User* add_user(const char* version,const char* name,const char *ssh_login_user,uint32_t ip);
//User* del_user(User* deluser);
User* add_user(User *head,const char* version,const char* name,const char *ssh_login_user,uint32_t ip);
User* del_user(User **head,User* deluser);
NewConnData *add_newconn(int newsockfd,uint32_t ip);
LaunchDialogQueue *add_launchdialog_queue(char *username,char *appname,uint32_t ip);
int show_user_online(User *UserNode);
gboolean show_user_offline(gpointer user_data);
int get_remoteuser_apps(User *UserNode);
void send_launchapp_req(User *UserNode,char *appname);
LaunchAppQueue* add_to_launch_queue(char *appname,uint32_t ip,int reqid);
void set_launchappqueue_locked(LaunchAppQueue *dest,LaunchAppQueue *src);
int process_launchapp_req(char *buf,NewConnData *data);
gboolean launch_approve_dialog(gpointer data);
int process_launchreq_accepted(NewConnData *data);
int launch_using_sshx(NewConnData *data);
int launch_using_x2go(NewConnData *data);
void freeusermem(void);
void freeLaunchAppQueue(void);
void freeLaunchDialogQueue(void);
void launchdlg_approved(char *appname,uint32_t ip);
void launch_approve_dialog_response(GtkWidget *dialog,gint response_id, gpointer user_data);
NewConnData *del_newconn(NewConnData *conndata);
int getlocaladdrs();
int process_useronline_avahi_msg(const char *ip, const char *username, const char *ssh_login_user, const char *version);
void pipe_to_program(char *path, char **args, int *in, int *out,int *err);
void killchild(int signo);
int readconfigfile(void);
int launch_container(void);
int stop_container(void);

//----------
//Threads
//----------

gpointer connlistener_thread(gpointer user_data);
gpointer newconnrequests_thread(gpointer user_data);
gpointer check_client_status_thread(gpointer user_data);
gpointer create_port_to_container_thread(gpointer user_data);
gpointer start_server(gpointer ptr_ip);

//----------
//Signals
//----------

void on_treeview_row_activated(GtkWidget *widget,GtkTreePath *path,GtkTreeViewColumn *column,gpointer user_data);

#endif
