#include <stdio.h>

int main(void)
{
   char buf[4096];
   int x;
   
   while (fgets(buf, sizeof(buf)-2, stdin) != NULL) {
        for (x = 0; x < 128; ) {
            printf("0x%c%c, ", buf[x], buf[x+1]);
            if (!((x += 2) & 31)) printf("\n");
        }
   }
}


/* $Source: /cvs/cable/Cow/userspace/public/apps/sshd/libtomcrypt/notes/etc/whirltest.c,v $ */
/* $Revision: 1.3 $ */
/* $Date: 2015/08/20 02:27:48 $ */
