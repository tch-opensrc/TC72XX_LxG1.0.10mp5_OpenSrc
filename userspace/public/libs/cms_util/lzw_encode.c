/*
 * LZW encoder
 * Copyright (c) 2007 Bartlomiej Wolowiec
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifdef COMPRESSED_CONFIG_FILE

#include "cms.h"
#include "cms_util.h"


#ifdef DESKTOP_LINUX

/*
 * We are on Intel.  Intel is LE.
 */
#define be2me_16(x) ntohs(x)
#define be2me_32(x) ntohl(x)

#else

/*
 * We are on the MIPS.  MIPS is in BE mode.
 */
#define be2me_16(x) (x)
#define be2me_32(x) (x)

#define UNALIGNED_STORES_ARE_BAD
#endif


#define FFMAX(a,b) ((a) > (b) ? (a) : (b))
#define FFMIN(a,b) ((a) > (b) ? (b) : (a))


/**
 * Hash function adding character
 * @param head LZW code for prefix
 * @param add Character to add
 * @return New hash value
 */
static inline int hash(int head, const int add)
{
    head ^= (add << LZW_HASH_SHIFT);
    if (head >= LZW_HASH_SIZE)
        head -= LZW_HASH_SIZE;
    cmsAst_assert(head >= 0 && head < LZW_HASH_SIZE);
    return head;
}

/**
 * Hash function calculates next hash value
 * @param head Actual hash code
 * @param offset Offset calculated by hashOffset
 * @return New hash value
 */
static inline int hashNext(int head, const int offset)
{
    head -= offset;
    if(head < 0)
        head += LZW_HASH_SIZE;
    return head;
}

/**
 * Hash function calculates hash offset
 * @param head Actual hash code
 * @return Hash offset
 */
static inline int hashOffset(const int head)
{
    return head ? LZW_HASH_SIZE - head : 1;
}


static void init_put_bits(PutBitContext *s, uint8_t *buffer, int buffer_size)
{
    if(buffer_size < 0) {
        buffer_size = 0;
        buffer = NULL;
    }

    s->buf = buffer;
    s->buf_end = s->buf + buffer_size;

    s->buf_ptr = s->buf;
    s->bit_left=32;
    s->bit_buf=0;
}


void put_bits(PutBitContext *s, int n, unsigned int value)
{
    unsigned int bit_buf;
    int bit_left;

    //    printf("put_bits=%d %c (0x%x)\n", n, value, value);
    cmsAst_assert(n == 32 || value < (1U << n));

    bit_buf = s->bit_buf;
    bit_left = s->bit_left;

    //    printf("n=%d value=%x cnt=%d buf=%x\n", n, value, bit_cnt, bit_buf);
    /* XXX: optimize */
    if (n < bit_left) {
        bit_buf = (bit_buf<<n) | value;
        bit_left-=n;
    } else {
        bit_buf<<=bit_left;
        bit_buf |= value >> (n - bit_left);
#ifdef UNALIGNED_STORES_ARE_BAD
        if (3 & (intptr_t) s->buf_ptr) {
            s->buf_ptr[0] = bit_buf >> 24;
            s->buf_ptr[1] = bit_buf >> 16;
            s->buf_ptr[2] = bit_buf >>  8;
            s->buf_ptr[3] = bit_buf      ;
        } else
#endif
        *(uint32_t *)s->buf_ptr = be2me_32(bit_buf);
        //        printf("STORING: buf=%p be bitbuf = %08x me=%08x (bit_left=%d)\n",
        //               s->buf_ptr, bit_buf, *(uint32_t *)s->buf_ptr, bit_left);
        s->buf_ptr+=4;
        bit_left+=32 - n;
        bit_buf = value;
    }

    s->bit_buf = bit_buf;
    s->bit_left = bit_left;
}

/* pad the end of the output stream with zeros */
static inline void flush_put_bits(PutBitContext *s)
{
    s->bit_buf<<= s->bit_left;
    while (s->bit_left < 32) {
        /* XXX: should test end of buffer */
        *s->buf_ptr++=s->bit_buf >> 24;
        s->bit_buf<<=8;
        s->bit_left+=8;
    }
    s->bit_left=32;
    s->bit_buf=0;
}


/* return the number of bits output */
static inline int put_bits_count(PutBitContext *s)
{
    return (s->buf_ptr - s->buf) * 8 + 32 - s->bit_left;
}



/**
 * Write one code to stream
 * @param s LZW state
 * @param c code to write
 */
static inline void writeCode(LZWEncoderState * s, int c)
{
    cmsAst_assert(0 <= c && c < 1 << s->bits);
    put_bits(&s->pb, s->bits, c);
}


/**
 * Find LZW code for block
 * @param s LZW state
 * @param c Last character in block
 * @param hash_prefix LZW code for prefix
 * @return LZW code for block or -1 if not found in table
 */
static inline int findCode(LZWEncoderState * s, uint8_t c, int hash_prefix)
{
    int h = hash(FFMAX(hash_prefix, 0), c);
    int hash_offset = hashOffset(h);

    while (s->tab[h].hash_prefix != LZW_PREFIX_FREE) {
        if ((s->tab[h].suffix == c)
            && (s->tab[h].hash_prefix == hash_prefix))
            return h;
        h = hashNext(h, hash_offset);
    }

    return h;
}

/**
 * Add block to LZW code table
 * @param s LZW state
 * @param c Last character in block
 * @param hash_prefix LZW code for prefix
 * @param hash_code LZW code for bytes block
 */
static inline void addCode(LZWEncoderState * s, uint8_t c, int hash_prefix, int hash_code)
{
    s->tab[hash_code].code = s->tabsize;
    s->tab[hash_code].suffix = c;
    s->tab[hash_code].hash_prefix = hash_prefix;

    s->tabsize++;

    if (s->tabsize >= 1 << s->bits)
        s->bits++;
}

/**
 * Clear LZW code table
 * @param s LZW state
 */
static void clearTable(LZWEncoderState * s)
{
    int i, h;
    //    printf("clearTable called\n");
    writeCode(s, s->clear_code);
    s->bits = 9;
    for (i = 0; i < LZW_HASH_SIZE; i++) {
        s->tab[i].hash_prefix = LZW_PREFIX_FREE;
    }
    for (i = 0; i < 256; i++) {
        h = hash(0, i);
        s->tab[h].code = i;
        s->tab[h].suffix = i;
        s->tab[h].hash_prefix = LZW_PREFIX_EMPTY;
    }
    s->tabsize = 258;
}

/**
 * Calculate number of bytes written
 * @param s LZW encode state
 * @return Number of bytes written
 */
static SINT32 writtenBytes(LZWEncoderState *s){
    int ret = put_bits_count(&s->pb) >> 3;
    ret -= s->output_bytes;
    s->output_bytes += ret;
    return ret;
}


CmsRet cmsLzw_initEncoder(LZWEncoderState **p, UINT8 *outbuf, UINT32 outsize)
{
   LZWEncoderState *s;

    *p = (LZWEncoderState *) cmsMem_alloc(sizeof(LZWEncoderState), ALLOC_ZEROIZE);
    if (*p == NULL)
    {
       cmsLog_error("could not allocate %d bytes for encoder state", sizeof(LZWEncoderState));
       return CMSRET_RESOURCE_EXCEEDED;
    }
    else
    {
       cmsLog_debug("%d bytes allocated for encoder state", sizeof(LZWEncoderState));
    }

    s = *p;

    s->clear_code = 256;
    s->end_code = 257;
    s->maxbits = LZW_MAXBITS;
    init_put_bits(&s->pb, outbuf, outsize);
    s->bufsize = outsize;
    cmsAst_assert(9 <= s->maxbits && s->maxbits <= s->maxbits);
    s->maxcode = 1 << s->maxbits;
    s->output_bytes = 0;
    s->last_code = LZW_PREFIX_EMPTY;
    s->bits = 9;

    return CMSRET_SUCCESS;
}


int cmsLzw_encode(LZWEncoderState *s, const UINT8 *inbuf, UINT32 insize)
{
    UINT32 i;
    UINT32 round_error=5;

    //    printf("size check, insize*3=%d outbuf*2=%d\n", insize * 3, s->bufsize * 2);
    if(insize * 3 > (UINT32) (s->bufsize - s->output_bytes) * 2 + round_error){
        cmsLog_error("size check failed, insize=%d (*3) outsize=%d (*2)", insize, (s->bufsize - s->output_bytes));
        return -1;
    }

    if (s->last_code == LZW_PREFIX_EMPTY)
        clearTable(s);

    for (i = 0; i < insize; i++) {
        uint8_t c = *inbuf++;
        int code = findCode(s, c, s->last_code);

        //        printf("\n\n[%d] c=%c code=%d\n", i, c, code);        
        if (s->tab[code].hash_prefix == LZW_PREFIX_FREE) {
            writeCode(s, s->last_code);
            addCode(s, c, s->last_code, code);
            code= hash(0, c);
        }
        s->last_code = s->tab[code].code;
        if (s->tabsize >= s->maxcode - 1) {
            clearTable(s);
        }
    }

    return writtenBytes(s);
}


SINT32 cmsLzw_flushEncoder(LZWEncoderState * s)
{
    if (s->last_code != -1)
        writeCode(s, s->last_code);

    writeCode(s, s->end_code);
    flush_put_bits(&s->pb);
    s->last_code = -1;

    return writtenBytes(s);
}


void cmsLzw_cleanupEncoder(LZWEncoderState **s)
{
   cmsMem_free(*s);
   *s = NULL;
}


#endif /* COMPRESSED_CONFIG_FILE */

