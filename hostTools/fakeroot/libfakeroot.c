/*
  Copyright: GPL. 
  Author: joost witteveen  (joostje@debian.org)
*/
/* #define _POSIX_C_SOURCE 199309L whatever that may mean...*/ 
/* #define _BSD_SOURCE             I use strdup, S_IFDIR, etc */ 

/* Roderich Schupp writes (bug #79100):
   /usr/include/dlfcn.h from libc6 2.2-5 defines RTLD_NEXT only
   when compiled with _GNU_SOURCE defined. Hence libfakeroot.c doesn't pick
   it
   up and does a dlopen("/lib/libc.so.6",...) in get_libc().
   This works most of the time, but explodes if you have an arch-optimized
   libc installed: the program now has two versions of libc.so
   (/lib/libc.so.6 and, say, /lib/i586/libc.so.6) mapped. Again for
   some programs you might get away with this, but running bash under
   fakeroot
   always bombs. Simple fix:
*/
#define _GNU_SOURCE 

#define FAKEROOT_LIBFAKEROOT

#include "config.h"
#include "communicate.h"

#ifdef STAT64_SUPPORT 
#define INT_STRUCT_STAT struct stat64
#define INT_NEXT_STAT(a,b,c) NEXT_STAT64(a,b,c)
#define INT_NEXT_LSTAT(a,b,c) NEXT_LSTAT64(a,b,c)
#define INT_NEXT_FSTAT(a,b,c) NEXT_FSTAT64(a,b,c)
#ifndef STUPID_ALPHA_HACK
#define INT_SEND_STAT(a,b) send_stat64(a,b)
#else
#define INT_SEND_STAT(a,b,c) send_stat64(a,b,c)
#endif
#else
#define INT_STRUCT_STAT struct stat
#define INT_NEXT_STAT(a,b,c) NEXT_STAT(a,b,c)
#define INT_NEXT_LSTAT(a,b,c) NEXT_LSTAT(a,b,c)
#define INT_NEXT_FSTAT(a,b,c) NEXT_FSTAT(a,b,c)
#ifndef STUPID_ALPHA_HACK
#define INT_SEND_STAT(a,b) send_stat(a,b)
#else
#define INT_SEND_STAT(a,b,c) send_stat(a,b,c)
#endif
#endif

#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <unistd.h> 
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#ifdef HAVE_SYS_ACL_H
#include <sys/acl.h>
#endif /* HAVE_SYS_ACL_H */

#if !HAVE_DECL_SETENV
extern int setenv (const char *name, const char *value, int replace);
#endif
#if !HAVE_DECL_UNSETENV
extern int unsetenv (const char *name);
#endif


/* 
   Where are those shared libraries? 
   If I knew of a configure/libtool way to find that out, I'd use it. Or
   any other way other than the method I'm using below. Does anybody know
   how I can get that location? (BTW, symply linking a programme, and running
   `ldd' on it isn't the option, as Digital Unix doesn't have ldd)
*/


/* 
   Note that LIBCPATH isn't actually used on Linux or Solaris, as RTLD_NEXT
   is defined and we use that to get the `next_*' functions

   Linux:
*/

/* OSF1 :*/
/*#define LIBCPATH "/usr/shlib/libc.so"*/

#undef __xstat
#undef __fxstat
#undef __lxstat
#undef __xstat64
#undef __fxstat64
#undef __lxstat64
#undef _FILE_OFFSET_BITS

/*
// next_wrap_st:
// this structure is used in next_wrap, which is defined in
// wrapstruct.h, included below
*/
 
struct next_wrap_st{
  void **doit;
  char *name;
};

void *get_libc(){
 
#ifndef RTLD_NEXT
 void *lib=0;
 if(!lib){ 
   lib= dlopen(LIBCPATH,RTLD_LAZY);
 }
 if (NULL==lib) {
   fprintf(stderr, "Couldn't find libc at: %s\n", LIBCPATH);
   abort();
 }
 return lib;
#else
  return RTLD_NEXT;
#endif
}
void load_library_symbols(void);

int fakeroot_disabled = 0;

#include "wrapped.h"
#include "wraptmpf.h"
#include "wrapdef.h"
#include "wrapstruct.h"


void load_library_symbols(void){
  /* this function loads all original functions from the C library.
     I ran into problems when  each function individually
     loaded it's original counterpart, as RTLD_NEXT seems to have
     a different meaning in files with different names than libtricks.c
     (I.E, dlsym(RTLD_NEXT, ...) called in vsearch.c returned funtions
     defined in libtricks */
  /* The calling of this function itself is somewhat tricky:
     the awk script wrapawk generates several .h files. In wraptmpf.h
     there are temporary definitions for tmp_*, that do the call
     to this function. The other generated .h files do even more tricky
     things :) */

  static int done=0;
  int i;
  char* msg;
  
  if(!done){
    for(i=0; next_wrap[i].doit; i++){
      *(next_wrap[i].doit)=dlsym(get_libc(), next_wrap[i].name);
      if ( (msg = dlerror()) != NULL){
	fprintf (stderr, "dlsym(%s): %s\n", next_wrap[i].name, msg);
/*	abort ();*/
      }
    }
  }
}


/*
 * Fake implementations for the setuid family of functions.
 * The fake IDs are inherited by child processes via environment variables.
 *
 * Issues:
 *   o Privileges are not checked, which results in incorrect behaviour.
 *     Example: process changes its (real, effective and saved) uid to 1000
 *     and then tries to regain root privileges.  This should normally result
 *     in an EPERM, but our implementation doesn't care...
 *   o If one of the setenv calls fails, the state may get corrupted.
 *   o Not thread-safe.
 */


/* Generic set/get ID functions */

static int env_get_id(const char *key) {
  char *str = getenv(key);
  if (str)
    return atoi(str);
  return 0;
}

static int env_set_id(const char *key, int id) {
  if (id == 0) {
    unsetenv(key);
    return 0;
  } else {
    char str[12];
    snprintf(str, sizeof (str), "%d", id);
    return setenv(key, str, 1);
  }
}

static void read_id(unsigned int *id, const char *key) {
  if (*id == (unsigned int)-1)
    *id = env_get_id(key);
}

static int write_id(const char *key, int id) {
  if (env_get_id(key) != id)
    return env_set_id(key, id);
  return 0;
}

/* Fake ID storage */

static uid_t faked_real_uid = -1;
static gid_t faked_real_gid = -1;
static uid_t faked_effective_uid = -1;
static gid_t faked_effective_gid = -1;
static uid_t faked_saved_uid = -1;
static gid_t faked_saved_gid = -1;
static uid_t faked_fs_uid = -1;
static gid_t faked_fs_gid = -1;

/* Read user ID */

static void read_real_uid() {
  read_id(&faked_real_uid, FAKEROOTUID_ENV);
}

static void read_effective_uid() {
  read_id(&faked_effective_uid, FAKEROOTEUID_ENV);
}

static void read_saved_uid() {
  read_id(&faked_saved_uid, FAKEROOTSUID_ENV);
}

static void read_fs_uid() {
  read_id(&faked_fs_uid, FAKEROOTFUID_ENV);
}

static void read_uids() {
  read_real_uid();
  read_effective_uid();
  read_saved_uid();
  read_fs_uid();
}

/* Read group ID */

static void read_real_gid() {
  read_id(&faked_real_gid, FAKEROOTGID_ENV);
}

static void read_effective_gid() {
  read_id(&faked_effective_gid, FAKEROOTEGID_ENV);
}

static void read_saved_gid() {
  read_id(&faked_saved_gid, FAKEROOTSGID_ENV);
}

static void read_fs_gid() {
  read_id(&faked_fs_gid, FAKEROOTFGID_ENV);
}

static void read_gids() {
  read_real_gid();
  read_effective_gid();
  read_saved_gid();
  read_fs_gid();
}

/* Write user ID */

static int write_real_uid() {
  return write_id(FAKEROOTUID_ENV, faked_real_uid);
}

static int write_effective_uid() {
  return write_id(FAKEROOTEUID_ENV, faked_effective_uid);
}

static int write_saved_uid() {
  return write_id(FAKEROOTSUID_ENV, faked_saved_uid);
}

static int write_fs_uid() {
  return write_id(FAKEROOTFUID_ENV, faked_fs_uid);
}

static int write_uids() {
  if (write_real_uid() < 0)
    return -1;
  if (write_effective_uid() < 0)
    return -1;
  if (write_saved_uid() < 0)
    return -1;
  if (write_fs_uid() < 0)
    return -1;
  return 0;
}

/* Write group ID */

static int write_real_gid() {
  return write_id(FAKEROOTGID_ENV, faked_real_gid);
}

static int write_effective_gid() {
  return write_id(FAKEROOTEGID_ENV, faked_effective_gid);
}

static int write_saved_gid() {
  return write_id(FAKEROOTSGID_ENV, faked_saved_gid);
}

static int write_fs_gid() {
  return write_id(FAKEROOTFGID_ENV, faked_fs_gid);
}

static int write_gids() {
  if (write_real_gid() < 0)
    return -1;
  if (write_effective_gid() < 0)
    return -1;
  if (write_saved_gid() < 0)
    return -1;
  if (write_fs_gid() < 0)
    return -1;
  return 0;
}

/* Faked get functions */

static uid_t get_faked_uid() {
  read_real_uid();
  return faked_real_uid;
}

static gid_t get_faked_gid() {
  read_real_gid();
  return faked_real_gid;
}

static uid_t get_faked_euid() {
  read_effective_uid();
  return faked_effective_uid;
}

static gid_t get_faked_egid() {
  read_effective_gid();
  return faked_effective_gid;
}

static uid_t get_faked_suid() {
  read_saved_uid();
  return faked_saved_uid;
}

static gid_t get_faked_sgid() {
  read_saved_gid();
  return faked_saved_gid;
}

static uid_t get_faked_fsuid() {
  read_fs_uid();
  return faked_fs_uid;
}

static gid_t get_faked_fsgid() {
  read_fs_gid();
  return faked_fs_gid;
}

/* Faked set functions */

static int set_faked_uid(uid_t uid) {
  read_uids();
  if (faked_effective_uid == 0) {
    faked_real_uid = uid;
    faked_effective_uid = uid;
    faked_saved_uid = uid;
  } else {
    faked_effective_uid = uid;
  }
  faked_fs_uid = uid;
  return write_uids();
}

static int set_faked_gid(gid_t gid) {
  read_gids();
  if (faked_effective_gid == 0) {
    faked_real_gid = gid;
    faked_effective_gid = gid;
    faked_saved_gid = gid;
  } else {
    faked_effective_gid = gid;
  }
  faked_fs_gid = gid;
  return write_gids();
}

static int set_faked_euid(uid_t euid) {
  read_effective_uid();
  faked_effective_uid = euid;
  read_fs_uid();
  faked_fs_uid = euid;
  if (write_effective_uid() < 0)
    return -1;
  if (write_fs_uid() < 0)
    return -1;
  return 0;
}

static int set_faked_egid(gid_t egid) {
  read_effective_gid();
  faked_effective_gid = egid;
  read_fs_gid();
  faked_fs_gid = egid;
  if (write_effective_gid() < 0)
    return -1;
  if (write_fs_gid() < 0)
    return -1;
  return 0;
}

static int set_faked_reuid(uid_t ruid, uid_t euid) {
  read_uids();
  if (ruid != (uid_t)-1 || euid != (uid_t)-1)
    faked_saved_uid = faked_effective_uid;
  if (ruid != (uid_t)-1)
    faked_real_uid = ruid;
  if (euid != (uid_t)-1)
    faked_effective_uid = euid;
  faked_fs_uid = faked_effective_uid;
  return write_uids();
}

static int set_faked_regid(gid_t rgid, gid_t egid) {
  read_gids();
  if (rgid != (gid_t)-1 || egid != (gid_t)-1)
    faked_saved_gid = faked_effective_gid;
  if (rgid != (gid_t)-1)
    faked_real_gid = rgid;
  if (egid != (gid_t)-1)
    faked_effective_gid = egid;
  faked_fs_gid = faked_effective_gid;
  return write_gids();
}

#ifdef HAVE_SETRESUID
static int set_faked_resuid(uid_t ruid, uid_t euid, uid_t suid) {
  read_uids();
  if (ruid != (uid_t)-1)
    faked_real_uid = ruid;
  if (euid != (uid_t)-1)
    faked_effective_uid = euid;
  if (suid != (uid_t)-1)
    faked_saved_uid = suid;
  faked_fs_uid = faked_effective_uid;
  return write_uids();
}
#endif

#ifdef HAVE_SETRESGID
static int set_faked_resgid(gid_t rgid, gid_t egid, gid_t sgid) {
  read_gids();
  if (rgid != (gid_t)-1)
    faked_real_gid = rgid;
  if (egid != (gid_t)-1)
    faked_effective_gid = egid;
  if (sgid != (gid_t)-1)
    faked_saved_gid = sgid;
  faked_fs_gid = faked_effective_gid;
  return write_gids();
}
#endif

#ifdef HAVE_SETFSUID
static uid_t set_faked_fsuid(uid_t fsuid) {
  uid_t prev_fsuid = get_faked_fsuid();
  faked_fs_uid = fsuid;
  return prev_fsuid;
}
#endif

#ifdef HAVE_SETFSGID
static gid_t set_faked_fsgid(gid_t fsgid) {
  gid_t prev_fsgid = get_faked_fsgid();
  faked_fs_gid = fsgid;
  return prev_fsgid;
}
#endif


static int dont_try_chown(){
  static int inited=0;
  static int donttry;

  if(!inited){
    donttry=(env_var_set(FAKEROOTDONTTRYCHOWN_ENV)!=NULL);
    inited=1;
  }
  return donttry;
}


/* The wrapped functions */


int WRAP_LSTAT LSTAT_ARG(int ver, 
		       const char *file_name, 
		       struct stat *statbuf){

  int r;

  r=NEXT_LSTAT(ver, file_name, statbuf);
  if(r)
    return -1;
#ifndef STUPID_ALPHA_HACK
  send_get_stat(statbuf);
#else
  send_get_stat(statbuf, ver);
#endif
  return 0;
}


int WRAP_STAT STAT_ARG(int ver, 
		       const char *file_name, 
		       struct stat *st){
  int r;

  r=NEXT_STAT(ver, file_name, st);
  if(r)
    return -1;
#ifndef STUPID_ALPHA_HACK
  send_get_stat(st);
#else
  send_get_stat(st,ver);
#endif
  return 0;
}


int WRAP_FSTAT FSTAT_ARG(int ver, 
			int fd, 
			struct stat *st){


  int r;

  r=NEXT_FSTAT(ver, fd, st);
  if(r)
    return -1;
#ifndef STUPID_ALPHA_HACK
  send_get_stat(st);
#else
  send_get_stat(st,ver);
#endif
  return 0;
}

#ifdef HAVE_FSTATAT
int WRAP_FSTATAT FSTATAT_ARG(int ver,
			     int dir_fd,
			     const char *path,
			     struct stat *st,
			     int flags){


  int r;

  r=NEXT_FSTATAT(ver, dir_fd, path, st, flags);
  if(r)
    return -1;
#ifndef STUPID_ALPHA_HACK
  send_get_stat(st);
#else
  send_get_stat(st,ver);
#endif
  return 0;
}
#endif /* HAVE_FSTATAT */

#ifdef STAT64_SUPPORT

int WRAP_LSTAT64 LSTAT64_ARG (int ver, 
			   const char *file_name, 
			   struct stat64 *st){

  int r;

  r=NEXT_LSTAT64(ver, file_name, st);

  if(r)
    return -1;

#ifndef STUPID_ALPHA_HACK
  send_get_stat64(st);
#else
  send_get_stat64(st,ver);
#endif
  return 0;
}


int WRAP_STAT64 STAT64_ARG(int ver, 
			   const char *file_name, 
			   struct stat64 *st){
  int r;

  r=NEXT_STAT64(ver,file_name,st);
  if(r)
    return -1;
#ifndef STUPID_ALPHA_HACK
  send_get_stat64(st);
#else
  send_get_stat64(st,ver);
#endif
  return 0;
}


int WRAP_FSTAT64 FSTAT64_ARG(int ver, 
			     int fd, 
			     struct stat64 *st){
  int r;

  r=NEXT_FSTAT64(ver, fd, st);
  if(r)
    return -1;
#ifndef STUPID_ALPHA_HACK
  send_get_stat64(st);
#else
  send_get_stat64(st,ver);
#endif

  return 0;
}

#ifdef HAVE_FSTATAT
int WRAP_FSTATAT64 FSTATAT64_ARG(int ver,
				 int dir_fd,
				 const char *path,
				 struct stat64 *st,
				 int flags){


  int r;

  r=NEXT_FSTATAT64(ver, dir_fd, path, st, flags);
  if(r)
    return -1;
#ifndef STUPID_ALPHA_HACK
  send_get_stat64(st);
#else
  send_get_stat64(st,ver);
#endif
  return 0;
}
#endif /* HAVE_FSTATAT */

#endif /* STAT64_SUPPORT */




/*************************************************************/
/*
  Wrapped functions general info:
  
  In general, the structure is as follows:
    - Then, if the function does things that (possibly) fail by
      other users than root, allow for `fake' root privileges.
      Do this by obtaining the inode the function works on, and then
      informing faked (the deamon that remembers all `fake' file
      permissions e.d.) about the intentions of the user.
      Faked maintains a list of inodes and their respective
      `fake' ownerships/filemodes.
    - Or, if the function requests information that we should
      fake, again get the inode of the file, and ask faked about the
      ownership/filemode data it maintains for that inode.

*/
/*************************************************************/



/* chown, lchown, fchown, chmod, fchmod, mknod functions
   
   quite general. See the `Wrapped functions general info:' above
   for more info.
 */

int chown(const char *path, uid_t owner, gid_t group){
  INT_STRUCT_STAT st;
  int r=0;


#ifdef LCHOWN_SUPPORT
  /*chown(sym-link) works on the target of the symlink if lchown is
    present and enabled.*/
  r=INT_NEXT_STAT(_STAT_VER, path, &st);
#else
  /*chown(sym-link) works on the symlink itself, use lstat: */
  r=INT_NEXT_LSTAT(_STAT_VER, path, &st);
#endif
  
  if(r)
    return r;
  st.st_uid=owner;
  st.st_gid=group;
#ifndef STUPID_ALPHA_HACK
  INT_SEND_STAT(&st,chown_func);
#else
  INT_SEND_STAT(&st,chown_func, _STAT_VER);
#endif
  if(!dont_try_chown())
    r=next_lchown(path,owner,group);
  else
    r=0;
  if(r&&(errno==EPERM))
    r=0;
  
  return r;
}


#ifdef LCHOWN_SUPPORT
int lchown(const char *path, uid_t owner, gid_t group){
  struct stat st;
  int r=0;

  r=NEXT_LSTAT(_STAT_VER, path, &st);
  if(r)
    return r;
  st.st_uid=owner;
  st.st_gid=group;
#ifndef STUPID_ALPHA_HACK
  send_stat(&st,chown_func);
#else
  send_stat(&st,chown_func, _STAT_VER);
#endif
  if(!dont_try_chown())
    r=next_lchown(path,owner,group);
  else
    r=0;
  if(r&&(errno==EPERM))
    r=0;

  return r;
}
#endif

int fchown(int fd, uid_t owner, gid_t group){
  struct stat st;
  int r;

  r=NEXT_FSTAT(_STAT_VER, fd, &st);
  if(r)
    return r;
  
  st.st_uid=owner;
  st.st_gid=group;
#ifndef STUPID_ALPHA_HACK
  send_stat(&st, chown_func);
#else
  send_stat(&st, chown_func, _STAT_VER);
#endif
  
  if(!dont_try_chown())
    r=next_fchown(fd,owner,group);
  else
    r=0;
  
  if(r&&(errno==EPERM))
    r=0;
  
  return r;
}

#ifdef HAVE_FSTATAT
#ifdef HAVE_FCHOWNAT
int fchownat(int dir_fd, const char *path, uid_t owner, gid_t group, int flags) {
  int r;
  /* If AT_SYMLINK_NOFOLLOW is set in the fchownat call it should
     be when we stat it. */
#ifdef STAT64_SUPPORT
  struct stat64 st;
  r=NEXT_FSTATAT64(_STAT_VER, dir_fd, path, &st, (flags & AT_SYMLINK_NOFOLLOW));
#else
  struct stat st;
  r=NEXT_FSTATAT(_STAT_VER, dir_fd, path, &st, (flags & AT_SYMLINK_NOFOLLOW));
#endif
  
  if(r)
    return(r);
  
  st.st_uid=owner;
  st.st_gid=group;
#ifdef STAT64_SUPPORT
#ifndef STUPID_ALPHA_HACK
  send_stat64(&st,chown_func);  
#else
  send_stat64(&st,chown_func, _STAT_VER);  
#endif
#else
#ifndef STUPID_ALPHA_HACK
  send_stat(&st,chown_func);  
#else
  send_stat(&st,chown_func, _STAT_VER);  
#endif
#endif /* STAT64_SUPPORT */
  
  if(!dont_try_chown())
    r=next_fchownat(dir_fd,path,owner,group,flags);
  else
    r=0;
  
  if(r&&(errno==EPERM))
    r=0;
  
  return r;
}
#endif /* HAVE_FCHOWNAT */
#endif /* HAVE_FSTATAT */

int chmod(const char *path, mode_t mode){
  INT_STRUCT_STAT st;
  int r;

  r=INT_NEXT_STAT(_STAT_VER, path, &st);
  if(r)
    return r;
  
  st.st_mode=(mode&ALLPERMS)|(st.st_mode&~ALLPERMS);
    
#ifndef STUPID_ALPHA_HACK
  INT_SEND_STAT(&st, chmod_func);
#else
  INT_SEND_STAT(&st, chmod_func, _STAT_VER);
#endif
  
  /* if a file is unwritable, then root can still write to it
     (no matter who owns the file). If we are fakeroot, the only
     way to fake this is to always make the file writable, readable
     etc for the real user (who started fakeroot). Also holds for
     the exec bit of directories.
     Yes, packages requering that are broken. But we have lintian
     to get rid of broken packages, not fakeroot.
  */
  mode |= 0600;
  if(S_ISDIR(st.st_mode))
    mode |= 0100;
  
  r=next_chmod(path, mode);
  if(r&&(errno==EPERM))
    r=0;
  return r;
}

int fchmod(int fd, mode_t mode){
  int r;
  INT_STRUCT_STAT st;


  r=INT_NEXT_FSTAT(_STAT_VER, fd, &st);
  
  if(r)
    return(r);
  
  st.st_mode=(mode&ALLPERMS)|(st.st_mode&~ALLPERMS);
#ifndef STUPID_ALPHA_HACK
  INT_SEND_STAT(&st,chmod_func);  
#else
  INT_SEND_STAT(&st,chmod_func, _STAT_VER);  
#endif
  
  /* see chmod() for comment */
  mode |= 0600;
  if(S_ISDIR(st.st_mode))
    mode |= 0100;
  
  r=next_fchmod(fd, mode);
  if(r&&(errno==EPERM))
    r=0;
  return r;
}

#ifdef HAVE_FSTATAT
#ifdef HAVE_FCHMODAT
int fchmodat(int dir_fd, const char *path, mode_t mode, int flags) {
/*   (int fd, mode_t mode){*/
  int r;
  struct stat st;

  /* If AT_SYMLINK_NOFOLLOW is set in the fchownat call it should
     be when we stat it. */
  r=NEXT_FSTATAT(_STAT_VER, dir_fd, path, &st, flags & AT_SYMLINK_NOFOLLOW);
  
  if(r)
    return(r);
  
  st.st_mode=(mode&ALLPERMS)|(st.st_mode&~ALLPERMS);
#ifndef STUPID_ALPHA_HACK
  send_stat(&st,chmod_func);  
#else
  send_stat(&st,chmod_func, _STAT_VER);  
#endif
  
  /* see chmod() for comment */
  mode |= 0600;
  if(S_ISDIR(st.st_mode))
    mode |= 0100;
  
  r=next_fchmodat(dir_fd, path, mode, flags);
  if(r&&(errno==EPERM))
    r=0;
  return r;
}
#endif /* HAVE_FCHMODAT */
#endif /* HAVE_FSTATAT */

int WRAP_MKNOD MKNOD_ARG(int ver UNUSED,
			 const char *pathname, 
			 mode_t mode, dev_t XMKNOD_FRTH_ARG dev)
{
  struct stat st;
  mode_t old_mask=umask(022);
  int fd,r;

  umask(old_mask);
  
  /*Don't bother to mknod the file, that probably doesn't work.
    just create it as normal file, and leave the premissions
    to the fakemode.*/
  
  fd=open(pathname, O_WRONLY|O_CREAT|O_TRUNC, 00644);

  if(fd==-1)
    return -1;
  
  close(fd);
  /* get the inode, to communicate with faked */

  r=NEXT_LSTAT(_STAT_VER, pathname, &st);

  if(r)
    return -1;
  
  st.st_mode= mode & ~old_mask;
  st.st_rdev= XMKNOD_FRTH_ARG dev;
  
#ifndef STUPID_ALPHA_HACK
  send_stat(&st,mknod_func);
#else
  send_stat(&st,mknod_func, _STAT_VER);
#endif
    
  return 0;
}

#ifdef HAVE_FSTATAT
#ifdef HAVE_MKNODAT
int WRAP_MKNODAT MKNODAT_ARG(int ver UNUSED,
			     int dir_fd,
			     const char *pathname, 
			     mode_t mode, dev_t XMKNODAT_FIFTH_ARG dev)
{
  struct stat st;
  mode_t old_mask=umask(022);
  int fd,r;

  umask(old_mask);
  
  /*Don't bother to mknod the file, that probably doesn't work.
    just create it as normal file, and leave the permissions
    to the fakemode.*/

  fd=openat(dir_fd, pathname, O_WRONLY|O_CREAT|O_TRUNC, 00644);

  if(fd==-1)
    return -1;
  
  close(fd);
  /* get the inode, to communicate with faked */

  /* The only known flag is AT_SYMLINK_NOFOLLOW and
     we don't want that here. */
  r=NEXT_FSTATAT(_STAT_VER, dir_fd, pathname, &st, 0);

  if(r)
    return -1;
  
  st.st_mode= mode & ~old_mask;
  st.st_rdev= XMKNODAT_FIFTH_ARG dev;
  
#ifndef STUPID_ALPHA_HACK
  send_stat(&st,mknod_func);
#else
  send_stat(&st,mknod_func, _STAT_VER);
#endif
    
  return 0;
}
#endif /* HAVE_MKNODAT */
#endif /* HAVE_FSTATAT */

int mkdir(const char *path, mode_t mode){
  INT_STRUCT_STAT st;
  int r;
  mode_t old_mask=umask(022);

  umask(old_mask);


  /* we need to tell the fake deamon the real mode. In order
     to communicate with faked we need a struct stat, so we now
     do a stat of the new directory (just for the inode/dev) */

  r=next_mkdir(path, mode|0700); 
  /* mode|0700: see comment in the chown() function above */
  if(r)
    return -1;
  r=INT_NEXT_STAT(_STAT_VER, path, &st);
  
  if(r)
    return -1;
  
  st.st_mode=(mode&~old_mask&ALLPERMS)|(st.st_mode&~ALLPERMS)|S_IFDIR;

#ifndef STUPID_ALPHA_HACK
  INT_SEND_STAT(&st, chmod_func);
#else
  INT_SEND_STAT(&st, chmod_func, _STAT_VER);
#endif

  return 0;
}

#ifdef HAVE_FSTATAT
#ifdef HAVE_MKDIRAT
int mkdirat(int dir_fd, const char *path, mode_t mode){
  struct stat st;
  int r;
  mode_t old_mask=umask(022);

  umask(old_mask);


  /* we need to tell the fake deamon the real mode. In order
     to communicate with faked we need a struct stat, so we now
     do a stat of the new directory (just for the inode/dev) */

  r=next_mkdirat(dir_fd, path, mode|0700); 
  /* mode|0700: see comment in the chown() function above */
  if(r)
    return -1;
  r=NEXT_FSTATAT(_STAT_VER, dir_fd, path, &st, 0);

  if(r)
    return -1;
  
  st.st_mode=(mode&~old_mask&ALLPERMS)|(st.st_mode&~ALLPERMS)|S_IFDIR;

#ifndef STUPID_ALPHA_HACK
  send_stat(&st, chmod_func);
#else
  send_stat(&st, chmod_func, _STAT_VER);
#endif

  return 0;
}
#endif /* HAVE_MKDIRAT */
#endif /* HAVE_FSTATAT */

/* 
   The remove funtions: unlink, rmdir, rename.
   These functions can all remove inodes from the system.
   I need to inform faked about the removal of these inodes because
   of the following:
    # rm -f file
    # touch file 
    # chown admin file
    # rm file
    # touch file
   In the above example, assuming that for both touch-es, the same
   inode is generated, faked will still report the owner of `file'
   as `admin', unless it's informed about the removal of the inode.   
*/

int unlink(const char *pathname){
  int r;
  INT_STRUCT_STAT st;


  r=INT_NEXT_LSTAT(_STAT_VER, pathname, &st);
  if(r)
    return -1;

  r=next_unlink(pathname);  

  if(r)
    return -1;
  
#ifndef STUPID_ALPHA_HACK
  INT_SEND_STAT(&st, unlink_func);
#else
  INT_SEND_STAT(&st, unlink_func, _STAT_VER);
#endif
  
  return 0;
}

#ifdef HAVE_FSTATAT
#ifdef HAVE_UNLINKAT
int unlinkat(int dir_fd, const char *pathname, int flags){
  int r;
#ifdef STAT64_SUPPORT
  struct stat64 st;
  r=NEXT_FSTATAT64(_STAT_VER, dir_fd, pathname, &st, (flags&~AT_REMOVEDIR) | AT_SYMLINK_NOFOLLOW);
#else
  struct stat st;
  r=NEXT_FSTATAT(_STAT_VER, dir_fd, pathname, &st, (flags&~AT_REMOVEDIR) | AT_SYMLINK_NOFOLLOW);
#endif
  if(r)
    return -1;

  r=next_unlinkat(dir_fd, pathname, flags);

  if(r)
    return -1;
  
#ifdef STAT64_SUPPORT
#ifndef STUPID_ALPHA_HACK
  send_stat64(&st, unlink_func);
#else
  send_stat64(&st, unlink_func, _STAT_VER);
#endif
#else
  send_stat(&st, unlink_func);
#endif
  
  return 0;
}
#endif /* HAVE_UNLINKAT */
#endif /* HAVE_FSTATAT */

/*
  See the `remove funtions:' comments above for more info on
  these remove function wrappers.
*/
int rmdir(const char *pathname){
  int r;
  INT_STRUCT_STAT st;

  r=INT_NEXT_LSTAT(_STAT_VER, pathname, &st);
  if(r)
    return -1;
  r=next_rmdir(pathname);  
  if(r)
    return -1;

#ifndef STUPID_ALPHA_HACK
  INT_SEND_STAT(&st,unlink_func);  
#else
  INT_SEND_STAT(&st,unlink_func, _STAT_VER);  
#endif

  return 0;
}

/*
  See the `remove funtions:' comments above for more info on
  these remove function wrappers.
*/
int remove(const char *pathname){
  int r;
  INT_STRUCT_STAT st;

  r=INT_NEXT_LSTAT(_STAT_VER, pathname, &st);
  if(r)
    return -1;
  r=next_remove(pathname);  
  if(r)
    return -1;
#ifndef STUPID_ALPHA_HACK
  INT_SEND_STAT(&st,unlink_func);  
#else
  INT_SEND_STAT(&st,unlink_func, _STAT_VER);  
#endif
  
  return r;
}

/*
  if the second argument to the rename() call points to an
  existing file, then that file will be removed. So, we have
  to treat this function as one of the `remove functions'.

  See the `remove funtions:' comments above for more info on
  these remove function wrappers.
*/

int rename(const char *oldpath, const char *newpath){
  int r,s;
  INT_STRUCT_STAT st;     

  /* If newpath points to an existing file, that file will be 
     unlinked.   Make sure we tell the faked daemon about this! */

  /* we need the st_new struct in order to inform faked about the
     (possible) unlink of the file */

  r=INT_NEXT_LSTAT(_STAT_VER, newpath, &st);

  s=next_rename(oldpath, newpath);
  if(s)
    return -1;
  if(!r)
#ifndef STUPID_ALPHA_HACK
    INT_SEND_STAT(&st,unlink_func);
#else
    INT_SEND_STAT(&st,unlink_func, _STAT_VER);
#endif

  return 0;
}

#ifdef HAVE_FSTATAT
#ifdef HAVE_RENAMEAT
int renameat(int olddir_fd, const char *oldpath,
             int newdir_fd, const char *newpath){
  int r,s;
  struct stat st;     

  /* If newpath points to an existing file, that file will be 
     unlinked.   Make sure we tell the faked daemon about this! */

  /* we need the st_new struct in order to inform faked about the
     (possible) unlink of the file */

  r=NEXT_FSTATAT(_STAT_VER, newdir_fd, newpath, &st, AT_SYMLINK_NOFOLLOW);

  s=next_renameat(olddir_fd, oldpath, newdir_fd, newpath);
  if(s)
    return -1;
  if(!r)
#ifndef STUPID_ALPHA_HACK
    send_stat(&st,unlink_func);
#else
    send_stat(&st,unlink_func, _STAT_VER);
#endif

  return 0;
}
#endif /* HAVE_RENAMEAT */
#endif /* HAVE_FSTATAT */


#ifdef FAKEROOT_FAKENET
pid_t fork(void)
{
  pid_t pid;

  pid = next_fork();

  if (pid == 0) {
    /* No need to lock in the child process. */
    if (comm_sd >= 0) {
      next_close(comm_sd);
      comm_sd = -1;
    }
  }

  return pid;
}

pid_t vfork(void)
{
  /* We can't wrap vfork(2) without breaking everything... */
  return fork();
}

/* Return an error when trying to close the comm_sd file descriptor
   (pretend that it's closed). */
int close(int fd)
{
  int retval, reterr;

  lock_comm_sd();

  if (comm_sd >= 0 && comm_sd == fd) {
    retval = -1;
    reterr = EBADF;
  } else {
    retval = next_close(fd);
    reterr = errno;
  }

  unlock_comm_sd();

  errno = reterr;
  return retval;
}

int dup2(int oldfd, int newfd)
{
  int retval, reterr;

  lock_comm_sd();

  if (comm_sd >= 0 && comm_sd == newfd) {
    /* If dup fails, comm_sd gets set to -1, which is fine. */
    comm_sd = dup(newfd);
    next_close(newfd);
  }

  retval = next_dup2(oldfd, newfd);
  reterr = errno;

  unlock_comm_sd();

  errno = reterr;
  return retval;
}
#endif /* FAKEROOT_FAKENET */

uid_t getuid(void){
  if (fakeroot_disabled)
    return next_getuid();
  return get_faked_uid();
}

uid_t geteuid(void){
  if (fakeroot_disabled)
    return next_geteuid();
  return get_faked_euid();
}

#ifdef HAVE_GETRESUID
int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid){
  if (fakeroot_disabled)
    return next_getresuid(ruid, euid, suid);
  *ruid = get_faked_uid();
  *euid = get_faked_euid();
  *suid = get_faked_suid();
  return 0;
}
#endif /* HAVE_GETRESUID */

uid_t getgid(void){
  if (fakeroot_disabled)
    return next_getgid();
  return get_faked_gid();
}

uid_t getegid(void){
  if (fakeroot_disabled)
    return next_getegid();
  return get_faked_egid();
}

#ifdef HAVE_GETRESGID
int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid){
  if (fakeroot_disabled)
    return next_getresgid(rgid, egid, sgid);
  *rgid = get_faked_gid();
  *egid = get_faked_egid();
  *sgid = get_faked_sgid();
  return 0;
}
#endif /* HAVE_GETRESGID */

int setuid(uid_t id){
  if (fakeroot_disabled)
    return next_setuid(id);
  return set_faked_uid(id);
}

int setgid(uid_t id){
  if (fakeroot_disabled)
    return next_setgid(id);
  return set_faked_gid(id);
}

int seteuid(uid_t id){
  if (fakeroot_disabled)
    return next_seteuid(id);
  return set_faked_euid(id);
}

int setegid(uid_t id){
  if (fakeroot_disabled)
    return next_setegid(id);
  return set_faked_egid(id);
}

int setreuid(SETREUID_ARG ruid, SETREUID_ARG euid){
  if (fakeroot_disabled)
    return next_setreuid(ruid, euid);
  return set_faked_reuid(ruid, euid);
}

int setregid(SETREGID_ARG rgid, SETREGID_ARG egid){
  if (fakeroot_disabled)
    return next_setregid(rgid, egid);
  return set_faked_regid(rgid, egid);
}

#ifdef HAVE_SETRESUID
int setresuid(uid_t ruid, uid_t euid, uid_t suid){
  if (fakeroot_disabled)
    return next_setresuid(ruid, euid, suid);
  return set_faked_resuid(ruid, euid, suid);
}
#endif /* HAVE_SETRESUID */

#ifdef HAVE_SETRESGID
int setresgid(gid_t rgid, gid_t egid, gid_t sgid){
  if (fakeroot_disabled)
    return next_setresgid(rgid, egid, sgid);
  return set_faked_resgid(rgid, egid, sgid);
}
#endif /* HAVE_SETRESGID */

#ifdef HAVE_SETFSUID
uid_t setfsuid(uid_t fsuid){
  if (fakeroot_disabled)
    return next_setfsuid(fsuid);
  return set_faked_fsuid(fsuid);
}
#endif /* HAVE_SETFSUID */

#ifdef HAVE_SETFSGID
gid_t setfsgid(gid_t fsgid){
  if (fakeroot_disabled)
    return next_setfsgid(fsgid);
  return set_faked_fsgid(fsgid);
}
#endif /* HAVE_SETFSGID */

int initgroups(const char* user, INITGROUPS_SECOND_ARG group){
  if (fakeroot_disabled)
    return next_initgroups(user, group);
  else
    return 0;
}

int setgroups(SETGROUPS_SIZE_TYPE size, const gid_t *list){
  if (fakeroot_disabled)
    return next_setgroups(size, list);
  else
    return 0;
}

int fakeroot_disable(int new)
{
  int old = fakeroot_disabled;
  fakeroot_disabled = new ? 1 : 0;
  return old;
}

int fakeroot_isdisabled(void)
{
  return fakeroot_disabled;
}

#ifdef HAVE_SYS_ACL_H
int acl_set_fd(int fd, acl_t acl) {
  errno = ENOTSUP;
  return -1;
}

int acl_set_file(const char *path_p, acl_type_t type, acl_t acl) {
  errno = ENOTSUP;
  return -1;
}
#endif /* HAVE_SYS_ACL_H */
