Here only binaries of the gdb and gdbserver (compiled from  gdb-6.8) are
present.

if you want to compile the binaries yourself , download the gdb source
and untar it in your home directory

>cd gdb-x.x/

>./configure  --target=mips-linux-uclibc --prefix=/opt/toolchains/uclibc-crosstools-gcc-4.2.3-3/usr --disable-werror

>make
 
>cp gdb/gdb  /xx/../xx/Broadcom/devbranch/CommEngine/userspace/gpl/apps/gdb/mips-linux-uclibc-gdb


>cd gdb/gdbserver/

>export CC=/opt/toolchains/uclibc-crosstools-gcc-4.2.3-3/usr/bin/mips-linux-uclibc-gcc

>./configure --host=mips-linux-uclibc --target=mips-linux-uclibc --with-sysroot=/opt/toolchains/uclibc-crosstools-gcc-4.2.3-3/usr/lib --prefix=/opt/toolchains/uclibc-crosstools-gcc-4.2.3-3/usr

>make

>cp gdbserver  /xx/../xx/Broadcom/devbranch/CommEngine/userspace/gpl/apps/gdb/gdbserver




Note:The current toolchain(Broadcom_4.2.3) does not have libthread_db library, this library is needed
if you want to compile gdb with thread support(i.e to debug applications with threads).  

To add libthread_db to toolchain you need  rebuild the toolchain as described in Howto.txt and selecting the
 "Build Pthreads Debugging Support" option under "General Library Settings->Posix Thread support"
 of "make uclibc-menuconfig".

once toolchain is built compile gdb and gdbserver as mentioned above.



