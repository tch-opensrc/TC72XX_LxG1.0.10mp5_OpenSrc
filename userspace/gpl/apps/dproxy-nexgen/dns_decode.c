#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "dns_decode.h"

/*
 * The following macros set num to the host version of the number pointed to 
 * by buf and increment the buff (usually a pointer) and count (usually and 
 * int counter)
 */
#define SET_UINT16( num, buff) num = htons(*(uint16*)*buff); *buff += 2
#define SET_UINT32( num, buff) num = htonl(*(uint32*)*buff); *buff += 4
/*****************************************************************************/
/* reverse lookups encode the IP in reverse, so we need to turn it around
 * example 2.0.168.192.in-addr.arpa => 192.168.0.2
 * this function only returns the first four numbers in the IP
 *  NOTE: it assumes that name points to a buffer BUF_SIZE long
 */
void dns_decode_reverse_name(char *name)
{
    char temp[NAME_SIZE];
    int i,j,k;

    k = 0;

    for( j = 0, i = 0; j<3; i++) if( name[i] == '.' )j++;
    for( ; name[i] != '.'; i++) temp[k++] = name[i];
    temp[k++] = '.';

    name[i] = 0;

    for( j = 0, i = 0; j<2; i++) if( name[i] == '.' )j++;
    for( ; name[i] != '.'; i++) temp[k++] = name[i];
    temp[k++] = '.';

    for( j = 0, i = 0; j<1; i++) if( name[i] == '.' )j++;
    for( ; name[i] != '.'; i++) temp[k++] = name[i];
    temp[k++] = '.';

    for( i = 0; name[i] != '.'; i++) temp[k++] = name[i];

    temp[k] = 0;

    strcpy( name, temp );
}
/*****************************************************************************/
/* Queries are encoded such that there is and integer specifying how long 
 * the next block of text will be before the actuall text. For eaxmple:
 *             www.linux.com => \03www\05linux\03com\0
 * This function assumes that buf points to an encoded name.
 * The name is decoded into name. Name should be at least 255 bytes long.
 */
void dns_decode_name(char *name, char **buf)
{
  int i, k, len, j;

  i = k = 0;
  while( **buf ){
         len = *(*buf)++;
         for( j = 0; j<len ; j++)
	      name[k++] = *(*buf)++;
         name[k++] = '.';
  }
  name[k-1] = *(*buf)++;
}
/*****************************************************************************/
/* Decodes the raw packet in buf to create a rr. Assumes buf points to the 
 * start of a rr. 
 * Note: Question rrs dont have rdatalen or rdata. Set is_question when
 * decoding question rrs, else clear is_question
 */
void dns_decode_rr(struct dns_rr *rr, char **buf, int is_question,char *header, char *buf_start, struct dns_message *m)
{
  /* if the first two bits the of the name are set, then the message has been
     compressed and so the next byte is an offset from the start of the message
     pointing to the start of the name */
  if( **buf & 0xC0 ){
    (*buf)++;
    header += *(*buf)++;
    dns_decode_name( rr->name, &header );
  }else{
    /* ordinary decode name */
    dns_decode_name( rr->name, buf );
  }  

  SET_UINT16( rr->type, buf );
  SET_UINT16( rr->class, buf);

  if( is_question != 1 ){
    SET_UINT32( rr->ttl, buf );
    SET_UINT16( rr->rdatalen, buf );
    
    /* BRCM message format wrong. drop it */
    if(((*buf - buf_start) >= MAX_PACKET_SIZE) ||
       (((*buf - buf_start) + rr->rdatalen) >= MAX_PACKET_SIZE) ||
       (rr->rdatalen >= MAX_PACKET_SIZE/2))
    {
      m->header.ancount = 0;
      return;
    }
    memcpy( rr->data, *buf, rr->rdatalen );
    *buf += rr->rdatalen;
    /*
    for(i = 0; i < rr->rdatalen; i+=4 )
      SET_UINT32( (uint32)rr->data[i], buf );
    */
  }

  if( rr->type == PTR ){ /* reverse lookup */
    dns_decode_reverse_name( rr->name );
  }
}
/*****************************************************************************/
/* A raw packet pointed to by buf is decoded in the assumed to be alloced 
 * dns_message structure.
 * RETURNS: 0
 */
int dns_decode_message(struct dns_message *m, char **buf)
{
  int i;
  char *header_start = *buf;
  char *buf_start = *buf;

  //BRCM: just decode id and header
  SET_UINT16( m->header.id, buf );  
  SET_UINT16( m->header.flags.flags, buf );
  
  SET_UINT16( m->header.qdcount, buf );
  SET_UINT16( m->header.ancount, buf );
  SET_UINT16( m->header.nscount, buf );
  SET_UINT16( m->header.arcount, buf );

  #if 0 //BRCM
  if( m->header.ancount > 1 ){
    printf("Lotsa answers\n");
  }
  #endif

  /* decode all the question rrs */
  for( i = 0; i < m->header.qdcount && i < NUM_RRS; i++){
    dns_decode_rr( &m->question[i], buf, 1, header_start, buf_start, m);
  }  
  /* decode all the answer rrs */
  for( i = 0; i < m->header.ancount && i < NUM_RRS; i++){
    dns_decode_rr( &m->answer[i], buf, 0, header_start, buf_start, m);
  }

  return 0;
}
/*****************************************************************************/
void dns_decode_request(dns_request_t *m)
{
  struct in_addr *addr;
  char *ptr;
  int i;

  m->here = m->original_buf;
  dns_decode_message( &m->message, &m->here);
 
  if( m->message.question[0].type == PTR ){
      strcpy( m->ip, m->message.question[0].name);
  }else if ( m->message.question[0].type == A || 
	  m->message.question[0].type == AAA){ 
      strncpy( m->cname, m->message.question[0].name, NAME_SIZE );
  }

  /* set according to the answer */
  for( i = 0; i < m->message.header.ancount && i < NUM_RRS; i++){

     m->ttl = m->message.answer[i].ttl;
     /* make sure we ge the same type as the query incase there are multiple
        and unrelated answers */
     if( m->message.question[0].type == m->message.answer[i].type ){
         if( m->message.answer[i].type == A
             /*BRCM || m->message.answer[i].type == AAA*/ ){
             
             /* Standard lookup so convert data to an IP */
             addr = (struct in_addr *)m->message.answer[i].data;
             strcpy( m->ip, inet_ntoa( addr[0] ) );
             break;
         }
         else if( m->message.answer[i].type == PTR )
         {
             /* Reverse lookup so convert data to a nume */
             ptr = m->message.answer[i].data;
             dns_decode_name( m->cname, &ptr );
             strcpy( m->ip, m->message.answer[i].name);
             break;
         }
     } /* if( question == answer ) */
  } /* for */
}
