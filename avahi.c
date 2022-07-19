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

/*
 * Structures...
 */

/*
 * Local globals...
 */

#ifdef HAVE_MDNSRESPONDER
static void DNSSD_API resolve_callback(DNSServiceRef sdRef, DNSServiceFlags flags, uint32_t interfaceIndex, DNSServiceErrorType errorCode, const char *fullName, const char *hostTarget, uint16_t port, uint16_t txtLen, const unsigned char *txtRecord, void *context) _CUPS_NONNULL(1, 5, 6, 9, 10);
#elif defined(HAVE_AVAHI)
int poll_callback(struct pollfd *pollfds,
                         unsigned int num_pollfds, int timeout,
                         void *context);
#endif /* HAVE_MDNSRESPONDER */


#ifdef HAVE_MDNSRESPONDER
DNSServiceRef dnssd_ref;		/* Master service reference */
#elif defined(HAVE_AVAHI)
AvahiClient *avahi_client = NULL;/* Client information */
int	avahi_got_data = 0;	/* Got data from poll? */
AvahiSimplePoll *avahi_poll = NULL;
AvahiServiceBrowser *sb = NULL;
					/* Poll information */
#endif /* HAVE_MDNSRESPONDER */

// static int	address_family = AF_UNSPEC;
					/* Address family for LIST */
int	bonjour_error = 0;	/* Error browsing/resolving? */
double	bonjour_timeout = 1.0;	/* Timeout in seconds */
int	ipp_version = 20;	/* IPP version for LIST */
char* errorContext;

int err = 0;

// individual functions for browse and resolve

/*
    implementation of avahi_intialize, to create objects necessary for
    browse and resolve to work
    */

int avahi_initialize()
{
   
    /* allocate main loop object */
    if (!(avahi_poll))
    {
        avahi_poll = avahi_simple_poll_new();

        if (!avahi_poll)
        {
            fprintf(stderr, "%s: Failed to create simple poll object.\n", errorContext);
            return 0;
        }
    }

    // if(avahi_poll){
    //     fprintf(stderr, "avahi_poll is allocated first\n");
    // }

    if (avahi_poll)
        avahi_simple_poll_set_func(avahi_poll, poll_callback, NULL);

    // if(avahi_poll){
    //     fprintf(stderr, "avahi_poll is allocated second\n");
    // }

    int error;
    /* allocate a new client */
    avahi_client = avahi_client_new(avahi_simple_poll_get(avahi_poll), (AvahiClientFlags)0, client_callback, avahi_poll, &error);

    // if(avahi_poll){
    //     fprintf(stderr, "avahi_poll is allocated second\n");
    // }
    
    if (!avahi_client)
    {
        fprintf(stderr, "Initialization Error, Failed to create client: %s\n", avahi_strerror(error));
        return 0;
    }

    return 1;
}

// things to figure out yet
// 1. return type and error handling
// 2. more/specific parameters
void browse_services(char *regtype)
{

    int avahi_client_result;

    // initialize a client object
    if (!avahi_client && (avahi_client_result = avahi_initialize() == 0))
    {
        fprintf(stderr, "Initialization Error");
        return;
    }

    fprintf(stderr, "avahi_initialize is good\n");

    // we may need to change domain parameter below, currently it is default(.local)
    if (!sb)
    {
       fprintf(stderr, "before sb init\n\n");

    //    fprintf(stderr, "size of avahi_client = %d,\n", sizeof(avahi_client));
      sb = avahi_service_browser_new(avahi_client, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, regtype, NULL, (AvahiLookupFlags)0, browse_callback, NULL);
       fprintf(stderr, "after sb init\n\n");
    }

    // assuming avahi_service_browser_new returns NULL on failure
   if (!sb)
    {
        // fprintf(stderr, "client->error = %d\n\n\n", sizeof(AvahiClient));
        
    //    err = avahi_client_errno(NULL);
    }

    fprintf(stderr, "after sb check\n");
    if (err)
    {
        // fprintf(stderr, "ippfind: Unable to browse or resolve: %s",
        // dnssd_error_string(err));
        fprintf(stderr, "There was an error in browse_services\n");
        return;
    }

    fprintf(stderr, "finishing browse_services\n");
}

void resolve_services(avahi_srv_t *service)
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
    service->ref = avahi_service_resolver_new(avahi_client, AVAHI_IF_UNSPEC,
                                              AVAHI_PROTO_UNSPEC, service->name,
                                              service->regtype, service->domain,
                                              AVAHI_PROTO_UNSPEC, 0,
                                              resolve_callback, service);
    if (service->ref)
        err = 0;
    else
        err = avahi_client_errno(avahi_client);
#endif /* HAVE_MDNSRESPONDER */
}

/*
 * 'client_callback()' - Avahi client callback function.
 */

void
client_callback(
    AvahiClient *client,    /* I - Client information (unused) */
    AvahiClientState state, /* I - Current state */
    void *context)          /* I - User data (unused) */
{
    (void)client;
    (void)context;

    /*
     * If the connection drops, quit.
     */

    // if(!client){
    //     fprintf(stderr, "client_callback: 'client' parameter is NULL\n");
    // }

    fprintf(stderr, "client_callback: client_callback called\n");

    if (state == AVAHI_CLIENT_FAILURE)
    {
        fputs("DEBUG: Avahi connection failed.\n", stderr);
        bonjour_error = 1;
        avahi_simple_poll_quit(avahi_poll);
    }
}

#ifdef HAVE_AVAHI
/*
 * 'poll_callback()' - Wait for input on the specified file descriptors.
 *
 * Note: This function is needed because avahi_simple_poll_iterate is broken
 *       and always uses a timeout of 0 (!) milliseconds.
 *       (Avahi Ticket #364)
 */

int /* O - Number of file descriptors matching */
poll_callback(
    struct pollfd *pollfds,   /* I - File descriptors */
    unsigned int num_pollfds, /* I - Number of file descriptors */
    int timeout,              /* I - Timeout in milliseconds (unused) */
    void *context)            /* I - User data (unused) */
{
    int val; /* Return value */

    (void)timeout;
    (void)context;

    val = poll(pollfds, num_pollfds, 500);

    if (val > 0)
        avahi_got_data = 1;

    return (val);
}
#endif /* HAVE_AVAHI */

// static void resolve_callback(
//     AvahiServiceResolver *r,
//     AvahiIfIndex interface,
//     AvahiProtocol protocol,
//     AvahiResolverEvent event,
//     const char *name,
//     const char *type,
//     const char *domain,
//     const char *host_name,
//     const AvahiAddress *address,
//     uint16_t port,
//     AvahiStringList *txt,
//     AvahiLookupResultFlags flags,
//     void *userdata)
// {
//     assert(r);

//     /* Called whenever a service has been resolved successfully or timed out */
//     switch (event)
//     {
//     case AVAHI_RESOLVER_FAILURE:
//         fprintf(stderr, "(Resolver) Failed to resolve service '%s' of type '%s' in domain '%s': %s\n", name, type, domain, avahi_strerror(avahi_client_errno(avahi_service_resolver_get_client(r))));
//         break;
//     case AVAHI_RESOLVER_FOUND:
//     {
//         char a[AVAHI_ADDRESS_STR_MAX], *t;
//         fprintf(stderr, "(Resolver): Service '%s' of type '%s' in domain '%s':\n", name, type, domain);
//         avahi_address_snprint(a, sizeof(a), address);
//         t = avahi_string_list_to_string(txt);
//         fprintf(stderr,
//                 "\thostname = %s\n\tport = %u\n\tip-address = %s\n"
//                 "\tTXT=%s\n"
//                 "\tcookie is %u\n"
//                 "\tis_local: %i\n"
//                 "\tour_own: %i\n"
//                 "\twide_area: %i\n"
//                 "\tmulticast: %i\n"
//                 "\tcached: %i\n",
//                 host_name, port, a,
//                 t,
//                 avahi_string_list_get_service_cookie(txt),
//                 !!(flags & AVAHI_LOOKUP_RESULT_LOCAL),
//                 !!(flags & AVAHI_LOOKUP_RESULT_OUR_OWN),
//                 !!(flags & AVAHI_LOOKUP_RESULT_WIDE_AREA),
//                 !!(flags & AVAHI_LOOKUP_RESULT_MULTICAST),
//                 !!(flags & AVAHI_LOOKUP_RESULT_CACHED));
//         avahi_free(t);
//     }
//     }
//     avahi_service_resolver_free(r);
// }
// static void browse_callback(
//     AvahiServiceBrowser *b,
//     AvahiIfIndex interface,
//     AvahiProtocol protocol,
//     AvahiBrowserEvent event,
//     const char *name,
//     const char *type,
//     const char *domain,
//     AvahiLookupResultFlags flags,
//     void *userdata)
// {
//     AvahiClient *c = (AvahiClient *)userdata;
//     assert(b);
//     fprintf(stderr, "I was here inside browse_callback\n");
//     /* Called whenever a new services becomes available on the LAN or is removed from the LAN */
//     switch (event)
//     {
//     case AVAHI_BROWSER_FAILURE:
//         fprintf(stderr, "(Browser) %s\n", avahi_strerror(avahi_client_errno(avahi_service_browser_get_client(b))));
//         avahi_simple_poll_quit(avahi_poll);
//         return;
//     case AVAHI_BROWSER_NEW:
//         fprintf(stderr, "(Browser) NEW: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
//         /* We ignore the returned resolver object. In the callback
//            function we free it. If the server is terminated before
//            the callback function is called the server will free
//            the resolver for us. */

//         // SACHIN THAKAN: lets break to see what info browse callback got us
//         // break;
//         fprintf(stderr, "I was here before resolving\n");
//         if (!(avahi_service_resolver_new(c, interface, protocol, name, type, domain, AVAHI_PROTO_UNSPEC, (AvahiLookupFlags)0, resolve_callback, c)))
//             fprintf(stderr, "Failed to resolve service '%s': %s\n", name, avahi_strerror(avahi_client_errno(c)));
//         break;
//     case AVAHI_BROWSER_REMOVE:
//         fprintf(stderr, "(Browser) REMOVE: service '%s' of type '%s' in domain '%s'\n", name, type, domain);
//         break;
//     case AVAHI_BROWSER_ALL_FOR_NOW:
//     case AVAHI_BROWSER_CACHE_EXHAUSTED:
//         fprintf(stderr, "(Browser) %s\n", event == AVAHI_BROWSER_CACHE_EXHAUSTED ? "CACHE_EXHAUSTED" : "ALL_FOR_NOW");
//         break;
//     }
// }

// static void client_callback(AvahiClient *c, AvahiClientState state, void * userdata) {
//     assert(c);
//     fprintf(stderr, "client counter = %d\n\n", counter++);

//     /* Called whenever the client or server state changes */
//     if (state == AVAHI_CLIENT_FAILURE) {
//         fprintf(stderr, "Server connection failure: %s\n", avahi_strerror(avahi_client_errno(c)));
//         avahi_simple_poll_quit(avahi_poll);
//     }

//     if(state == AVAHI_CLIENT_S_RUNNING)
//         fprintf(stderr, "SACHIN THAKAN: SERVER IS RUNNING\n");

// }

/*
 * 'get_service()' - Create or update a device.
 */

// static avahi_srv_t *			/* O - Service */
// avahi_get_service(cups_array_t *services,	/* I - Service array */
// 	    const char   *serviceName,	/* I - Name of service/device */
// 	    const char   *regtype,	/* I - Type of service */
// 	    const char   *replyDomain)	/* I - Service domain */
// {
//   avahi_srv_t	key,			/* Search key */
// 		*service;		/* Service */
//   char		fullName[kDNSServiceMaxDomainName];
// 					/* Full name for query */

//  /*
//   * See if this is a new device...
//   */

//   key.name    = (char *)serviceName;
//   key.regtype = (char *)regtype;

//   for (service = cupsArrayFind(services, &key);
//        service;
//        service = cupsArrayNext(services))
//     if (_cups_strcasecmp(service->name, key.name))
//       break;
//     else if (!strcmp(service->regtype, key.regtype))
//       return (service);

//  /*
//   * Yes, add the service...
//   */

//   if ((service = calloc(sizeof(avahi_srv_t), 1)) == NULL)
//     return (NULL);

//   service->name     = strdup(serviceName);
//   service->domain   = strdup(replyDomain);
//   service->regtype  = strdup(regtype);

//   cupsArrayAdd(services, service);

//  /*
//   * Set the "full name" of this service, which is used for queries and
//   * resolves...
//   */

// #ifdef HAVE_MDNSRESPONDER
//   DNSServiceConstructFullName(fullName, serviceName, regtype, replyDomain);
// #else /* HAVE_AVAHI */
//   avahi_service_name_join(fullName, kDNSServiceMaxDomainName, serviceName,
//                           regtype, replyDomain);
// #endif /* HAVE_MDNSRESPONDER */

//   service->fullName = strdup(fullName);

//   return (service);
// }
