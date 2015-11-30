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
   @file ofb_done.c
   OFB implementation, finish chain, Tom St Denis
*/

#ifdef OFB

/** Terminate the chain
  @param ofb    The OFB chain to terminate
  @return CRYPT_OK on success
*/
int ofb_done(symmetric_OFB *ofb)
{
   int err;
   LTC_ARGCHK(ofb != NULL);

   if ((err = cipher_is_valid(ofb->cipher)) != CRYPT_OK) {
      return err;
   }
   cipher_descriptor[ofb->cipher].done(&ofb->key);
   return CRYPT_OK;
}

   

#endif

/* $Source: /cvs/cable/Cow/userspace/public/apps/sshd/libtomcrypt/src/modes/ofb/ofb_done.c,v $ */
/* $Revision: 1.3 $ */
/* $Date: 2015/08/20 02:27:57 $ */
