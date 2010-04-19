
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include "defs.h"
#include "ipcalc.h"
#include "olsr.h"
#include "socket_parser.h"
#include "parser.h"
#include "scheduler.h"
#include "net_olsr.h"

#include "olsr_ebp.h"
#include "olsr_ebp_common.h"


int ebp_plugin_init(void)
{
    int comm_connected;

    comm_connected = client_comm_init();

    /* register functions with olsrd */
    olsr_parser_add_function((parse_function *) &olsr_ebpmsg_parser, PARSER_TYPE);

    /* periodic message generation */
    msg_gen_timer = olsr_start_timer(my_interval * MSEC_PER_SEC, EMISSION_JITTER, OLSR_TIMER_PERIODIC, &olsr_ebpmsg_gen, NULL, 0);

    return 1;
}


/**
 * Scheduled event: generate and send ebp packet
 */
void olsr_ebpmsg_gen(void *foo __attribute__ ((unused)))
{
    struct interface *ifn;
    struct ebp_olsrmsg msg;

    /* Fill packet header */
    msg.olsr_msgtype = MESSAGE_TYPE;
    msg.olsr_vtime = reltime_to_me(my_timeout * MSEC_PER_SEC);
    memcpy(&msg.originator, &olsr_cnf->main_addr, olsr_cnf->ipsize);
    msg.ttl = MAX_TTL;
    msg.hopcnt = 0;
    msg.seqno = htons(get_msg_seqno());
  
    strncpy(msg.clientname,"eBrainOnline",13);
    strncpy(msg.version,"v1",6);
    strncpy(msg.username,client_username,20);
    msg.port = htons(2010);
    msg.olsr_msgsize = htons(sizeof(struct ebp_olsrmsg));

    /* looping through interfaces */
    for(ifn = ifnet; ifn; ifn = ifn->int_next) 
       {
       OLSR_PRINTF(3, "ebp PLUGIN: Generating packet - [%s]\n", ifn->int_name);

       if(net_outbuffer_push(ifn, &msg, sizeof(struct ebp_olsrmsg)) != sizeof(struct ebp_olsrmsg)) 
         {
         /* send data and try again */
         net_output(ifn);
         if(net_outbuffer_push(ifn, &msg, sizeof(struct ebp_olsrmsg)) != sizeof(struct ebp_olsrmsg)) 
           {
           OLSR_PRINTF(3, "ebp PLUGIN: could not send on interface: %s\n", ifn->int_name);
           }
         }
     }
}

bool olsr_ebpmsg_parser(unsigned char *packet, struct interface *olsr_if, union olsr_ip_addr *from_addr, int *length)
{
    struct ebp_olsrmsg *msg;
    union olsr_ip_addr originator;
    olsr_reltime vtime;
    int size;
    uint16_t seqno;
    char buf[256];
    struct ipaddr_str buf1;
    int comm_connected;

    msg = (struct ebp_olsrmsg *)(ARM_NOWARN_ALIGN)packet;
    /* Fetch the originator of the messsage */
    memcpy(&originator, &msg->originator, olsr_cnf->ipsize);
    seqno = ntohs(msg->seqno);
    	
    vtime = me_to_reltime(msg->olsr_vtime);
    size = ntohs(msg->olsr_msgsize);

    /* Check if message originated from this node.
       If so - back off */
    if(ipequal(&originator, &olsr_cnf->main_addr))
      return false;

    /* Check that the neighbor this message was received from is symmetric.
       If not - back off */
    if(check_neighbor_link(from_addr) != SYM_LINK) 
      {
      struct ipaddr_str strbuf;
      OLSR_PRINTF(3, "eBP PLUGIN: Received msg from NON SYM neighbor %s\n", olsr_ip_to_string(&strbuf, from_addr));
      return false;
      }	
	
    msg->ip = olsr_ip_to_string(&buf1,(union olsr_ip_addr *)&originator);
    sprintf(buf,"%s:Version==%s:Username==%s:IP==%d:",msg->clientname,msg->version,msg->username,originator.v4.s_addr);
    comm_connected = connect_to_client();
    if(comm_connected == 0)
      {
      send(comm_socket,buf,strlen(buf),MSG_NOSIGNAL);	
      close(comm_socket);
      }

    /* Forward the message */
    return true;
}


int client_comm_init(void)
{
    return 0;
}

int connect_to_client(void)
{
    struct sockaddr_in s_in;
    int ret;

    comm_socket = socket(AF_INET,SOCK_STREAM, 0);

    memset(&s_in,0,sizeof(s_in));
    s_in.sin_family = AF_INET;
    s_in.sin_port = htons(CLIENT_COMM_PORT);
    s_in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    ret = connect(comm_socket,(struct sockaddr *) &s_in,sizeof(s_in));

    return(ret);
}

void ebp_plugin_exit(void)
{	

}

