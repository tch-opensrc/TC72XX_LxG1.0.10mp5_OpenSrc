#include <tomcrypt_test.h>

int main(void)
{
init_timer();
reg_algs();
time_keysched();
time_cipher();
time_cipher2();
time_cipher3();
time_hash();
time_macs();
time_encmacs();
time_prng();
time_mult();
time_sqr();
time_rsa();
time_ecc();
time_dh();
return EXIT_SUCCESS;

}

/* $Source: /cvs/cable/Cow/userspace/public/apps/sshd/libtomcrypt/demos/timing.c,v $ */
/* $Revision: 1.3 $ */
/* $Date: 2015/08/20 02:27:47 $ */
