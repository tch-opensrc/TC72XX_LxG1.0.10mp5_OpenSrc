#ifndef SQUASHFS_SWAP_H
#define SQUASHFS_SWAP_H
/*
 * Squashfs
 *
 * Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009
 * Phillip Lougher <phillip@lougher.demon.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * squashfs_swap.h
 */

/*
 * macros to convert each stucture from big endian to little endian
 */

extern void swap_le16(unsigned short *, unsigned short *);
extern void swap_le32(unsigned int *, unsigned int *);
extern void swap_le64(long long *, long long *);

#define SWAP_LE16(d, s, field)	swap_le16(&((s)->field), &((d)->field))
#define SWAP_LE32(d, s, field)	swap_le32(&((s)->field), &((d)->field))
#define SWAP_LE64(d, s, field)	swap_le64(&((s)->field), &((d)->field))

#define _SQUASHFS_SWAP_SUPER_BLOCK(s, d, SWAP_FUNC) {\
	SWAP_FUNC##32(s, d, s_magic);\
	SWAP_FUNC##32(s, d, inodes);\
	SWAP_FUNC##32(s, d, mkfs_time);\
	SWAP_FUNC##32(s, d, block_size);\
	SWAP_FUNC##32(s, d, fragments);\
	SWAP_FUNC##16(s, d, compression);\
	SWAP_FUNC##16(s, d, block_log);\
	SWAP_FUNC##16(s, d, flags);\
	SWAP_FUNC##16(s, d, no_ids);\
	SWAP_FUNC##16(s, d, s_major);\
	SWAP_FUNC##16(s, d, s_minor);\
	SWAP_FUNC##64(s, d, root_inode);\
	SWAP_FUNC##64(s, d, bytes_used);\
	SWAP_FUNC##64(s, d, id_table_start);\
	SWAP_FUNC##64(s, d, xattr_table_start);\
	SWAP_FUNC##64(s, d, inode_table_start);\
	SWAP_FUNC##64(s, d, directory_table_start);\
	SWAP_FUNC##64(s, d, fragment_table_start);\
	SWAP_FUNC##64(s, d, lookup_table_start);\
}

#define SQUASHFS_SWAP_SUPER_BLOCK(s, d)	\
			_SQUASHFS_SWAP_SUPER_BLOCK(s, d, SWAP_LE)

#endif

