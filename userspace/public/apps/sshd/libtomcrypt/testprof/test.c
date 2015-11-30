#include <tomcrypt_test.h>

void run_cmd(int res, int line, char *file, char *cmd)
{
   if (res != CRYPT_OK) {
      fprintf(stderr, "%s (%d)\n%s:%d:%s\n", error_to_string(res), res, file, line, cmd);
      exit(EXIT_FAILURE);
   }
}

/* $Source: /cvs/cable/Cow/userspace/public/apps/sshd/libtomcrypt/testprof/test.c,v $ */
/* $Revision: 1.3 $ */
/* $Date: 2015/08/20 02:28:01 $ */
