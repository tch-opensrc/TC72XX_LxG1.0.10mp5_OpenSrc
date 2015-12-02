#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

MODULE = Fakeroot::Stat		PACKAGE = Fakeroot::Stat

void
fakestat(key, path)
	char *key;
	char *path;
    PROTOTYPE: $$
    INIT:
	struct stat st;
	int	    i;
    PPCODE:
	/* Stat first, then fill in uid/gid from fakeroot (ignore everything
	 * else */
	i = stat(path, &st);
	if (i == 0) {
		if (key && strlen(key))
			fake_get_owner(0, key, path, &st.st_uid, &st.st_gid,
				       &st.st_mode);

		EXTEND(SP, 13);
		PUSHs(sv_2mortal(newSVnv(st.st_dev)));
		PUSHs(sv_2mortal(newSVnv(st.st_ino)));
		PUSHs(sv_2mortal(newSVnv(st.st_mode)));
		PUSHs(sv_2mortal(newSVnv(st.st_nlink)));
		PUSHs(sv_2mortal(newSVnv(st.st_uid)));
		PUSHs(sv_2mortal(newSVnv(st.st_gid)));
		PUSHs(sv_2mortal(newSVnv(st.st_rdev)));
		PUSHs(sv_2mortal(newSVnv(st.st_size)));
		PUSHs(sv_2mortal(newSVnv(st.st_atime)));
		PUSHs(sv_2mortal(newSVnv(st.st_mtime)));
		PUSHs(sv_2mortal(newSVnv(st.st_ctime)));
		PUSHs(sv_2mortal(newSVnv(st.st_blksize)));
		PUSHs(sv_2mortal(newSVnv(st.st_blocks)));
	}

void
fakelstat(key, path)
	char *key;
	char *path;
    PROTOTYPE: $$
    INIT:
	struct stat st;
	int	    i;
    PPCODE:
	/* Stat first, then fill in uid/gid from fakeroot (ignore everything
	 * else */
	i = lstat(path, &st);
	if (i == 0) {
		if (key && strlen(key))
			fake_get_owner(1, key, path, &st.st_uid, &st.st_gid,
				       &st.st_mode);

		EXTEND(SP, 13);
		PUSHs(sv_2mortal(newSVnv(st.st_dev)));
		PUSHs(sv_2mortal(newSVnv(st.st_ino)));
		PUSHs(sv_2mortal(newSVnv(st.st_mode)));
		PUSHs(sv_2mortal(newSVnv(st.st_nlink)));
		PUSHs(sv_2mortal(newSVnv(st.st_uid)));
		PUSHs(sv_2mortal(newSVnv(st.st_gid)));
		PUSHs(sv_2mortal(newSVnv(st.st_rdev)));
		PUSHs(sv_2mortal(newSVnv(st.st_size)));
		PUSHs(sv_2mortal(newSVnv(st.st_atime)));
		PUSHs(sv_2mortal(newSVnv(st.st_mtime)));
		PUSHs(sv_2mortal(newSVnv(st.st_ctime)));
		PUSHs(sv_2mortal(newSVnv(st.st_blksize)));
		PUSHs(sv_2mortal(newSVnv(st.st_blocks)));
	}
