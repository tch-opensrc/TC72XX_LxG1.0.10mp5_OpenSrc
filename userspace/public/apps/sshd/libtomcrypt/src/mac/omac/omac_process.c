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
  @file omac_process.c
  OMAC1 support, process data, Tom St Denis
*/


#ifdef OMAC

/** 
   Process data through OMAC
   @param omac     The OMAC state
   @param in       The input data to send through OMAC
   @param inlen    The length of the input (octets)
   @return CRYPT_OK if successful
*/
int omac_process(omac_state *omac, const unsigned char *in, unsigned long inlen)
{
   unsigned long n, x;
   int           err;

   LTC_ARGCHK(omac  != NULL);
   LTC_ARGCHK(in    != NULL);
   if ((err = cipher_is_valid(omac->cipher_idx)) != CRYPT_OK) {
      return err;
   }

   if ((omac->buflen > (int)sizeof(omac->block)) || (omac->buflen < 0) ||
       (omac->blklen > (int)sizeof(omac->block)) || (omac->buflen > omac->blklen)) {
      return CRYPT_INVALID_ARG;
   }

#ifdef LTC_FAST
   if (omac->buflen == 0 && inlen > 16) {
      int y;
      for (x = 0; x < (inlen - 16); x += 16) {
          for (y = 0; y < 16; y += sizeof(LTC_FAST_TYPE)) {
              *((LTC_FAST_TYPE*)(&omac->prev[y])) ^= *((LTC_FAST_TYPE*)(&in[y]));
          }
          in += 16;
          cipher_descriptor[omac->cipher_idx].ecb_encrypt(omac->prev, omac->prev, &omac->key);
      }
      inlen -= x;
    }
#endif

   while (inlen != 0) { 
       /* ok if the block is full we xor in prev, encrypt and replace prev */
       if (omac->buflen == omac->blklen) {
          for (x = 0; x < (unsigned long)omac->blklen; x++) {
              omac->block[x] ^= omac->prev[x];
          }
          cipher_descriptor[omac->cipher_idx].ecb_encrypt(omac->block, omac->prev, &omac->key);
          omac->buflen = 0;
       }

       /* add bytes */
       n = MIN(inlen, (unsigned long)(omac->blklen - omac->buflen));
       XMEMCPY(omac->block + omac->buflen, in, n);
       omac->buflen  += n;
       inlen         -= n;
       in            += n;
   }

   return CRYPT_OK;
}

#endif


/* $Source: /cvs/cable/Cow/userspace/public/apps/sshd/libtomcrypt/src/mac/omac/omac_process.c,v $ */
/* $Revision: 1.3 $ */
/* $Date: 2015/08/20 02:27:53 $ */
