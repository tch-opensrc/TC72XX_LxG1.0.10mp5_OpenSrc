#include <linux/netfilter.h>
#include "libipq.h"
#include "filter.h"
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <syslog.h>

#define BUFSIZE 2048

void add_entry(char *website, char *folder)
{
   PURL new_entry, current, prev;

   new_entry = (PURL) malloc (sizeof(URL));
   strcpy(new_entry->website, website);
   strcpy(new_entry->folder, folder);
   new_entry->next = NULL;

   if (purl == NULL)
   {
      purl = new_entry;
   }
   else 
   {
      current = purl;
      while (current) 
      {
         prev = current;
         current = current->next;
      }
      prev->next = new_entry;
   }
}

int get_url_info()
{
   char temp[MAX_WEB_LEN + MAX_FOLDER_LEN], *temp1, *temp2, web[MAX_WEB_LEN], folder[MAX_FOLDER_LEN];
   int i=0;
			
   FILE *f = fopen("/var/url_list", "r");
   while (fgets(temp,96, f) != '\0')
   {
      if (temp[0]=='h' && temp[1]=='t' && temp[2]=='t' && temp[3]=='p' && temp[4]==':' && temp[5]=='/' && temp[6]=='/')
      {
         temp1 = temp + 7;	
      }
      else
      {
         temp1 = temp;	
      }

      if (*temp1=='w' && *(temp1+1)=='w' && *(temp1+2)=='w' && *(temp1+3)=='.')
      {
         temp1 = temp1 + 4;
      }

      if (temp2 = strchr (temp1, '\n'))
      {
         *temp2 = '\0';
      }
		       
      sscanf(temp1, "%[^/]", web);		
      temp1 = strchr(temp1, '/');
      if (temp1 == NULL)
      {
         strcpy(folder, "\0");
      }
      else
      {
         strcpy(folder, ++temp1);		
      }
      add_entry(web, folder);
      list_count ++;
   }

   fclose(f);

   return 0;
}


static void die(struct ipq_handle *h)
{
   ipq_perror("passer");
   ipq_destroy_handle(h);
   exit(1);
}

int main(int argc, char **argv)
{
   int status, i;
   unsigned int payload_len, payload_offset;
   unsigned char buf[BUFSIZE], listtype[8];
   struct ipq_handle *h;
   unsigned char *match, *folder, *url;
   PURL current;

   strcpy (listtype, argv[1]);
   get_url_info();

   h = ipq_create_handle(0, PF_INET);
   if (!h)
   {
      die(h);
   }

   status = ipq_set_mode(h, IPQ_COPY_PACKET, BUFSIZE);
   if (status < 0)
   {
      die(h);
   }

   do
   {
      memset(buf, 0, sizeof(buf));
      status = ipq_read(h, buf, BUFSIZE, 0);
      if (status < 0)
      {
         die(h);
      }

      switch (ipq_message_type(buf)) 
      {
         case NLMSG_ERROR:
         {
            fprintf(stderr, "Received error message %d\n",
            ipq_get_msgerr(buf));
            break;
         }

         case IPQM_PACKET:  
         {
            ipq_packet_msg_t *m = ipq_get_packet(buf);
            char decision = 'n';
            struct iphdr *iph = ((struct iphdr *)m->payload);
            struct tcphdr *tcp = (struct tcphdr *)(m->payload + (iph->ihl<<2));
            match = folder = url = NULL;
            payload_offset = ((iph->ihl)<<2) + (tcp->doff<<2);
            payload_len = (unsigned int)ntohs(iph->tot_len) - ((iph->ihl)<<2) + (tcp->doff<<2);
            match = (char *)(m->payload + payload_offset);

            if(strstr(match, "GET ") == NULL && strstr(match, "POST ") == NULL && strstr(match, "HEAD ") == NULL)
            {
               status = ipq_set_verdict(h, m->packet_id, NF_ACCEPT, 0, NULL);
//printf("****NO HTTP INFORMATION!!!\n");
               if (status < 0)
               {
                  die(h);
               }
								
               break;		  
            }
	    
            for (current = purl; current != NULL; current = current->next)
            {
               if (current->folder[0] != '\0')
               {
                  folder = strstr(match, current->folder);
               }
//printf("####payload = %s\n\n", match);

               if ( (url = strstr(match, current->website)) != NULL ) 
               {
                  if (strcmp(listtype, "Exclude") == 0) 
                  {
                     if ( (folder != NULL) || (current->folder[0] == '\0') )
                     {
                        status = ipq_set_verdict(h, m->packet_id, NF_DROP, 0, NULL);
//printf("####This page is blocked by Exclude list!");
                        decision = 'y';
										
                     }
                     else 
                     {
                        status = ipq_set_verdict(h, m->packet_id, NF_ACCEPT, 0, NULL);
//printf("###Website hits but folder no hit in Exclude list! packets pass\n");
                        decision = 'y';
                     }

                     if (status < 0)
                     {
                        die(h);
                     }
								
                     break;
									
                  }
                  else 
                  {
                     if ( (folder != NULL) || (current->folder[0] == '\0') )
                     {
                        status = ipq_set_verdict(h, m->packet_id, NF_ACCEPT, 0, NULL);
//printf("####This page is accepted by Include list!");
                        decision = 'y';
                     }
                     else 
                     {
                        status = ipq_set_verdict(h, m->packet_id, NF_DROP, 0, NULL);
//printf("####Website hits but folder no hit in Include list!, packets drop\n");
                        decision = 'y';
                     }
										
                     if (status < 0)
                     {
                        die(h);
                     }

                     break;
                  }
               }
            }

            if (url == NULL) 
            {
               if (strcmp(listtype, "Exclude") == 0) 
               {
                  status = ipq_set_verdict(h, m->packet_id, NF_ACCEPT, 0, NULL);
//printf("~~~~No Url hits!! This page is accepted by Exclude list!\n");
                  decision = 'y';
               }
               else 
               {
                  status = ipq_set_verdict(h, m->packet_id, NF_DROP, 0, NULL);
//printf("~~~~No Url hits!! This page is blocked by Include list!\n");
                  decision = 'y';
               }

               if (status < 0)
               {
                  die(h);
               }
            }
								
            if (decision == 'n') 
            {
               ipq_set_verdict(h, m->packet_id, NF_ACCEPT, 0, NULL);
//printf("~~~None of rules can be applied!! Traffic is allowed!!\n");
            }

            break;
         }

         default:
         {
            fprintf(stderr, "Unknown message type!\n");
            break;
         }
      }
   } while (1);

   ipq_destroy_handle(h);
   return 0;
}

