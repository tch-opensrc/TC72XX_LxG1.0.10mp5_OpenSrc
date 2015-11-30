/***********************************************************************
 *
 *  Copyright (c) 2006  Broadcom Corporation
 *  All Rights Reserved
 *
# 
# 
# Unless you and Broadcom execute a separate written software license 
# agreement governing use of this software, this software is licensed 
# to you under the terms of the GNU General Public License version 2 
# (the "GPL"), available at http://www.broadcom.com/licenses/GPLv2.php, 
# with the following added to such license:
# 
#    As a special exception, the copyright holders of this software give 
#    you permission to link this software with independent modules, and 
#    to copy and distribute the resulting executable under terms of your 
#    choice, provided that you also meet, for each linked independent 
#    module, the terms and conditions of the license of that module. 
#    An independent module is a module which is not derived from this
#    software.  The special exception does not apply to any modifications 
#    of the software.  
# 
# Not withstanding the above, under no circumstances may you combine 
# this software in any way with any other Broadcom software provided 
# under a license other than the GPL, without Broadcom's express prior 
# written consent. 
#
 * 
 ************************************************************************/

#include <string.h>
#include "cms.h"
#include "cms_mem.h"
#include "cms_dlist.h"
#include "cms_ast.h"
#include "oal.h"
#include "bget.h"

// this is in standard Intel libc, we implement it locally for MIPS
extern int backtrace(void **buffer, int size);


/** Macro to round up to nearest 4 byte length */
#define ROUNDUP4(s)  (((s) + 3) & 0xfffffffc)


/** Macro to calculate how much we need to allocate for a given user size request.
 *
 * We need the header even when not doing MEM_DEBUG to keep
 * track of the size and allocFlags that was passed in during cmsMem_alloc.
 * This info is needed during cmsMem_realloc.
 */
#ifdef CMS_MEM_DEBUG
#define REAL_ALLOC_SIZE(s) (CMS_MEM_HEADER_LENGTH + ROUNDUP4(s) + CMS_MEM_FOOTER_LENGTH)
#else
#define REAL_ALLOC_SIZE(s) (CMS_MEM_HEADER_LENGTH + (s))
#endif


static CmsMemStats mStats;


#ifdef CMS_MEM_LEAK_TRACING

#define NUM_STACK_ENTRIES   15

typedef struct alloc_record {
   DlistNode dlist;
   unsigned char *bufAddr;
   UINT32 userSize;
   UBOOL8 isClone;
   UBOOL8 dumpThisClone;
   UINT32 seq;
   void *stackAddr[NUM_STACK_ENTRIES];
} AllocRecord;

static UINT32 allocSeq=0;
DLIST_HEAD(glbAllocRec);

static void initAllocSeq(void)
{
   CmsTimestamp tms;

   if (allocSeq > 0)
   {
      return;
   }

   cmsTms_get(&tms);

   srand(tms.nsec);
   allocSeq = rand() << 16;
   cmsLog_debug("allocSeq=%lu\n", allocSeq);

   return;
}

static void garbageCollectAllocRec(void);
#endif /* CMS_MEM_LEAK_TRACING */


#ifdef MDM_SHARED_MEM

/** Macro to determine if a pointer is in shared memory or not. */
#define IS_IN_SHARED_MEM(p) ((((UINT32) (p)) >= mStats.shmAllocStart) && \
                             (((UINT32) (p)) < mStats.shmAllocEnd))


void cmsMem_initSharedMem(void *addr, UINT32 len)
{
   mStats.shmAllocStart = (UINT32) addr;
   mStats.shmAllocEnd = mStats.shmAllocStart + len;
   mStats.shmTotalBytes = len;

   cmsLog_notice("shm pool: %p-%p", mStats.shmAllocStart, mStats.shmAllocEnd);

   bpool(addr, len);
}

void cmsMem_initSharedMemPointer(void *addr, UINT32 len)
{
   /*
    * You might be tempted to do a memset(&mStats, 0, sizeof(mStats)) here.
    * But don't do it.  mStats will be initialized to all zeros anyways since
    * it is in the bss.  And smd will start using the memory allocator before
    * it calls cmsMem_initSharedMemPointer, so if we zero out the structure
    * at the beginning of this function, the counters will be wrong.
    */
   mStats.shmAllocStart = (UINT32) addr;
   mStats.shmAllocEnd = mStats.shmAllocStart + len;
   mStats.shmTotalBytes = len;

   cmsLog_notice("shm pool: %p-%p", mStats.shmAllocStart, mStats.shmAllocEnd);

   bcm_secondary_bpool(addr);
}

#endif

void cmsMem_cleanup(void)
{

#ifdef MDM_SHARED_MEM
   bcm_cleanup_bpool();
#endif

   return;
}


void *cmsMem_alloc(UINT32 size, UINT32 allocFlags)
{
   void *buf;
   UINT32 allocSize;

#ifdef CMS_MEM_LEAK_TRACING
   initAllocSeq();
#endif

   allocSize = REAL_ALLOC_SIZE(size);

#ifdef MDM_SHARED_MEM
   if (allocFlags & ALLOC_SHARED_MEM)
   {
#ifdef CMS_MEM_LEAK_TRACING
      buf = bget(allocSize, allocSeq);
#else
      buf = bget(allocSize);
#endif
   }
   else
#endif
   {
      buf = oal_malloc(allocSize);
      if (buf)
      {
         mStats.bytesAllocd += size;
         mStats.numAllocs++;
      }
   }


   if (buf != NULL)
   {
      UINT32 *intBuf = (UINT32 *) buf;
      UINT32 intSize = allocSize / sizeof(UINT32);


      if (allocFlags & ALLOC_ZEROIZE)
      {
         memset(buf, 0, allocSize);
      }
#ifdef CMS_MEM_POISON_ALLOC_FREE
      else
      {
         /*
          * Set alloc'ed buffer to garbage to catch use-before-init.
          * But we also allocate huge buffers for storing image downloads.
          * Don't bother writing garbage to those huge buffers.
          */
         if (allocSize < 64 * 1024)
         {
            memset(buf, CMS_MEM_ALLOC_PATTERN, allocSize);
         }
      }
#endif
         
      /*
       * Record the allocFlags in the first word, and the 
       * size of user buffer in the next 2 words of the buffer.
       * Make 2 copies of the size in case one of the copies gets corrupted by
       * an underflow.  Make one copy the XOR of the other so that there are
       * not so many 0's in size fields.
       */
      intBuf[0] = allocFlags;
      intBuf[1] = size;
      intBuf[2] = intBuf[1] ^ 0xffffffff;

      buf = &(intBuf[3]); /* this gets returned to user */

#ifdef CMS_MEM_DEBUG
      {
         UINT8 *charBuf = (UINT8 *) buf;
         UINT32 i, roundup4Size = ROUNDUP4(size);

         for (i=size; i < roundup4Size; i++)
         {
            charBuf[i] = CMS_MEM_FOOTER_PATTERN & 0xff;
         }

         intBuf[intSize - 1] = CMS_MEM_FOOTER_PATTERN;
         intBuf[intSize - 2] = CMS_MEM_FOOTER_PATTERN;
      }
#endif

#ifdef CMS_MEM_LEAK_TRACING
      {
         AllocRecord *allocRec;
         if (!(allocRec = calloc(1, sizeof(AllocRecord))))
         {
            cmsLog_error("could not malloc a record to track alloc");
         }
         else
         {
            allocRec->bufAddr = buf;
            allocRec->userSize = size;
            allocRec->seq = allocSeq++;
            backtrace(allocRec->stackAddr, NUM_STACK_ENTRIES);
            /*
             * new allocs are placed at the beginning of the list, right after
             * the head.
             */
            dlist_append((struct dlist_node *)allocRec, &glbAllocRec);
         }

         /*
          * do periodic garbage collection on the allocRecs which point
          * to shmBuf's that has been freed by another app.
          */
         if ((allocSeq % 2000) == 0)
         {
            cmsLog_debug("Starting allocRec garbage collection");
            garbageCollectAllocRec();
            cmsLog_debug("garbage collection done");
         }
      }
#endif

   }

   return buf;
}


void *cmsMem_realloc(void *origBuf, UINT32 size)
{
   void *buf;
   UINT32 origSize, origAllocSize, origAllocFlags;
   UINT32 allocSize;
   UINT32 *intBuf;

   if (origBuf == NULL)
   {
      cmsLog_error("cannot take a NULL buffer");
      return NULL;
   }

   if (size == 0)
   {
      cmsMem_free(origBuf);
      return NULL;
   }

   allocSize = REAL_ALLOC_SIZE(size);


   intBuf = (UINT32 *) (((UINT32) origBuf) - CMS_MEM_HEADER_LENGTH);

   origAllocFlags = intBuf[0];
   origSize = intBuf[1];

   /* sanity check the original length */
   if (intBuf[1] != (intBuf[2] ^ 0xffffffff))
   {
      cmsLog_error("memory underflow detected, %d %d", intBuf[1], intBuf[2]);
      cmsAst_assert(0);
      return NULL;
   }

   origAllocSize = REAL_ALLOC_SIZE(origSize);

   if (allocSize <= origAllocSize)
   {
      /* currently, I don't shrink buffers, but could in the future. */
      return origBuf;
   }

   buf = cmsMem_alloc(allocSize, origAllocFlags);
   if (buf != NULL)
   {
      /* got new buffer, copy orig buffer to new buffer */
      memcpy(buf, origBuf, origSize);
      cmsMem_free(origBuf);
   }
   else
   {
      /*
       * We could not allocate a bigger buffer.
       * Return NULL but leave the original buffer untouched.
       */
   }

   return buf;
}



/** Free previously allocated memory
 * @param buf Previously allocated buffer.
 */
void cmsMem_free(void *buf)
{
   UINT32 size;

   if (buf != NULL)
   {
      UINT32 *intBuf = (UINT32 *) (((UINT32) buf) - CMS_MEM_HEADER_LENGTH);

#ifdef CMS_MEM_LEAK_TRACING
      {
         AllocRecord *allocRec;
         dlist_for_each_entry(allocRec, &glbAllocRec, dlist)
            if (allocRec->bufAddr == buf)
               break;

         if ((DlistNode *) allocRec != &glbAllocRec)
         {
            dlist_unlink((struct dlist_node *) allocRec);
            free(allocRec);
         }
         else
         {
            /*
             * Buffers allocated from shared mem could have been freed by 
             * another app, so if we have an alloc record but cannot find
             * it in shared mem, ignore it.  But if the alloc record is in
             * private heap, that is an error.
             */
            if (!IS_IN_SHARED_MEM(buf))
            {
               cmsLog_error("possible double free, could not find allocRec for buf %p", buf);
            }
         }
      }
#endif

      size = intBuf[1];

      if (intBuf[1] != (intBuf[2] ^ 0xffffffff))
      {
         cmsLog_error("memory underflow detected, %d %d", intBuf[1], intBuf[2]);
         cmsAst_assert(0);
         return;
      }

#ifdef CMS_MEM_DEBUG
      {
         UINT32 allocSize, intSize, roundup4Size, i;
         UINT8 *charBuf = (UINT8 *) buf;

         allocSize = REAL_ALLOC_SIZE(intBuf[1]);
         intSize = allocSize / sizeof(UINT32);
         roundup4Size = ROUNDUP4(intBuf[1]);

         for (i=intBuf[1]; i < roundup4Size; i++)
         {
            if (charBuf[i] != (UINT8) (CMS_MEM_FOOTER_PATTERN & 0xff))
            {
               cmsLog_error("memory overflow detected at idx=%d 0x%x 0x%x 0x%x",
                            i, charBuf[i], intBuf[intSize-1], intBuf[intSize-2]);
               cmsAst_assert(0);
               return;
            }
         }
               
         if ((intBuf[intSize - 1] != CMS_MEM_FOOTER_PATTERN) ||
             (intBuf[intSize - 2] != CMS_MEM_FOOTER_PATTERN))
         {
            cmsLog_error("memory overflow detected, 0x%x 0x%x",
                         intBuf[intSize - 1], intBuf[intSize - 2]);
            cmsAst_assert(0);
            return;
         }

#ifdef CMS_MEM_POISON_ALLOC_FREE
         /*
          * write garbage into buffer which is about to be freed to detect
          * users of freed buffers.
          */
         memset(intBuf, CMS_MEM_FREE_PATTERN, allocSize);
#endif
      }

#endif  /* CMS_MEM_DEBUG */


      buf = intBuf;  /* buf points to real start of buffer */


#ifdef MDM_SHARED_MEM
      if (IS_IN_SHARED_MEM(buf))
      {
         brel(buf);
      }
      else
#endif
      {
         oal_free(buf);
         mStats.bytesAllocd -= size;
         mStats.numFrees++;
      }
   }
}


char *cmsMem_strdup(const char *str)
{
   return cmsMem_strdupFlags(str, 0);
}


char *cmsMem_strdupFlags(const char *str, UINT32 flags)
{
   UINT32 len;
   void *buf;

   if (str == NULL)
   {
      return NULL;
   }

   /* this is somewhat dangerous because it depends on str being NULL
    * terminated.  Use strndup/strlen if not sure the length of the string.
    */
   len = strlen(str);

   buf = cmsMem_alloc(len+1, flags);
   if (buf == NULL)
   {
      return NULL;
   }

   strncpy((char *) buf, str, len+1);

   return ((char *) buf);
}



char *cmsMem_strndup(const char *str, UINT32 maxlen)
{
   return cmsMem_strndupFlags(str, maxlen, 0);
}


char *cmsMem_strndupFlags(const char *str, UINT32 maxlen, UINT32 flags)
{
   UINT32 len;
   char *buf;

   if (str == NULL)
   {
      return NULL;
   }

   len = cmsMem_strnlen(str, maxlen, NULL);

   buf = (char *) cmsMem_alloc(len+1, flags);
   if (buf == NULL)
   {
      return NULL;
   }

   strncpy(buf, str, len);
   buf[len] = 0;

   return buf;
}


UINT32 cmsMem_strnlen(const char *str, UINT32 maxlen, UBOOL8 *isTerminated)
{
   UINT32 len=0;

   while ((len < maxlen) && (str[len] != 0))
   {
      len++;
   }

   if (isTerminated != NULL)
   {
      *isTerminated = (str[len] == 0);
   }

   return len;
}


void cmsMem_getStats(CmsMemStats *stats)
{
   bufsize curalloc, totfree, maxfree;
   long nget, nrel;


   stats->shmAllocStart = mStats.shmAllocStart;
   stats->shmAllocEnd = mStats.shmAllocEnd;
   stats->shmTotalBytes = mStats.shmTotalBytes;

   /*
    * Get shared memory stats from bget.  The shared memory stats reflects
    * activity of all processes who are accessing the shared memory region,
    * not just this process.
    */
   bstats(&curalloc, &totfree, &maxfree, &nget, &nrel);

   stats->shmBytesAllocd = (UINT32) curalloc;
   stats->shmNumAllocs = (UINT32) nget;
   stats->shmNumFrees = (UINT32) nrel;


   /* the private heap memory stats can come directly from our data structure */
   stats->bytesAllocd = mStats.bytesAllocd;
   stats->numAllocs = mStats.numAllocs;
   stats->numFrees = mStats.numFrees;

   return;
}


#ifdef SUPPORT_DEBUG_TOOLS

#define KB_IN_B  1024

void cmsMem_dumpMemStats()
{
   CmsMemStats memStats;

   cmsMem_getStats(&memStats);

   printf("Total MDM Shared Memory Region : %dKB\n", (memStats.shmAllocEnd - MDM_SHM_ATTACH_ADDR)/KB_IN_B);
   printf("Shared Memory Usable           : %06dKB\n", memStats.shmTotalBytes/KB_IN_B);
   printf("Shared Memory in-use           : %06dKB\n", memStats.shmBytesAllocd/KB_IN_B);
   printf("Shared Memory free             : %06dKB\n", (memStats.shmTotalBytes - memStats.shmBytesAllocd)/KB_IN_B);
   printf("Shared Memory allocs           : %06d\n", memStats.shmNumAllocs);
   printf("Shared Memory frees            : %06d\n", memStats.shmNumFrees);
   printf("Shared Memory alloc/free delta : %06d\n", memStats.shmNumAllocs - memStats.shmNumFrees);
   printf("\n");

   printf("Heap bytes in-use     : %06d\n", memStats.bytesAllocd);
   printf("Heap allocs           : %06d\n", memStats.numAllocs);
   printf("Heap frees            : %06d\n", memStats.numFrees);
   printf("Heap alloc/free delta : %06d\n", memStats.numAllocs - memStats.numFrees);

   return;
}


#endif /* SUPPORT_DEBUG_TOOLS */


#ifdef CMS_MEM_LEAK_TRACING

static const int FSHIFT = 16;              /* nr of bits of precision */
#define FIXED_1         (1<<FSHIFT)     /* 1.0 as fixed-point */
#define LOAD_INT(x) ((x) >> FSHIFT)
#define LOAD_FRAC(x) LOAD_INT(((x) & (FIXED_1-1)) * 100)

#define abs(s) ((s) < 0? -(s) : (s))

#ifndef DESKTOP_LINUX
int backtrace(void **buffer, int size)
{
	unsigned long *addr;
	unsigned long *ra;
	unsigned long *sp;
	size_t ra_offset;
	size_t stack_size;
	int depth;

	if (!size)
		return 0;
	if (!buffer || size < 0)
		return -1;

	/* Get current $ra and $sp */
	__asm__ __volatile__ (
	"	move %0, $ra\n"
	"	move %1, $sp\n"
	: "=r"(ra), "=r"(sp)
	);

	/* Scanning to find the size of the current stack-frame */
	stack_size = 0;
	for (addr = (unsigned long *)backtrace; !stack_size; ++addr) {
		if ((*addr & 0xffff0000) == 0x27bd0000) /* addiu sp, sp, sz */
			stack_size = abs((short)(*addr & 0xffff));
		else if (*addr == 0x03e00008) /* jr ra */
			break;
	}
	sp = (unsigned long *)((unsigned long)sp + stack_size);

	/* Repeat backward scanning */
	for (depth = 0; depth < size && ra; ++depth) {
		buffer[depth] = ra;
		ra_offset = 0;
		stack_size = 0;

		for (addr = ra; !ra_offset || !stack_size; --addr) {
			switch (*addr & 0xffff0000) {
			case 0x27bd0000: /* addiu sp, sp, -stack_size */
				stack_size = abs((short)(*addr & 0xffff));
				break;
			case 0xafbf0000: /* sw ra, offset */
				ra_offset = (short)(*addr & 0xffff);
				break;
			case 0x3c1c0000: /* lui gp, constant */
				return depth + 1;
			default:
				break;
			}
		}
		ra = *(unsigned long **)((unsigned long)sp + ra_offset);
		sp = (unsigned long *)((unsigned long)sp + stack_size);
	}
	return depth;
}
#endif

static void print_backtrace(void **array, int size)
{
	FILE *fp;
	int i;

	fp = fopen("/proc/self/maps","r");

	for (i = 0; i < size && 0 < (UINT32) array[i]; i++) {
		char line[1024];
		int found = 0;

		rewind(fp);
		while (fgets(line, sizeof(line), fp)) {
			char lib[1024];
			void *start, *end;
			unsigned int offset;
			int n = sscanf(line, "%p-%p %*s %x %*s %*d %s",
				       &start, &end, &offset, lib);
			if (n == 4 && array[i] >= start && array[i] < end) {
#ifdef DESKTOP_LINUX
            offset = array[i] - start;
#else
				if (array[i] < (void*)0x10000000)
					offset = (unsigned int)array[i];
				else
					offset += array[i] - start;
#endif

				printf("#%d  [%08x] in %s\n", i, offset, lib);
				found = 1;
				break;
			}
		}
		if (!found)
			printf("#%d  [%08x]\n", i, (unsigned)array[i]);
	}
	fclose(fp);
}


static void dumpAllocRec(AllocRecord *allocRec, UBOOL8 dumpStackTrace)
{

   printf("allocRec size=%d buf=%p seq=%u\n", allocRec->userSize, allocRec->bufAddr, allocRec->seq);
   if (dumpStackTrace)
   {
      print_backtrace(allocRec->stackAddr, NUM_STACK_ENTRIES);
   }

#ifdef dump_raw_addrs
   {
      UINT32 i;
      for (i=0; i < NUM_STACK_ENTRIES && allocRec->stackAddr[i] != 0; i++)
          printf("   [%02d] %p\n", i, allocRec->stackAddr[i]);
   }
#endif

   return;
}


static UBOOL8 compareStacks(void **stack1, void **stack2)
{
   UBOOL8 same=TRUE;
   UINT32 i;

   for (i=0; i < NUM_STACK_ENTRIES && same; i++)
   {
      if (stack1[i] != stack2[i])
      {
         same = FALSE;
      }
   }

   return same;
}


void dumpNumTraces(UINT32 max)
{
   AllocRecord *allocRec;
   UINT32 count=0;

   dlist_for_each_entry(allocRec, &glbAllocRec, dlist)
   {
      count++;
      if (max > 0 && count > max)
      {
         break;
      }

      if (IS_IN_SHARED_MEM(allocRec->bufAddr))
      {
         if (isShmAddrInUse(allocRec->bufAddr-CMS_MEM_HEADER_LENGTH, allocRec->seq))
         {
            dumpAllocRec(allocRec, TRUE);
         }
         else
         {
            /*
             * The shm buffer allocated by this app in not in use, which means
             * it was freed by another app.  Just ignore it.  Let the next run of
             * garbage collection delete this allocation record.
             */
         }
      }
      else
      {
         dumpAllocRec(allocRec, TRUE);
      }
   }
}

void cmsMem_dumpTraceAll()
{
   dumpNumTraces(0);
}

void cmsMem_dumpTrace50()
{
   dumpNumTraces(50);
}

#define CLONE_COUNT_THRESH  5

void cmsMem_dumpTraceClones()
{
   AllocRecord *allocRec;
   AllocRecord *allocRec2;
   UINT32 cloneCount;
   UBOOL8 anyClonesFound=FALSE;

   /* first clear all the isClone flags in the allocRecs */
   dlist_for_each_entry(allocRec, &glbAllocRec, dlist)
   {
      allocRec->isClone = FALSE;
      allocRec->dumpThisClone = FALSE;
   }

   dlist_for_each_entry(allocRec, &glbAllocRec, dlist)
   {
      cloneCount = 1;  /* the current one counts as a clone too */

      /* clear all previous dumpThisClone flags from previous clone search */
      allocRec2 = (AllocRecord *) allocRec->dlist.next;
      while (((DlistNode *) allocRec2) != &glbAllocRec)
      {
         allocRec2->dumpThisClone = FALSE;
         allocRec2 = (AllocRecord *) allocRec2->dlist.next;
      }

      /* search for clones of allocRec */
      allocRec2 = (AllocRecord *) allocRec->dlist.next;
      while (((DlistNode *) allocRec2) != &glbAllocRec)
      {
         if ((!allocRec2->isClone) &&
             (allocRec->userSize == allocRec2->userSize) &&
             (compareStacks(allocRec->stackAddr, allocRec2->stackAddr)) &&
             ((IS_IN_SHARED_MEM(allocRec->bufAddr) && IS_IN_SHARED_MEM(allocRec2->bufAddr)) ||
              (!IS_IN_SHARED_MEM(allocRec->bufAddr) && !IS_IN_SHARED_MEM(allocRec2->bufAddr))))
         {
            /* found a clone */
            allocRec2->isClone = TRUE;
            allocRec2->dumpThisClone = TRUE;
            cloneCount++;
         }

         allocRec2 = (AllocRecord *) allocRec2->dlist.next;
      }

      if (cloneCount >= CLONE_COUNT_THRESH)
      {
         printf("==== Found %d clones ====\n", cloneCount);

         anyClonesFound = TRUE;

         dumpAllocRec(allocRec, TRUE);

         allocRec2 = (AllocRecord *) allocRec->dlist.next;
         while (((DlistNode *) allocRec2) != &glbAllocRec)
         {
            if (allocRec2->dumpThisClone)
            {
               dumpAllocRec(allocRec2, FALSE);
            }

            allocRec2 = (AllocRecord *) allocRec2->dlist.next;
         }

         printf("\n\n");
      }
   }

   if (!anyClonesFound)
   {
      printf("==== No cloned records found ====\n");
   }

   return;
}


static void garbageCollectAllocRec()
{
   UBOOL8 found=TRUE;
   AllocRecord *allocRec;

   while (found)
   {
      found = FALSE;

      dlist_for_each_entry(allocRec, &glbAllocRec, dlist)
      {
         if (IS_IN_SHARED_MEM(allocRec->bufAddr) &&
             !(isShmAddrInUse(allocRec->bufAddr-CMS_MEM_HEADER_LENGTH, allocRec->seq)))
         {
            /*
             * Found an allocation record for a shmBuf that is not in use.
             * Must have been freed by another app.  Delete our AllocRec.
             */
            cmsLog_debug("collected %p", allocRec->bufAddr);
            found = TRUE;
            break;
         }
      }

      if (found)
      {
         dlist_unlink((struct dlist_node *) allocRec);
         free(allocRec);
      }
   }
}


#endif  /* CMS_MEM_LEAK_TRACING */

