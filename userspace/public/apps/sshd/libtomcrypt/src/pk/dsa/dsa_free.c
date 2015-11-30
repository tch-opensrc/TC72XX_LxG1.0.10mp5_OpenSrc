/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@gmail.com, http://libtomcrypt.org
 */
#include "tomcrypt.h"

/**
   @file dsa_free.c
   DSA implementation, free a DSA key, Tom St Denis
*/

#ifdef MDSA

/**
   Free a DSA key
   @param key   The key to free from memory
*/
void dsa_free(dsa_key *key)
{
   LTC_ARGCHK(key != NULL);
   mp_clear_multi(&key->g, &key->q, &key->p, &key->x, &key->y, NULL);
}

#endif

/* $Source: /cvs/cable/Cow/userspace/public/apps/sshd/libtomcrypt/src/pk/dsa/dsa_free.c,v $ */
/* $Revision: 1.3 $ */
/* $Date: 2015/08/20 02:27:59 $ */
