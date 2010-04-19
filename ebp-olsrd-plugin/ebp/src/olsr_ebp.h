

/*
 * olsr_ebp.c
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


#ifndef _OLSR_EBP_H
#define _OLSR_EBP_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <stdio.h>

#include "olsr_types.h"
#include "interfaces.h"
#include "parser.h"
#include "olsr.h"
#include "ipcalc.h"
#include "net_olsr.h"
#include "routing_table.h"
#include "mantissa.h"
#include "scheduler.h"
#include "duplicate_set.h"
#include "tc_set.h"
#include "hna_set.h"
#include "mid_set.h"
#include "link_set.h"

/* The type of message you will use */
#define MESSAGE_TYPE 128

/* The type of messages we will receive - can be set to promiscuous */
#define PARSER_TYPE MESSAGE_TYPE
#define EMISSION_INTERVAL	2     /* seconds */
#define EMISSION_JITTER         25      /* percent */
#define CLIENT_COMM_PORT	2010

struct ebp_msg
       {
       char clientname[10];
       char version[6];
       char username[20];
       int port;
       };

struct ebp_olsrmsg 
       {
       uint8_t olsr_msgtype;
       uint8_t olsr_vtime;
       uint16_t olsr_msgsize;
       uint32_t originator;
       uint8_t ttl;
       uint8_t hopcnt;
       uint16_t seqno;

       /* YOUR PACKET GOES HERE */
       char clientname[15];
       char version[6];
       char username[20];
       const char *ip;
       int port;
       };

/* periodic message generation */
struct timer_entry *msg_gen_timer = NULL;
int my_interval = EMISSION_INTERVAL;
int my_timeout = 3600; //jeetu - can be controlled by plugin params instead of being hardcoded here
static int comm_socket;
bool gUserOffline = 1;

int ebp_plugin_init(void);
void ebp_plugin_exit(void);
bool olsr_ebpmsg_parser(unsigned char *packet, struct interface *olsr_if, union olsr_ip_addr *from_addr, int *length);
void olsr_ebpmsg_gen(void *foo __attribute__ ((unused)));
int client_comm_init(void);
int connect_to_client(void);

#endif
