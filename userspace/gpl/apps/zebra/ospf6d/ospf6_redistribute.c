/*
 * OSPFv3 Redistribute
 * Copyright (C) 1999 Yasuhiro Ohara
 *
 * This file is part of GNU Zebra.
 *
 * GNU Zebra is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * GNU Zebra is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with GNU Zebra; see the file COPYING.  If not, write to the 
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
 * Boston, MA 02111-1307, USA.  
 */

#if 0

#include <zebra.h>

#include "log.h"
#include "memory.h"
#include "vty.h"
#include "prefix.h"
#include "table.h"
#include "linklist.h"
#include "routemap.h"
#include "command.h"

#include "ospf6_top.h"
#include "ospf6_redistribute.h"
#include "ospf6_dump.h"
#include "ospf6_prefix.h"
#include "ospf6_lsa.h"
#include "ospf6_lsdb.h"

#include "ospf6_route.h"
#include "ospf6_zebra.h"

#include "ospf6_message.h"
#include "ospf6_neighbor.h"
#include "ospf6_interface.h"

extern struct ospf6 *ospf6;

#else /*0*/

#include "ospf6d.h"

#endif /*0*/

unsigned int redistribute_id = 0;

void
ospf6_redistribute_routemap_set (struct ospf6 *o6, int type, char *mapname)
{
  if (o6->rmap[type].name)
    free (o6->rmap[type].name);

  o6->rmap[type].name = strdup (mapname);
  o6->rmap[type].map = route_map_lookup_by_name (mapname);
}

void
ospf6_redistribute_routemap_update ()
{
  struct ospf6 *o6 = ospf6;
  int i;

  for (i = 0; i < ZEBRA_ROUTE_MAX; i++)
    {
      if (o6->rmap[i].name)
        o6->rmap[i].map = route_map_lookup_by_name (o6->rmap[i].name);
      else
        o6->rmap[i].map = NULL;
    }
}

void
ospf6_redistribute_routemap_unset (struct ospf6 *o6, int type)
{
  if (o6->rmap[type].name)
    free (o6->rmap[type].name);

  o6->rmap[type].name = NULL;
  o6->rmap[type].map = NULL;
}

void
ospf6_redistribute_route_add (int type, int ifindex, struct prefix_ipv6 *p)
{
  int ret;

  char prefix_name[128];
  struct ospf6_route_req request;

  /* for log */
  inet_ntop (AF_INET6, &p->prefix, prefix_name, sizeof (prefix_name));

  if (IS_OSPF6_DUMP_ZEBRA)
    {
      zlog_info ("ZEBRA: Read: Add %s/%d ifindex: %d (%s)",
                 prefix_name, p->prefixlen, ifindex, zebra_route_name[type]);
    }

  /* Ignore Connected prefix of OSPF enabled Interface */
  if (type == ZEBRA_ROUTE_CONNECT && ospf6_interface_is_enabled (ifindex))
    {
      if (IS_OSPF6_DUMP_ZEBRA)
        zlog_info ("ZEBRA:   Ignore connect route of enabled I/F");
      return;
    }

  memset (&request, 0, sizeof (request));
  request.route.type = OSPF6_DEST_TYPE_NETWORK;
  prefix_copy (&request.route.prefix, (struct prefix *) p);
  request.path.type = type + OSPF6_PATH_TYPE_ZOFFSET;
  request.path.metric_type = OSPF6_REDISTRIBUTE_DEFAULT_TYPE;
  request.path.cost = OSPF6_REDISTRIBUTE_DEFAULT_METRIC;
  request.path.cost_e2 = OSPF6_REDISTRIBUTE_DEFAULT_METRIC;
  request.nexthop.ifindex = ifindex;

  /* apply route-map */
  if (ospf6->rmap[type].map)
    {
      ret = route_map_apply (ospf6->rmap[type].map, (struct prefix *) p,
                             RMAP_OSPF6, &request);
      if (ret == RMAP_DENYMATCH)
        {
          if (IS_OSPF6_DUMP_ZEBRA)
            zlog_info ("ZEBRA:   Denied by route-map %s",
                       ospf6->rmap[type].name);
          return;
        }
    }

  request.path.origin.id = htonl (++redistribute_id);
  ospf6_route_add (&request, ospf6->external_table);
}

void
ospf6_redistribute_route_remove (int type, int ifindex, struct prefix_ipv6 *p)
{
  char prefix_name[128];
  struct ospf6_route_req request;

  /* for log */
  inet_ntop (AF_INET6, &p->prefix, prefix_name, sizeof (prefix_name));

  if (IS_OSPF6_DUMP_ZEBRA)
    {
      zlog_info ("ZEBRA: Read: Remove %s/%d ifindex: %d (%s)",
                 prefix_name, p->prefixlen, ifindex, zebra_route_name[type]);
    }

  ospf6_route_lookup (&request, (struct prefix *) p, ospf6->external_table);
  if (ospf6_route_end (&request))
    {
      if (IS_OSPF6_DUMP_ZEBRA)
        zlog_info ("ZEBRA:   No such route, ignore");
      return;
    }

  if (request.path.type != type + OSPF6_PATH_TYPE_ZOFFSET)
    {
      if (IS_OSPF6_DUMP_ZEBRA)
        zlog_info ("ZEBRA:   Type mismatch, ignore");
      return;
    }

  if (request.nexthop.ifindex != ifindex)
    {
      if (IS_OSPF6_DUMP_ZEBRA)
        zlog_info ("ZEBRA:   Ifindex mismatch, ignore");
      return;
    }

  ospf6_route_remove (&request, ospf6->external_table);
}

DEFUN (show_ipv6_route_ospf6_external,
       show_ipv6_route_ospf6_external_cmd,
       "show ipv6 ospf6 route redistribute",
       SHOW_STR
       IP6_STR
       ROUTE_STR
       OSPF6_STR
       "redistributing External information\n"
       )
{
  OSPF6_CMD_CHECK_RUNNING ();
  return ospf6_route_table_show (vty, argc, argv, ospf6->external_table);
}

ALIAS (show_ipv6_route_ospf6_external,
       show_ipv6_route_ospf6_external_prefix_cmd,
       "show ipv6 ospf6 route redistribute X::X",
       SHOW_STR
       IP6_STR
       ROUTE_STR
       OSPF6_STR
       "redistributing External information\n"
       "match IPv6 prefix\n"
       )

DEFUN (ospf6_redistribute,
       ospf6_redistribute_cmd,
       "redistribute (static|kernel|connected|ripng|bgp)",
       "Redistribute\n"
       "Static route\n"
       "Kernel route\n"
       "Connected route\n"
       "RIPng route\n"
       "BGP route\n"
      )
{
  int type = 0;

  if (strncmp (argv[0], "sta", 3) == 0)
    type = ZEBRA_ROUTE_STATIC;
  else if (strncmp (argv[0], "ker", 3) == 0)
    type = ZEBRA_ROUTE_KERNEL;
  else if (strncmp (argv[0], "con", 3) == 0)
    type = ZEBRA_ROUTE_CONNECT;
  else if (strncmp (argv[0], "rip", 3) == 0)
    type = ZEBRA_ROUTE_RIPNG;
  else if (strncmp (argv[0], "bgp", 3) == 0)
    type = ZEBRA_ROUTE_BGP;

  ospf6_zebra_no_redistribute (type);
  ospf6_redistribute_routemap_unset (ospf6, type);
  ospf6_zebra_redistribute (type);
  return CMD_SUCCESS;
}

DEFUN (ospf6_redistribute_routemap,
       ospf6_redistribute_routemap_cmd,
       "redistribute (static|kernel|connected|ripng|bgp) route-map WORD",
       "Redistribute\n"
       "Static routes\n"
       "Kernel route\n"
       "Connected route\n"
       "RIPng route\n"
       "BGP route\n"
       "Route map reference\n"
       "Route map name\n"
      )
{
  int type = 0;

  if (strncmp (argv[0], "sta", 3) == 0)
    type = ZEBRA_ROUTE_STATIC;
  else if (strncmp (argv[0], "ker", 3) == 0)
    type = ZEBRA_ROUTE_KERNEL;
  else if (strncmp (argv[0], "con", 3) == 0)
    type = ZEBRA_ROUTE_CONNECT;
  else if (strncmp (argv[0], "rip", 3) == 0)
    type = ZEBRA_ROUTE_RIPNG;
  else if (strncmp (argv[0], "bgp", 3) == 0)
    type = ZEBRA_ROUTE_BGP;

  ospf6_zebra_no_redistribute (type);
  ospf6_redistribute_routemap_set (ospf6, type, argv[1]);
  ospf6_zebra_redistribute (type);
  return CMD_SUCCESS;
}

DEFUN (no_ospf6_redistribute,
       no_ospf6_redistribute_cmd,
       "no redistribute (static|kernel|connected|ripng|bgp)",
       NO_STR
       "Redistribute\n"
       "Static route\n"
       "Kernel route\n"
       "Connected route\n"
       "RIPng route\n"
       "BGP route\n"
      )
{
  int type = 0;
  struct ospf6_route_req request;

  if (strncmp (argv[0], "sta", 3) == 0)
    type = ZEBRA_ROUTE_STATIC;
  else if (strncmp (argv[0], "ker", 3) == 0)
    type = ZEBRA_ROUTE_KERNEL;
  else if (strncmp (argv[0], "con", 3) == 0)
    type = ZEBRA_ROUTE_CONNECT;
  else if (strncmp (argv[0], "rip", 3) == 0)
    type = ZEBRA_ROUTE_RIPNG;
  else if (strncmp (argv[0], "bgp", 3) == 0)
    type = ZEBRA_ROUTE_BGP;

  ospf6_zebra_no_redistribute (type);
  ospf6_redistribute_routemap_unset (ospf6, type);

  /* remove redistributed route */
  for (ospf6_route_head (&request, ospf6->external_table);
       ! ospf6_route_end (&request);
       ospf6_route_next (&request))
    {
      if (request.path.type != type + OSPF6_PATH_TYPE_ZOFFSET)
        continue;
      ospf6_route_remove (&request, ospf6->external_table);
    }

  return CMD_SUCCESS;
}


char *zebra_route_string[] = { "system", "kernel", "connected", "static",
                               "rip", "ripng", "ospf", "ospf6", "bgp" };
int
ospf6_redistribute_config_write (struct vty *vty)
{
  int i;

  for (i = 0; i < ZEBRA_ROUTE_MAX; i++)
    {
      if (i == ZEBRA_ROUTE_OSPF6)
        continue;

      if (ospf6_zebra_is_redistribute (i) == 0)
        continue;

      if (ospf6->rmap[i].map)
        vty_out (vty, " redistribute %s route-map %s%s",
                 zebra_route_string[i], ospf6->rmap[i].name, VTY_NEWLINE);
      else
        vty_out (vty, " redistribute %s%s",
                 zebra_route_string[i], VTY_NEWLINE);
    }

  return 0;
}

void
ospf6_redistribute_show_config (struct vty *vty, struct ospf6 *o6)
{
  int i;

  if (ospf6_zebra_is_redistribute(ZEBRA_ROUTE_SYSTEM) ||
      ospf6_zebra_is_redistribute(ZEBRA_ROUTE_KERNEL) ||
      ospf6_zebra_is_redistribute(ZEBRA_ROUTE_STATIC) ||
      ospf6_zebra_is_redistribute(ZEBRA_ROUTE_RIPNG)  ||
      ospf6_zebra_is_redistribute(ZEBRA_ROUTE_BGP))
    vty_out (vty, " Redistributing External Routes from,%s", VTY_NEWLINE);
  else
    return;

  for (i = 0; i < ZEBRA_ROUTE_MAX; i++)
    {
      if (i == ZEBRA_ROUTE_OSPF6)
        continue;

      if (ospf6_zebra_is_redistribute (i))
        {
          if (o6->rmap[i].map)
            vty_out (vty, "    %s with route-map %s%s",
                     zebra_route_string[i], o6->rmap[i].name,
                     VTY_NEWLINE);
          else
            vty_out (vty, "    %s%s", zebra_route_string[i], VTY_NEWLINE);
        }
    }
}

void
ospf6_redistribute_init (struct ospf6 *o6)
{
  install_element (VIEW_NODE, &show_ipv6_route_ospf6_external_cmd);
  install_element (VIEW_NODE, &show_ipv6_route_ospf6_external_prefix_cmd);
  install_element (ENABLE_NODE, &show_ipv6_route_ospf6_external_cmd);
  install_element (ENABLE_NODE, &show_ipv6_route_ospf6_external_prefix_cmd);

  install_element (OSPF6_NODE, &ospf6_redistribute_cmd);
  install_element (OSPF6_NODE, &ospf6_redistribute_routemap_cmd);
  install_element (OSPF6_NODE, &no_ospf6_redistribute_cmd);
}

void
ospf6_redistribute_finish (struct ospf6 *o6)
{
  ospf6_route_remove_all (ospf6->external_table);
}

