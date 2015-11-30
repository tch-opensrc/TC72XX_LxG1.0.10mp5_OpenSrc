/*
 * Copyright (C) 2001 Yasuhiro Ohara
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

#include "ospf6d.h"

void
ospf6_asbr_external_lsa_update (struct ospf6_route_req *request)
{
  char buffer [MAXLSASIZE];
  u_int16_t size;
  struct ospf6_lsa_as_external *external;
  char *p;
  struct ospf6_route_req route;
  char pbuf[BUFSIZ];

  /* assert this is best path; if not, return */
  ospf6_route_lookup (&route, &request->route.prefix, request->table);
  if (memcmp (&route.path, &request->path, sizeof (route.path)))
    return;

  if (IS_OSPF6_DUMP_LSA)
    zlog_info ("Update AS-External: ID: %lu",
               (u_long) ntohl (request->path.origin.id));

  /* prepare buffer */
  memset (buffer, 0, sizeof (buffer));
  size = sizeof (struct ospf6_lsa_as_external);
  external = (struct ospf6_lsa_as_external *) buffer;
  p = (char *) (external + 1);

  if (route.path.metric_type == 2)
    SET_FLAG (external->bits_metric, OSPF6_ASBR_BIT_E);   /* type2 */
  else
    UNSET_FLAG (external->bits_metric, OSPF6_ASBR_BIT_E); /* type1 */

  /* forwarding address */
  if (! IN6_IS_ADDR_UNSPECIFIED (&route.nexthop.address))
    SET_FLAG (external->bits_metric, OSPF6_ASBR_BIT_F);
  else
    UNSET_FLAG (external->bits_metric, OSPF6_ASBR_BIT_F);

  /* external route tag */
  UNSET_FLAG (external->bits_metric, OSPF6_ASBR_BIT_T);

  /* set metric. note: related to E bit */
  OSPF6_ASBR_METRIC_SET (external, route.path.cost);

  /* prefixlen */
  external->prefix.prefix_length = route.route.prefix.prefixlen;

  /* PrefixOptions */
  external->prefix.prefix_options = route.path.prefix_options;

  /* don't use refer LS-type */
  external->prefix.prefix_refer_lstype = htons (0);

  if (IS_OSPF6_DUMP_LSA)
    {
      prefix2str (&route.route.prefix, pbuf, sizeof (pbuf));
      zlog_info ("  Prefix: %s", pbuf);
    }

  /* set Prefix */
  memcpy (p, &route.route.prefix.u.prefix6,
          OSPF6_PREFIX_SPACE (route.route.prefix.prefixlen));
  ospf6_prefix_apply_mask (&external->prefix);
  size += OSPF6_PREFIX_SPACE (route.route.prefix.prefixlen);
  p += OSPF6_PREFIX_SPACE (route.route.prefix.prefixlen);

  /* Forwarding address */
  if (CHECK_FLAG (external->bits_metric, OSPF6_ASBR_BIT_F))
    {
      memcpy (p, &route.nexthop.address, sizeof (struct in6_addr));
      size += sizeof (struct in6_addr);
      p += sizeof (struct in6_addr);
    }

  /* External Route Tag */
  if (CHECK_FLAG (external->bits_metric, OSPF6_ASBR_BIT_T))
    {
      /* xxx */
    }

  ospf6_lsa_originate (htons (OSPF6_LSA_TYPE_AS_EXTERNAL),
                       route.path.origin.id, ospf6->router_id,
                       (char *) external, size, ospf6);
  return;
}

void
ospf6_asbr_external_route_add (struct ospf6_route_req *route)
{
  ospf6_asbr_external_lsa_update (route);
}

void
ospf6_asbr_external_route_remove (struct ospf6_route_req *route)
{
  struct ospf6_lsa *lsa;

  lsa = ospf6_lsdb_lookup_lsdb (htons (OSPF6_LSA_TYPE_AS_EXTERNAL),
                                htonl (route->path.origin.id),
                                ospf6->router_id, ospf6->lsdb);
  if (lsa)
    ospf6_lsa_premature_aging (lsa);
}

void
ospf6_asbr_external_lsa_add (struct ospf6_lsa *lsa)
{
  struct ospf6_lsa_as_external *external;
  struct prefix_ls asbr_id;
  struct ospf6_route_req asbr_entry;
  struct ospf6_route_req request;

  external = OSPF6_LSA_HEADER_END (lsa->header);

  if (IS_LSA_MAXAGE (lsa))
    return;

  if (IS_OSPF6_DUMP_ASBR)
    zlog_info ("ASBR: Calculate %s", lsa->str);

  if (lsa->header->adv_router == ospf6->router_id)
    {
      if (IS_OSPF6_DUMP_ASBR)
        zlog_info ("ASBR:   Self-originated, ignore");
      return;
    }

  if (OSPF6_ASBR_METRIC (external) == LS_INFINITY)
    {
      if (IS_OSPF6_DUMP_ASBR)
        zlog_info ("ASBR:   Metric is Infinity, ignore");
      return;
    }

  memset (&asbr_id, 0, sizeof (asbr_id));
  asbr_id.family = AF_UNSPEC;
  asbr_id.prefixlen = 64; /* xxx */
  asbr_id.adv_router.s_addr = lsa->header->adv_router;

  ospf6_route_lookup (&asbr_entry, (struct prefix *) &asbr_id,
                      ospf6->topology_table);

  if (ospf6_route_end (&asbr_entry))
    {
      if (IS_OSPF6_DUMP_ASBR)
        {
          char buf[64];
          inet_ntop (AF_INET, &asbr_id.adv_router, buf, sizeof (buf));
          zlog_info ("ASBR:   ASBR %s not found, ignore", buf);
        }
      return;
    }

  memset (&request, 0, sizeof (request));
  request.route.type = OSPF6_DEST_TYPE_NETWORK;
  request.route.prefix.family = AF_INET6;
  request.route.prefix.prefixlen = external->prefix.prefix_length;
  memcpy (&request.route.prefix.u.prefix6, (char *)(external + 1),
          OSPF6_PREFIX_SPACE (request.route.prefix.prefixlen));

  request.path.area_id = asbr_entry.path.area_id;
  request.path.origin.type = htons (OSPF6_LSA_TYPE_AS_EXTERNAL);
  request.path.origin.id = lsa->header->id;
  request.path.origin.adv_router = lsa->header->adv_router;
  if (CHECK_FLAG (external->bits_metric, OSPF6_ASBR_BIT_E))
    {
      request.path.type = OSPF6_PATH_TYPE_EXTERNAL2;
      request.path.metric_type = 2;
      request.path.cost = asbr_entry.path.cost;
      request.path.cost_e2 = OSPF6_ASBR_METRIC (external);
    }
  else
    {
      request.path.type = OSPF6_PATH_TYPE_EXTERNAL1;
      request.path.metric_type = 1;
      request.path.cost = asbr_entry.path.cost
                          + OSPF6_ASBR_METRIC (external);
      request.path.cost_e2 = 0;
    }
  request.path.prefix_options = external->prefix.prefix_options;

  while (((struct prefix_ls *)&asbr_entry.route.prefix)->adv_router.s_addr ==
         asbr_id.adv_router.s_addr &&
         asbr_entry.route.type == OSPF6_DEST_TYPE_ROUTER)
    {
      memcpy (&request.nexthop, &asbr_entry.nexthop,
              sizeof (struct ospf6_nexthop));
      ospf6_route_add (&request, ospf6->route_table);
      ospf6_route_next (&asbr_entry);
    }
}

void
ospf6_asbr_external_lsa_remove (struct ospf6_lsa *lsa)
{
  struct ospf6_lsa_as_external *external;
  struct prefix dest;
  char buf[64];
  struct ospf6_route_req request;

  if (IS_OSPF6_DUMP_ASBR)
    zlog_info ("ASBR: Withdraw route of %s", lsa->str);

  if (lsa->header->adv_router == ospf6->router_id)
    {
      if (IS_OSPF6_DUMP_ASBR)
        zlog_info ("ASBR:   Self-originated, ignore");
      return;
    }

  external = OSPF6_LSA_HEADER_END (lsa->header);
  memset (&dest, 0, sizeof (dest));
  dest.family = AF_INET6;
  dest.prefixlen = external->prefix.prefix_length;
  memcpy (&dest.u.prefix6, (char *)(external + 1),
          OSPF6_PREFIX_SPACE (dest.prefixlen));

  prefix2str (&dest, buf, sizeof (buf));
  if (IS_OSPF6_DUMP_ASBR)
    zlog_info ("ASBR:   route: %s", buf);

  ospf6_route_lookup (&request, &dest, ospf6->route_table);
  if (ospf6_route_end (&request))
    {
      zlog_info ("ASBR:   route not found");
      return;
    }

  while (request.path.origin.id != lsa->header->id ||
         request.path.origin.adv_router != lsa->header->adv_router)
    {
      if (prefix_same (&request.route.prefix, &dest) != 1)
        {
          zlog_info ("ASBR:   Can't find the entry matches the origin");
          return;
        }
      ospf6_route_next (&request);
    }
  assert (request.path.origin.id == lsa->header->id);
  assert (request.path.origin.adv_router == request.path.origin.adv_router);

  while (request.path.origin.id == lsa->header->id &&
         request.path.origin.adv_router == lsa->header->adv_router &&
         prefix_same (&request.route.prefix, &dest) == 1)
    {
      ospf6_route_remove (&request, ospf6->route_table);
      ospf6_route_next (&request);
    }
}

void
ospf6_asbr_external_lsa_change (struct ospf6_lsa *old, struct ospf6_lsa *new)
{
  assert (old || new);

  if (old == NULL)
    ospf6_asbr_external_lsa_add (new);
  else if (new == NULL)
    ospf6_asbr_external_lsa_remove (old);
  else
    {
      ospf6_route_table_freeze (ospf6->route_table);
      ospf6_asbr_external_lsa_remove (old);
      ospf6_asbr_external_lsa_add (new);
      ospf6_route_table_thaw (ospf6->route_table);
    }
}

void
ospf6_asbr_asbr_entry_add (struct ospf6_route_req *topo_entry)
{
  struct ospf6_lsdb_node node;

  struct prefix_ls *inter_router;
  u_int32_t id, adv_router;

  inter_router = (struct prefix_ls *) &topo_entry->route.prefix;
  id = inter_router->id.s_addr;
  adv_router = inter_router->adv_router.s_addr;

  if (IS_OSPF6_DUMP_ASBR)
    {
      char buf[64];
      inet_ntop (AF_INET, &inter_router->adv_router, buf, sizeof (buf));
      zlog_info ("ASBR: New router found: %s", buf);
    }

  if (ntohl (id) != 0 ||
      ! OSPF6_OPT_ISSET (topo_entry->path.capability, OSPF6_OPT_E))
    {
      zlog_warn ("ASBR: Inter topology table malformed");
      return;
    }

  for (ospf6_lsdb_type_router (&node, htons (OSPF6_LSA_TYPE_AS_EXTERNAL),
                               adv_router, ospf6->lsdb);
       ! ospf6_lsdb_is_end (&node);
       ospf6_lsdb_next (&node))
    ospf6_asbr_external_lsa_add (node.lsa);
}

void
ospf6_asbr_asbr_entry_remove (struct ospf6_route_req *topo_entry)
{
  struct prefix_ls *inter_router;
  u_int32_t id, adv_router;
  struct ospf6_route_req request;

  inter_router = (struct prefix_ls *) &topo_entry->route.prefix;
  id = inter_router->id.s_addr;
  adv_router = inter_router->adv_router.s_addr;

  if (IS_OSPF6_DUMP_ASBR)
    {
      char buf[64];
      inet_ntop (AF_INET, &inter_router->adv_router, buf, sizeof (buf));
      zlog_info ("ASBR: Router disappearing: %s", buf);
    }

  if (ntohl (id) != 0 ||
      ! OSPF6_OPT_ISSET (topo_entry->path.capability, OSPF6_OPT_E))
    {
      zlog_warn ("ASBR: Inter topology table malformed");
    }

  for (ospf6_route_head (&request, ospf6->route_table);
       ! ospf6_route_end (&request);
       ospf6_route_next (&request))
    {
      if (request.path.type != OSPF6_PATH_TYPE_EXTERNAL1 &&
          request.path.type != OSPF6_PATH_TYPE_EXTERNAL2)
        continue;
      if (request.path.area_id != topo_entry->path.area_id)
        continue;
      if (request.path.origin.adv_router != topo_entry->path.origin.adv_router)
        continue;
      if (memcmp (&topo_entry->nexthop, &request.nexthop,
                  sizeof (struct ospf6_nexthop)))
        continue;

      ospf6_route_remove (&request, ospf6->route_table);
    }
}

int
ospf6_asbr_external_show (struct vty *vty, struct ospf6_lsa *lsa)
{
  struct ospf6_lsa_as_external *external;
  char buf[128], *ptr;
  struct in6_addr in6;

  assert (lsa->header);
  external = (struct ospf6_lsa_as_external *)(lsa->header + 1);
  
  /* bits */
  snprintf (buf, sizeof (buf), "%s%s%s",
            (CHECK_FLAG (external->bits_metric, OSPF6_ASBR_BIT_E) ?
             "E" : "-"),
            (CHECK_FLAG (external->bits_metric, OSPF6_ASBR_BIT_F) ?
             "F" : "-"),
            (CHECK_FLAG (external->bits_metric, OSPF6_ASBR_BIT_T) ?
             "T" : "-"));

  vty_out (vty, "     Bits: %s%s", buf, VTY_NEWLINE);
  vty_out (vty, "     Metric: %5lu%s", (u_long)OSPF6_ASBR_METRIC (external),
           VTY_NEWLINE);

  ospf6_prefix_options_str (external->prefix.prefix_options,
                            buf, sizeof (buf));
  vty_out (vty, "     Prefix Options: %s%s", buf, VTY_NEWLINE);

  vty_out (vty, "     Referenced LSType: %d%s",
           ntohs (external->prefix.prefix_refer_lstype), VTY_NEWLINE);

  ospf6_prefix_in6_addr (&external->prefix, &in6);
  inet_ntop (AF_INET6, &in6, buf, sizeof (buf));
  vty_out (vty, "     Prefix: %s/%d%s",
           buf, external->prefix.prefix_length, VTY_NEWLINE);

  /* Forwarding-Address */
  if (CHECK_FLAG (external->bits_metric, OSPF6_ASBR_BIT_F))
    {
      ptr = ((char *)(external + 1))
            + OSPF6_PREFIX_SPACE (external->prefix.prefix_length);
      inet_ntop (AF_INET6, (struct in6_addr *) ptr, buf, sizeof (buf));
      vty_out (vty, "     Forwarding-Address: %s%s", buf, VTY_NEWLINE);
    }

  return 0;
}

int
ospf6_asbr_external_refresh (void *old)
{
  struct ospf6_lsa *lsa = old;
  struct ospf6_route_req route, *target;

  assert (ospf6);

  target = NULL;
  for (ospf6_route_head (&route, ospf6->external_table);
       ! ospf6_route_end (&route);
       ospf6_route_next (&route))
    {
      if (route.path.origin.id == lsa->header->id)
        {
          target = &route;
          break;
        }
    }

  if (target)
    ospf6_asbr_external_lsa_update (target);
  else
    ospf6_lsa_premature_aging (lsa);

  return 0;
}

void
ospf6_asbr_database_hook (struct ospf6_lsa *old, struct ospf6_lsa *new)
{
  if (old)
    ospf6_asbr_external_lsa_remove (old);
  if (new && ! IS_LSA_MAXAGE (new))
    ospf6_asbr_external_lsa_add (new);
}

void
ospf6_asbr_register_as_external ()
{
  struct ospf6_lsa_slot slot;

  memset (&slot, 0, sizeof (slot));
  slot.type              = htons (OSPF6_LSA_TYPE_AS_EXTERNAL);
  slot.name              = "AS-External";
  slot.func_show         = ospf6_asbr_external_show;
  slot.func_refresh      = ospf6_asbr_external_refresh;
  ospf6_lsa_slot_register (&slot);

  ospf6_lsdb_hook[OSPF6_LSA_TYPE_AS_EXTERNAL & OSPF6_LSTYPE_CODE_MASK].hook = 
    ospf6_asbr_database_hook;
}

void
ospf6_asbr_init ()
{
  ospf6_asbr_register_as_external ();
}


