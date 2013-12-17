/*
 * avahi-discovery.c contains main().  
 * Uses Avahi to publish and discover the availability of eBrainPool clients on the network.
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
#include "avahi-discovery.h"

/** Initializes the avahi system, broadcasts this client and prepares to receive notifications on other clients on the LAN.
 *
 *  Currently sets up this client and listens for other clients broadcasting the _presence._tcp service type. 
 *  This is temporary till we have our own service type registered with IANA.
 *
 *  called by main()
 *
 *  returns 0 on error and 1 on success.
 */
int avahi_setup(void)
{
    int error;

    client = NULL;
    threaded_poll = NULL;
    avahi_name = NULL;
    group = NULL;

    //! Allocates main loop object. 
    if(!(threaded_poll = avahi_threaded_poll_new())) 
      {
      fprintf(stderr, "Failed to create simple poll object.\n");
      avahi_cleanup();
      return 0;
      }

    avahi_name = avahi_strdup(config_entry_username);

    //! Allocates a new client. 
    client = avahi_client_new(avahi_threaded_poll_get(threaded_poll), 0, client_callback, NULL, &error);

    // Check whether creating the client object succeeded
    if(!client) 
      {
      fprintf(stderr, "Failed to create client: %s\n", avahi_strerror(error));
      avahi_cleanup();
      return 0;
      }

    //! Create the service browser.
    if(!(sb = avahi_service_browser_new(client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, "_presence._tcp", NULL, 0, browse_callback, client))) 
      {
      fprintf(stderr, "Failed to create service browser: %s\n", avahi_strerror(avahi_client_errno(client)));
      avahi_cleanup();
      }

    //! Finally starts the main event loop thread.
    if(avahi_threaded_poll_start(threaded_poll) < 0) 
      {
      avahi_cleanup();
      return 0;
      }
 
    return 1;
}

/** Callback function setup while allocating a new avahi client.
 *
 *  callback setup by avahi_client_new() in avahi_setup().
 *
 *  Handles the following states:
 *  - AVAHI_CLIENT_S_RUNNING
 *  - AVAHI_CLIENT_FAILURE
 *  - AVAHI_CLIENT_S_COLLISION
 *  - AVAHI_CLIENT_S_REGISTERING
 *  - AVAHI_CLIENT_CONNECTING
 */
void client_callback(AvahiClient *c, AvahiClientState state, AVAHI_GCC_UNUSED void * userdata) 
{       
    assert(c);

    //! Called whenever the client or server state changes

    switch(state) 
          {
          case AVAHI_CLIENT_S_RUNNING:

              /* The server has startup successfully and registered its host
               * name on the network, so it's time to create our services */
              create_services(c);
              break;

          case AVAHI_CLIENT_FAILURE:

              fprintf(stderr, "Client failure: %s\n", avahi_strerror(avahi_client_errno(c)));
              avahi_threaded_poll_quit(threaded_poll);

              break;

          case AVAHI_CLIENT_S_COLLISION:

              /* Let's drop our registered services. When the server is back
               * in AVAHI_SERVER_RUNNING state we will register them
               * again with the new host name. */

          case AVAHI_CLIENT_S_REGISTERING:

              /* The server records are now being established. This
               * might be caused by a host name change. We need to wait
               * for our own records to register until the host name is
               * properly esatblished. */

              if(group)
                avahi_entry_group_reset(group);

              break;

          case AVAHI_CLIENT_CONNECTING:
              ;
          }
}


/** Creates a new entry group and service for this client.
 *
 *  Called in client_callback() in response to the AVAHI_CLIENT_S_RUNNING state.
 *  
 *  returns 0 on error and 1 on success.
 */
int create_services(AvahiClient *c) 
{

    char *n, ssh_login_username[270];
    int ret;
    assert(c);

    //! If this is the first time we're called, let's create a new
    //! entry group if necessary. 

    if(!group)
      {
      if(!(group = avahi_entry_group_new(c, entry_group_callback, NULL))) 
        {
        fprintf(stderr, "avahi_entry_group_new() failed: %s\n", avahi_strerror(avahi_client_errno(c)));
        avahi_threaded_poll_quit(threaded_poll);
        return 0;
        }
      }

    //! If the group is empty (either because it was just created, or
    //! because it was reset previously, add our entries. 

    if(avahi_entry_group_is_empty(group)) 
      {
      fprintf(stderr, "Adding service '%s'\n", avahi_name);

      //! Adds the ssh login name that is used by the ssh client to connect back to the remote host.
      snprintf(ssh_login_username, sizeof(ssh_login_username)+11, "ssh_login=%s", ssh_login_userdetails->pw_name);

      /* We will now add two services and one subtype to the entry
       * group. The two services have the same name, but differ in
       * the service type (IPP vs. BSD LPR). Only services with the
       * same name should be put in the same entry group. */

      //! TODO: Currently using the service type presence; this is temporary till we find an appropriate service type.

      //! Currently broadcasting only over IPv4 (INET).

      if((ret = avahi_entry_group_add_service(group, AVAHI_IF_UNSPEC, AVAHI_PROTO_INET, 0, avahi_name, "_presence._tcp", NULL, NULL, 651, ssh_login_username, NULL)) < 0)      
        {
        if(ret == AVAHI_ERR_COLLISION)
          {
          /* A service name collision with a local service happened. Let's
           * pick a new name */
          n = avahi_alternative_service_name(avahi_name);
          avahi_free(avahi_name);
          avahi_name = n;

          fprintf(stderr, "Service name collision, renaming service to '%s'\n", avahi_name);

          avahi_entry_group_reset(group);

          create_services(c);
          return 0;
          }

        fprintf(stderr, "Failed to add _presence._tcp service: %s\n", avahi_strerror(ret));
        avahi_threaded_poll_quit(threaded_poll);
        return 0;
        }

      //! Tells the server to register the service.
      if((ret = avahi_entry_group_commit(group)) < 0) 
        {
        fprintf(stderr, "Failed to commit entry group: %s\n", avahi_strerror(ret));
        avahi_threaded_poll_quit(threaded_poll);
        return 0;
        }
      }

    return 1;
}



/** Creates a new entry group for this client. 
 *
 *  called by create_services()
 *  
 *  Handles the states:
 *  - AVAHI_ENTRY_GROUP_ESTABLISHED
 *  - AVAHI_ENTRY_GROUP_COLLISION
 *  - AVAHI_ENTRY_GROUP_FAILURE
 *  - AVAHI_ENTRY_GROUP_UNCOMMITED
 *  - AVAHI_ENTRY_GROUP_REGISTERING
 */
void entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state, AVAHI_GCC_UNUSED void *userdata) 
{
    char *n;

    assert(g == group || group == NULL);
    group = g;

    /* Called whenever the entry group state changes */

    switch(state) 
          {
          case AVAHI_ENTRY_GROUP_ESTABLISHED :
              /* The entry group has been established successfully */
              fprintf(stderr, "Service '%s' successfully established.\n", avahi_name);
              break;

          case AVAHI_ENTRY_GROUP_COLLISION : 
              /* A service name collision with a remote service
               * happened. Let's pick a new name */
              n = avahi_alternative_service_name(avahi_name);
              avahi_free(avahi_name);
              avahi_name = n;

              fprintf(stderr, "Service name collision, renaming service to '%s'\n", avahi_name);

              /* And recreate the services */
              create_services(avahi_entry_group_get_client(g));
              break;

          case AVAHI_ENTRY_GROUP_FAILURE :

              fprintf(stderr, "Entry group failure: %s\n", avahi_strerror(avahi_client_errno(avahi_entry_group_get_client(g))));

              /* Some kind of failure happened while we were registering our services */
              avahi_threaded_poll_quit(threaded_poll);
              break;

          case AVAHI_ENTRY_GROUP_UNCOMMITED:
          case AVAHI_ENTRY_GROUP_REGISTERING:
              ;
          }
}



/** Callback registered by avahi_service_browser_new() at the time of creating a new service browser.
 *
 *  called by avahi_setup()
 *
 *  Handles the browser events :
 *  - AVAHI_BROWSER_FAILURE
 *  - AVAHI_BROWSER_NEW
 *  - AVAHI_BROWSER_REMOVE
 *  - AVAHI_BROWSER_ALL_FOR_NOW
 *  - AVAHI_BROWSER_CACHE_EXHAUSTED
 */ 
void browse_callback(
    AvahiServiceBrowser *b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char *name,
    const char *type,
    const char *domain,
    AVAHI_GCC_UNUSED AvahiLookupResultFlags flags,
    void* userdata) 
{

    AvahiClient *c = userdata;
    assert(b);

    //! Called whenever a new services becomes available on the LAN or is removed from the LAN

    switch(event) 
          {
          case AVAHI_BROWSER_FAILURE:

              fprintf(stderr, "(Browser) %s\n", avahi_strerror(avahi_client_errno(avahi_service_browser_get_client(b))));
              avahi_threaded_poll_quit(threaded_poll);
              return;

          case AVAHI_BROWSER_NEW:
              fprintf(stderr, "(Browser) NEW: service '%s' of type '%s' in domain '%s'\n", name, type, domain);

              /* We ignore the returned resolver object. In the callback
                 function we free it. If the server is terminated before
                 the callback function is called the server will free
                 the resolver for us. */

              if(!(avahi_service_resolver_new(c, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, 0, resolve_callback, c)))
                fprintf(stderr, "Failed to resolve service '%s': %s\n", name, avahi_strerror(avahi_client_errno(c)));

              break;

          case AVAHI_BROWSER_REMOVE:
              fprintf(stderr, "(Browser) REMOVE: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
              break;

          case AVAHI_BROWSER_ALL_FOR_NOW:
          case AVAHI_BROWSER_CACHE_EXHAUSTED:
              fprintf(stderr, "(Browser) %s\n", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
              break;
          }
}


/** Callback called whenever a service is resolved or times out.  
 *
 *  Callback setup by avahi_service_resolver_new() in browse_callback()
 *
 *  Handles the events :
 *  - AVAHI_RESOLVER_FAILURE
 *  - AVAHI_RESOLVER_FOUND
 */
void resolve_callback(
    AvahiServiceResolver *r,
    AVAHI_GCC_UNUSED AvahiIfIndex interface,
    AVAHI_GCC_UNUSED AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char *name,
    const char *type,
    const char *domain,
    const char *host_name,
    const AvahiAddress *address,
    uint16_t port,
    AvahiStringList *txt,
    AvahiLookupResultFlags flags,
    AVAHI_GCC_UNUSED void* userdata) 
{
    char a[AVAHI_ADDRESS_STR_MAX], *strtxt;

    assert(r);

    /* Called whenever a service has been resolved successfully or timed out */

    switch(event) 
          {
          case AVAHI_RESOLVER_FAILURE:
              fprintf(stderr, "(Resolver) Failed to resolve service '%s' of type '%s' in domain '%s': %s\n", name, type, domain, avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r))));
              break;

          case AVAHI_RESOLVER_FOUND:              

              fprintf(stderr, "Service '%s' of type '%s' in domain '%s':\n", name, type, domain);

              avahi_address_snprint(a, sizeof(a), address);
              strtxt = avahi_string_list_to_string(txt);
              printf("\nresolve_callback: strtxt = %s\n",strtxt);

              avahi_resolver_found(a,name,strtxt);
/*              fprintf(stderr,
                      "\t%s:%u (%s)\n"
                      "\tTXT=%s\n"
                      "\tcookie is %u\n"
                      "\tis_local: %i\n"
                      "\tour_own: %i\n"
                      "\twide_area: %i\n"
                      "\tmulticast: %i\n"
                      "\tcached: %i\n",
                      host_name, port, a,
                      t,
                      avahi_string_list_get_service_cookie(txt),
                      !!(flags & AVAHI_LOOKUP_RESULT_LOCAL),
                      !!(flags & AVAHI_LOOKUP_RESULT_OUR_OWN),
                      !!(flags & AVAHI_LOOKUP_RESULT_WIDE_AREA),
                      !!(flags & AVAHI_LOOKUP_RESULT_MULTICAST),
                      !!(flags & AVAHI_LOOKUP_RESULT_CACHED));
*/
              avahi_free(strtxt);
          }

    avahi_service_resolver_free(r);
}



/** Determines that the host found isn't on localhost and then processes it as a new user.
 *
 *  called by resolve_callback()
 */
int avahi_resolver_found(const char *address, const char *name,const char *txt)
{

    struct ifaddrs *ifa;
    int family, s;
    char host[NI_MAXHOST];
    int localaddr_check = -1;
    char *ssh_login_user = NULL;
    char str1[257];
    char *saveptr1 = NULL;

    printf("\navahi_resolver_found: address = %s\n",address);

    //! Loops through the interfaces on localhost and compares with the address of the
    //! new host resolved.
    for(ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) 
       {
       if(ifa->ifa_addr == NULL)
          continue; 

       family = ifa->ifa_addr->sa_family;

       /* Display interface name and family (including symbolic
          form of the latter for the common families) */

//       printf("%s  address family: %d%s\n",
//             ifa->ifa_name, family,
//             (family == AF_PACKET) ? " (AF_PACKET)" :
//             (family == AF_INET) ?   " (AF_INET)" :
//             (family == AF_INET6) ?  " (AF_INET6)" : "");
       /* For an AF_INET* interface address, display the address */

       /* for now services are only broadcast over IPv4 (INET) so we don't need to deal with AF_INET6 addresses */
       if(family == AF_INET) 
         {
         s = getnameinfo(ifa->ifa_addr,
                         (family == AF_INET) ? sizeof(struct sockaddr_in) :
                                               sizeof(struct sockaddr_in6),
         host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
         if(s != 0) 
           {
           printf("getnameinfo() failed: %s\n", gai_strerror(s));
           return 0;
           }
//         printf("\thost address: <%s> service address = %s\n", host,address);
         
         if(strcmp(address,host) == 0)
           {
           //! flag localaddr_check == 0 match found;service resides on local host
           localaddr_check = 0;
           break;         
           }
         }
       }

    //! flag localaddr_check == -1 match not found;service is unique and does not reside on local host
    if(localaddr_check == -1)
      {
      sscanf(txt,"\"ssh_login=%s",&str1[0]);
      ssh_login_user = strtok_r(str1, "\"", &saveptr1);
      printf("\nUnique address found %s str1= %s ssh_login_user = %s\n",address,str1,ssh_login_user);
      process_useronline_avahi_msg(address,name,ssh_login_user,"v0.3");
      }

    return 1;
}




/** Called to free avahi related stuff and cleanup the avahi client,service browser and the polling thread.
 *
 *  called by avahi_setup()
 *  TODO: This function should probably be called when the main program quits too.
 */
void avahi_cleanup(void)
{
    /* Cleanup things */

    if(sb)
      avahi_service_browser_free(sb);

    if(client)
      avahi_client_free(client);

    if(threaded_poll)
      avahi_threaded_poll_free(threaded_poll);

    avahi_free(avahi_name);
}
