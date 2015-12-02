

CFLAGS := -DWITHOUT_XATTR -I. -I$(KERNEL_DIR)/include

mkfs.jffs2: crc32.o compr_rtime.o mkfs.jffs2.o compr_zlib.o compr.o rbtree.o
	$(CC) $(LDFLAGS) -o $@ $^ -lz

clean:
	${RM} *~ *.o

