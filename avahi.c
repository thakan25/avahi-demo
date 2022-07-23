/*
 * implementation file using avahi discovery service APIS
 *
 * Copyright © 2021-2022 by OpenPrinting.
 * Copyright © 2020 by the IEEE-ISTO Printer Working Group
 * Copyright © 2008-2018 by Apple Inc.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers.
 */
#include "avahi.h"

// individual functions for browse and resolve

/*
    implementation of avahi_intialize, to create objects necessary for
    browse and resolve to work
    */

int avahi_initialize(AvahiSimplePoll **avahi_poll, AvahiClient **avahi_client, void (*client_callback)(AvahiClient*, AvahiClientState, void *), int *err)
{
   
    /* allocate main loop object */
    if (!(*avahi_poll))
    {
        avahi_poll = avahi_simple_poll_new();

        if (!(*avahi_poll))
        {
            fprintf(stderr, "%s: Failed to create simple poll object.\n");
            return 0;
        }
    }

    if (*avahi_poll)
        avahi_simple_poll_set_func(avahi_poll, poll_callback, NULL);

    int error;
    /* allocate a new client */
    *avahi_client = avahi_client_new(avahi_simple_poll_get(*avahi_poll), (AvahiClientFlags)0, *client_callback, *avahi_poll, err);

    if (!(*avahi_client))
    {
        fprintf(stderr, "Initialization Error, Failed to create client: %s\n", avahi_strerror(error));
        return 0;
    }

    return 1;
}

// things to figure out yet
// 1. return type and error handling
// 2. more/specific parameters
void browse_services(AvahiSimplePoll **avahi_poll, AvahiClient **avahi_client, AvahiServiceBrowser **sb, char *regtype, int *err)
{

    int avahi_client_result = 0;

    // we may need to change domain parameter below, currently it is default(.local)
    if (!(*sb))
    {
      *sb = avahi_service_browser_new(*avahi_client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, regtype, NULL, (AvahiLookupFlags)0, browse_callback, NULL);
    }

    // assuming avahi_service_browser_new returns NULL on failure
   if (!(*sb))
    {   
       *err = avahi_client_errno(*avahi_client);
    }

    fprintf(stderr, "finishing browse_services\n");
}

void resolve_services(AvahiClient **avahi_client, avahi_srv_t *service, int *err)
{
    if (getenv("IPPFIND_DEBUG"))
        fprintf(stderr, "Resolving name=\"%s\", regtype=\"%s\", domain=\"%s\"\n", service->name, service->regtype, service->domain);

#ifdef HAVE_MDNSRESPONDER
    service->ref = dnssd_ref;
    err = DNSServiceResolve(&(service->ref),
                            kDNSServiceFlagsShareConnection, 0, service->name,
                            service->regtype, service->domain, resolve_callback,
                            service);

#elif defined(HAVE_AVAHI)
    service->ref = avahi_service_resolver_new(*avahi_client, AVAHI_IF_UNSPEC,
                                              AVAHI_PROTO_UNSPEC, service->name,
                                              service->regtype, service->domain,
                                              AVAHI_PROTO_UNSPEC, 0,
                                              resolve_callback, service);
    if (service->ref)
        *err = 0;
    else
        *err = avahi_client_errno(avahi_client);
#endif /* HAVE_MDNSRESPONDER */
}

void dummy(){
    fprintf(stderr, "This is dummy function\n");
}

// /*
//  * 'client_callback()' - Avahi client callback function.
//  */

// void
// client_callback(
//     AvahiClient *client,    /* I - Client information (unused) */
//     AvahiPoll **avahi_poll,
//     AvahiClientState state, /* I - Current state */
//     void *context)          /* I - User data (unused) */
// {
//     (void)client;
//     (void)context;

//     /*
//      * If the connection drops, quit.
//      */

//     if (state == AVAHI_CLIENT_FAILURE)
//     {
//         fputs("DEBUG: Avahi connection failed.\n", stderr);
//         //bonjour_error = 1;
//         avahi_simple_poll_quit(*avahi_poll);
//     }
// }

// #ifdef HAVE_AVAHI
// /*
//  * 'poll_callback()' - Wait for input on the specified file descriptors.
//  *
//  * Note: This function is needed because avahi_simple_poll_iterate is broken
//  *       and always uses a timeout of 0 (!) milliseconds.
//  *       (Avahi Ticket #364)
//  */

// int /* O - Number of file descriptors matching */
// poll_callback(
//     struct pollfd *pollfds,   /* I - File descriptors */
//     unsigned int num_pollfds, /* I - Number of file descriptors */
//     int timeout,              /* I - Timeout in milliseconds (unused) */
//     void *context)            /* I - User data (unused) */
// {
//     int val; /* Return value */

//     (void)timeout;
//     (void)context;

//     val = poll(pollfds, num_pollfds, 500);

//     if (val > 0)
//         avahi_got_data = 1;

//     return (val);
// }
// #endif /* HAVE_AVAHI */

