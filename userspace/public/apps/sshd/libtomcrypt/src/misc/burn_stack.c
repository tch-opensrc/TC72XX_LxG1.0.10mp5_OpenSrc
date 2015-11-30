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
   @file burn_stack.c
   Burn stack, Tom St Denis
*/

/**
   Burn some stack memory
   @param len amount of stack to burn in bytes
*/
void burn_stack(unsigned long len)
{
   unsigned char buf[32];
   zeromem(buf, sizeof(buf));
   if (len > (unsigned long)sizeof(buf))
      burn_stack(len - sizeof(buf));
}



/* $Source: /cvs/cable/Cow/userspace/public/apps/sshd/libtomcrypt/src/misc/burn_stack.c,v $ */
/* $Revision: 1.3 $ */
/* $Date: 2015/08/20 02:27:54 $ */
