#include <stdlib.h>
#include <stdlib.h>
#include <string.h>

#include "dns_list.h"

dns_request_t *dns_request_list;
/* Last request in dns_request_list */
dns_request_t *dns_request_last;
static int dns_list_count=0;

/*****************************************************************************/
int dns_list_add(dns_request_t *r)
{
  dns_request_t *new;

  if (!(new = malloc(sizeof(*new)))) {
    debug("malloc() failed");
    return 0;
  }

  memcpy(new, r, sizeof(*r));
  new->expiry = time(NULL) + DNS_TIMEOUT;
  new->switch_on_timeout = 1;
  new->prev = NULL;
  new->next = dns_request_list;

  if (dns_request_list)
    dns_request_list->prev = new;
  if (dns_request_last == NULL)
    dns_request_last = new;
  dns_request_list = new;

  dns_list_count++;
  return 1;
}

/*****************************************************************************/
dns_request_t *dns_list_find_by_id(dns_request_t *req)
{ 
  dns_request_t *r;

  for(r = dns_request_list; r; r = r->next){
     if(req->message.header.id == r->message.header.id)
         return r;
  }
  return NULL;
}


/*****************************************************************************/
void dns_list_unarm_requests_after_this(const dns_request_t *r)
{
   dns_request_t *tmp = r->next;

   while (tmp) {
      debug("unarm request at %p\n", tmp);
      tmp->switch_on_timeout = 0;
      tmp = tmp->next;
   }

   return;
}



/*****************************************************************************/
int dns_list_remove(dns_request_t *r)
{
  if (r == dns_request_list)
    dns_request_list = r->next;
  else
    r->prev->next = r->next;

  if (r == dns_request_last)
    dns_request_last = r->prev;
  else
    r->next->prev = r->prev;
 
  free(r);

  dns_list_count--;
  return 1;
}

/*****************************************************************************/
void dns_list_print(void)
{
  dns_request_t *r;

  printf("\n===========Dumping request list (time=%d):\n", (int) time(NULL));
  printf("dns_request_list=%p dns_request_last=%p dns_list_count=%d\n",
         dns_request_list, dns_request_last, dns_list_count);

  for (r = dns_request_list; r; r = r->next) {
     printf("    (%p) ID: %d ... Name: %s ---- IP: %s expiry=%d switchOnTimeout=%d next=%p prev=%p\n",
            r, r->message.header.id, r->cname, r->ip, (int) r->expiry,
            r->switch_on_timeout, r->next, r->prev);
  }

  printf("=============================================\n\n");
}

/*****************************************************************************/
int dns_list_next_time(void)
{
  int timeout = 0;

  if (dns_request_last) {
    timeout = dns_request_last->expiry - time(NULL);
    if (timeout < 1)
      timeout = 1;
  }
  return timeout;
}


