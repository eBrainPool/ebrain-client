/*
 * ebp-moblin.h
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

#ifndef _EBP_MOBLIN_H
#define _EBP_MOBLIN_H

#define STAGE_WIDTH 260
#define STAGE_HEIGHT 380
#define SCROLLVIEW_XPOS 10
#define SCROLLVIEW_YPOS 80
#define SCROLLVIEW_WIDTH 220
#define SCROLLVIEW_HEIGHT 240
#define EXPANDER_GAP 10

#define CLIENT_COMM_PORT 2010

struct user 
    {
    unsigned int xpos;
    unsigned int ypos;
    gfloat height;
    gfloat width;
    int index;
    char name[20];
    char version[6];
    uint32_t ip;
    unsigned int noofapps;
    MxWidget *expander; 
    MxWidget *scroll;
    MxWidget *grid;
    struct user *prev;
    struct user *next;	
    };
	
typedef struct
    {
    ClutterActor *stage;
    MxWidget *box;
    int sockfd;
    int newsockfd;
    struct sockaddr_in cli_addr;
    char buffer[300];
    } ListenerThreadData;
	
typedef struct
    {
    ListenerThreadData *uidata;
    struct user* offlineuser;
    } OfflineUserData;
	
typedef struct
    {
    char *apps;
    int count;
    int blocksize;
    } AppsData;

struct LaunchAppQueue
    {
    gchar appname[300];
    int ip;
    int reqid;
    struct LaunchAppQueue *prev;
    struct LaunchAppQueue *next;
    };
	
typedef struct
    {
    ClutterActor *dialog;
    char appname[300];
    uint32_t ip;
    } LaunchApprDlgData;
	
struct user *gFirstUserNode = NULL;
struct user *gCurrentUserNode = NULL;
struct user *gLastUserNode = NULL;
static GStaticPrivate thread_data = G_STATIC_PRIVATE_INIT;
static GStaticPrivate listener_child_data = G_STATIC_PRIVATE_INIT;
AppsData appsdata;
struct LaunchAppQueue *gFirstLaunchAppQueue = NULL;
struct LaunchAppQueue *gLastLaunchAppQueue = NULL;
struct LaunchAppQueue *gCurrentLaunchAppQueue = NULL;
int requestid;

static void
expand_complete_cb (MxExpander  *expander,
                    struct user* UserNode);

static void send_launchapp_req(MxButton *button,struct user* UserNode);                      
int show_user_online(ListenerThreadData* data,struct user *UserNode);
struct user* add_user(int index, char* version,char* name,uint32_t ip);
struct user* del_user(struct user* deluser);
static gpointer listener(gpointer user_data);
static gpointer listener_child(gpointer user_data);
static ListenerThreadData *thread_data_new (void);
static gboolean process_useronline_msg (gpointer user_data);
static gpointer check_client_status(gpointer user_data);
int connect_to_client(uint32_t ip,int *comm_socket);
static gboolean show_user_offline(gpointer user_data);
char* list_installed_apps(int* count,int* blocksize);
int filter(const struct dirent *dir);
static gboolean process_launchapp_req (gpointer user_data);
static gpointer start_server(gpointer user_data);
gboolean launch_approve_dialog(char *name,char *appname, uint32_t ip);
static void launchdlg_approved(MxButton *button,LaunchApprDlgData* data);
static void launchdlg_rejected(MxButton *button,LaunchApprDlgData* data);
struct LaunchAppQueue* add_to_launch_queue(char *appname,int ip,int requestid);
static gpointer LaunchAppFromHost(gpointer user_data);
void freeusermem(void);
void freeLaunchAppQueue(void);

#endif
