#include <string.h>

extern int udhcpd(int argc, char *argv[]);
extern int udhcpc(int argc, char *argv[]);

#ifdef BUILD_STATIC
int dhcpd_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif

{
	int ret = 0;
	char *base = strrchr(argv[0], '/');
	
	if (strstr(base ? (base + 1) : argv[0], "dhcpd"))
		ret = udhcpd(argc, argv);
	else ret = udhcpc(argc, argv);
	
	return ret;
}
