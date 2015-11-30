#include "dproxy.h"

/*
 * Add a request to dns_request_list.
 * RETURNS: 1 for success, 0 for failure 
 */
int dns_list_add(dns_request_t *r);

/*
 * Scans dns_request_list and compare the id field
 * of each node with that of the id fields of 'r'.
 * RETURNS: pointer to the first node that matches else NULL.
 */
dns_request_t *dns_list_find_by_id(dns_request_t *r);


/*
 * BRCM: Turn off the switch_on_timeout flag on all requests after this one.
 * We do this in case the previous request or response was lost.
 * But since we have received a response for this request, the dns
 * server is obviously up.
 * We could be even more proactive and retransmit all these requests...
 */
void dns_list_unarm_requests_after_this(const dns_request_t *r);


/*
 * Removes and frees the node pointed to by r
 * RETURNS: 1 for success, 0 for failure
 */
int dns_list_remove(dns_request_t *r);

/*
 * Print out dns_request_list for debuging purposes
 */
void dns_list_print(void);

/*
 * Return the how many seconds left to expire the oldest request
 * RETURNS: seconds
 */
int dns_list_next_time(void);

extern dns_request_t *dns_request_list;
/* Last request in dns_request_list */
extern dns_request_t *dns_request_last;
