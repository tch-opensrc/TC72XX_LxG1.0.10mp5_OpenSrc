//*************************************************************************************
// Broadcom Corp. Confidential
// Copyright 2000, 2001, 2002, 2003 Broadcom Corp. All Rights Reserved.
//
// THIS SOFTWARE MAY ONLY BE USED SUBJECT TO AN EXECUTED
// SOFTWARE LICENSE AGREEMENT BETWEEN THE USER AND BROADCOM.
// YOU HAVE NO RIGHT TO USE OR EXPLOIT THIS MATERIAL EXCEPT
// SUBJECT TO THE TERMS OF SUCH AN AGREEMENT.
//
//**************************************************************************************
// File Name  : bcmImageBuilder.c
//
// Description: Build a image with a header (tag), a compressed kernel and
//              a compress JFFS2 root file system
//
// Created    : 04/23/2002  songw
//
// Modified   : 05/18/2002  seanl
//              Redefined the tag info.  Add imageType and getting all the flash address
//              from "board.h" which is shared by CFE and web uploading code.
//
//            : 04/18/2003 seanl
//              add the support for cramfs and new flash image layout:
//              1). minicfe : 64k
//              2). TAG:      256b
//              3). cramfs:   
//              4). kernel:
//              5). psi:      16k (default)
//              
//
//**************************************************************************************
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <netinet/in.h>

#define  BCMTAG_EXE_USE
#include "bcmTag.h"

/* The BCM963xx flash memory map contains five sections: boot loader,
 * file system, kernel, nvram storage, persistent storage.  This program
 * defines the starting address for the boot loader, file system and kernel.
 * The run-time flash driver determines the starting address of the nvram
 * and persistent storage sections based on the flash part size.
 */
#include "board.h"

/* Typedefs.h */
typedef unsigned char byte;
typedef unsigned int UINT32;

#define BUF_SIZE        100 * 1024
static byte buffer[BUF_SIZE];

/*
     struct option {
             char *name;
             int has_arg;
             int *flag;
             int val;
     };

     The name field should contain the option name without the leading double
     dash.
     The has_arg field should be one of:
     no_argument        no argument to the option is expect.
     required_argument  an argument to the option is required.
     optional_argument  an argument to the option may be presented.
     If flag is non-NULL, then the integer pointed to by it will be set to the
     value in the val field. If the flag field is NULL, then the val field
     will be returned. Setting flag to NULL and setting val to the correspond-
     ing short option will make this function act just like getopt(3).
*/
static struct option longopts[] = {
    { "help", 0, 0, 'h' },
    { "version", 0, 0, 'v' },
    { "littleendian", 0, 0, 'l'},
    { "chip", 1, 0, 0},
    { "board", 1, 0, 0},
    { "blocksize", 1, 0, 0},
    { "output", 1, 0, 0},
    { "cfefile", 1, 0, 0},
    { "rootfsfile", 1, 0, 0},
    { "kernelfile", 1, 0, 0},
    { "include-cfe", 0, 0, 'i'},
    { 0, 0, 0, 0 }
};

static char version[] = "Broadcom Image Builder version 0.3";

static char usage[] = "Usage:\t%s {-h|--help|-v|--version}\n"
                    "\t[--littleendian]\n"
                    "\t--chip <chipid> -- chip id {6328|6362|6368|6816}\n"
                    "\t--board <boardid> --blocksize {64|128}\n"
                    "\t--output <filename> --cfefile <filename>\n"
                    "\t[--rootfsfile <filename> --kernelfile <filename> [-i|--include-cfe]]\n";

static char *progname;

/***************************************************************************
// Function Name: getCrc32
// Description  : caculate the CRC 32 of the given data.
// Parameters   : pdata - array of data.
//                size - number of input data bytes.
//                crc - either CRC32_INIT_VALUE or previous return value.
// Returns      : crc.
****************************************************************************/
UINT32 getCrc32(byte *pdata, UINT32 size, UINT32 crc)
{
    while (size-- > 0)
        crc = (crc >> 8) ^ Crc32_table[(crc ^ *pdata++) & 0xff];

    return crc;
}

static void build_image(int littleendian, char *chipid, char *board, unsigned int blksize, 
    char *outputfile, char *cfefile, char *rootfsfile, char *kernelfile, int includecfe)
{
    FILE *stream_cfe = NULL;
    FILE *stream_rootfs = NULL;
    FILE *stream_kernel = NULL;
    FILE *stream_output = NULL;
    unsigned int total_size, cfe_size, rootfs_size, kernel_size, count, numWritten;
    unsigned int cfeaddr = IMAGE_BASE;
    unsigned int fskerneladdr;
    FILE_TAG tag;
    UINT32 imageCrc, tagCrc, fsCrc, kernelCrc;

    if ((stream_cfe = fopen(cfefile, "rb")) == NULL) {
        printf("bcmImageBuilder: Failed to open cfe file: %s\n", cfefile);
        fclose(stream_cfe);
        exit(1);
    }

    if (strlen(rootfsfile) != 0 && strlen(kernelfile) != 0) {
        if ((stream_rootfs = fopen(rootfsfile, "rb")) == NULL) {
            printf("bcmImageBuilder: Failed to open root file system file: %s\n", rootfsfile);
            fclose(stream_cfe);
            exit(1);
        }
        if ((stream_kernel = fopen(kernelfile, "rb")) == NULL) {
            printf("bcmImageBuilder: Filed to open kernel file: %s\n", kernelfile);
            fclose(stream_cfe);
            fclose(stream_rootfs);
            exit(1);
        }
    }

    if((stream_output = fopen(outputfile, "wb")) == NULL) {
        printf("bcmImageBuilder: Failed to create output file: %s\n", outputfile);
        fclose(stream_cfe);
        fclose(stream_kernel);
        fclose(stream_rootfs);
        exit(1);
    }

    total_size = cfe_size  = rootfs_size = kernel_size = count = 0;
    imageCrc = CRC32_INIT_VALUE;
    fsCrc = CRC32_INIT_VALUE;
    kernelCrc = CRC32_INIT_VALUE;

    while (!feof( stream_cfe )) {
        count = fread( buffer, sizeof(byte), BUF_SIZE, stream_cfe);
        if (ferror( stream_cfe)) {
            perror( "bcmImageBuilder: Read error" );
            exit(1);
        }
        imageCrc = getCrc32(buffer, count, imageCrc);
        /* Total up actual bytes read */
        total_size += count;
        cfe_size += count;
    }

    /* Calculate address after cfe in flash;  Tag is before fs now */
    fskerneladdr = cfeaddr + ((cfe_size + (blksize - 1)) & ~(blksize - 1));

    if (stream_rootfs && stream_kernel) {
        if (!includecfe) {
            /* CFE is not included in this image */
            total_size = 0;
            imageCrc = CRC32_INIT_VALUE;
            fclose(stream_cfe);
            stream_cfe = NULL;
        }

        while (!feof( stream_rootfs )) {
            count = fread( buffer, sizeof(byte), BUF_SIZE, stream_rootfs);
            if (ferror( stream_rootfs)) {
                perror( "bcmImageBuilder: Read error" );
                exit(1);
            }
            imageCrc = getCrc32(buffer, count, imageCrc);
            fsCrc = getCrc32(buffer, count, fsCrc);
            /* Total up actual bytes read */
            total_size += count;
            rootfs_size += count;
        }

        while (!feof( stream_kernel )) {
            count = fread( buffer, sizeof(byte), BUF_SIZE, stream_kernel );
            if (ferror( stream_kernel)) {
                perror( "bcmImageBuilder: Read error" );
                exit(1);
            }
            imageCrc = getCrc32(buffer, count, imageCrc);
            kernelCrc = getCrc32(buffer, count, kernelCrc);
            /* Total up actual bytes read */
            total_size += count;
            kernel_size += count;
        }
    }

    if (!stream_cfe) {
        cfeaddr = 0;
        cfe_size = 0;
    }

    if (!stream_rootfs || !stream_kernel) {
        fskerneladdr = 0;
        rootfs_size = 0;
        kernel_size = 0;
    }

    // fill the tag structure
    memset(&tag, 0, sizeof(FILE_TAG));

    strcpy(tag.tagVersion, BCM_TAG_VER);
    strncpy(tag.signiture_1, BCM_SIG_1, SIG_LEN -1);
    strncpy(tag.signiture_2, BCM_SIG_2, SIG_LEN_2 -1);
    strncpy(tag.chipId, chipid, CHIP_ID_LEN - 1);
    strncpy(tag.boardId, board, BOARD_ID_LEN -1);
    if (littleendian)
        strcpy(tag.bigEndian, "0");
    else
        strcpy(tag.bigEndian, "1");

    sprintf(tag.totalImageLen,"%d",total_size);

    sprintf(tag.cfeAddress, "%u", cfeaddr);
    sprintf(tag.cfeLen,"%d",cfe_size);

    if (!stream_rootfs || !stream_kernel) 
    {
        sprintf(tag.rootfsAddress,"%lu",(unsigned long) 0);
        sprintf(tag.rootfsLen,"%d",rootfs_size);
        sprintf(tag.kernelAddress,"%u", 0);
        sprintf(tag.kernelLen,"%d",kernel_size);
    }
    else
    {
        sprintf(tag.rootfsAddress,"%lu",(unsigned long) (fskerneladdr+TAG_LEN));
        sprintf(tag.rootfsLen,"%d",rootfs_size);
        sprintf(tag.kernelAddress,"%u", fskerneladdr + rootfs_size + TAG_LEN);
        sprintf(tag.kernelLen,"%d",kernel_size);
    }
    if (!littleendian) {
        imageCrc = htonl(imageCrc);
        fsCrc = htonl(fsCrc);
        kernelCrc = htonl(kernelCrc);
    }
    memcpy(tag.imageValidationToken, (byte*)&imageCrc, CRC_LEN);
    memcpy(tag.imageValidationToken + CRC_LEN, (byte*)&fsCrc, CRC_LEN);
    memcpy(tag.imageValidationToken + (CRC_LEN * 2), (byte*)&kernelCrc, CRC_LEN);

    // get tag crc
    tagCrc = CRC32_INIT_VALUE;
    tagCrc = getCrc32((byte*)&tag, TAG_LEN-TOKEN_LEN, tagCrc);
    if (!littleendian)
        tagCrc = htonl(tagCrc);
    memcpy(tag.tagValidationToken, (byte*)&tagCrc, CRC_LEN);

#ifdef DEBUG
    printf ("tagVersion %s\n", tag.tagVersion);
    printf ("signiture_1 %s\n", tag.signiture_1);
    printf ("signiture_2 %s\n", tag.signiture_2);
    printf ("chipId %s\n", tag.chipId);
    printf ("boardId %s\n", tag.boardId);
    printf ("bigEndian %s\n", tag.bigEndian);
    printf ("totalImageLen %s\n", tag.totalImageLen);
    printf ("cfeAddress %s, 0x%08X\n", tag.cfeAddress, (unsigned int)cfeaddr);
    printf ("cfeLen %s\n", tag.cfeLen);
    printf ("rootfsAddress %s, 0x%08X\n", tag.rootfsAddress, (unsigned int) fskerneladdr);
    printf ("rootfsLen %s\n", tag.rootfsLen);
    printf ("kernelAddress %s, 0x%08X\n", tag.kernelAddress, (unsigned int) (fskerneladdr + rootfs_size));
    printf ("kernelLen %s\n", tag.kernelLen);
#endif

    //----------------Start to write image file---------------------------
    //Write tag first
    numWritten = fwrite((byte*)&tag, sizeof(byte), TAG_LEN, stream_output);
    if (numWritten != TAG_LEN) {
        printf("bcmImageBuilder: Failed to write tag: byte written: %d byte\n", numWritten);
        exit(1);
    }

    //Write the cfe if exist
    if (stream_cfe) {
        rewind(stream_cfe);
        while (!feof( stream_cfe )) {
            count = fread( buffer, sizeof(byte), BUF_SIZE, stream_cfe );
            if (ferror(stream_cfe)) {
                perror("bcmImageBuilder: Read cfe file error");
                exit(1);
            }
            numWritten = fwrite(buffer, sizeof(byte), count, stream_output);
            if (ferror(stream_output)) {
                perror("bcmImageBuilder: Write image file error");
                exit(1);
            }
        }
        fclose(stream_cfe);
    }

    //Write the rootfs
    if (stream_rootfs) {
        rewind(stream_rootfs);
        while (!feof( stream_rootfs )) {
            count = fread( buffer, sizeof(byte), BUF_SIZE, stream_rootfs );
            if (ferror(stream_rootfs)) {
                perror("bcmImageBuilder: Read root file system file error");
                exit(1);
            }
            numWritten = fwrite(buffer, sizeof(byte), count, stream_output);
            if (ferror(stream_output)) {
                perror("bcmImageBuilder: Write image file error");
                exit(1);
            }
        }
        fclose(stream_rootfs);
    }

    //Write the kernel
    if (stream_kernel) {
        rewind(stream_kernel);
        while (!feof( stream_kernel )) {
            count = fread( buffer, sizeof(byte), BUF_SIZE, stream_kernel );
            if (ferror(stream_kernel)) {
                perror("bcmImageBuilder: Read kerenl file error");
                exit(1);
            }
            numWritten = fwrite(buffer, sizeof(byte), count, stream_output);
            if (ferror(stream_output)) {
                perror("bcmImageBuilder: Write image file error");
                exit(1);
            }
        }
        fclose(stream_kernel);
    }

    fclose(stream_output);

    printf("bcmImageBuilder\n");

    if (stream_cfe)
        printf("\tCFE image size                : %u\n", cfe_size);
    printf("\tFile tag size                 : %d\n", TAG_LEN);    
    if (stream_rootfs)
        printf("\tRoot filesystem image size    : %u\n", rootfs_size);
    if (stream_kernel)
        printf("\tKernel image size             : %u\n", kernel_size);

    printf("\tCombined image file size      : %u\n\n", total_size+TAG_LEN);
}


int main (int argc, char **argv)
{
  int optc;
  int h = 0, v = 0, littleendian = 0, includecfe = 0, lose = 0;
  int option_index = 0;
  char cfefile[256], rootfsfile[256], kernelfile[256], outputfile[256];
  char boardid[BOARD_ID_LEN];
  char chipid[CHIP_ID_LEN];
  unsigned int blksize = 0;

  progname = argv[0];
  chipid[0] = boardid[0] = cfefile[0] = kernelfile[0] = rootfsfile[0] = outputfile[0] = '\0';

  while ((optc = getopt_long (argc, argv, "hvli", longopts, &option_index)) != EOF) {
      switch (optc){
        case 0:
#ifdef DEBUG
          printf ("option %s", longopts[option_index].name);
          if (optarg)
             printf (" with arg %s", optarg);
          printf ("\n");
#endif
          if (strcmp(longopts[option_index].name, "cfefile") == 0) 
             strcpy(cfefile, optarg);
          else if (strcmp(longopts[option_index].name, "rootfsfile") == 0)
             strcpy(rootfsfile, optarg);
          else if (strcmp(longopts[option_index].name, "kernelfile") == 0)
             strcpy(kernelfile, optarg);
          else if (strcmp(longopts[option_index].name, "output") == 0)
             strcpy(outputfile, optarg);
          else if (strcmp(longopts[option_index].name, "chip") == 0)
             strcpy(chipid, optarg);
          else if (strcmp(longopts[option_index].name, "board") == 0)
             strcpy(boardid, optarg);
          else if (strcmp(longopts[option_index].name, "blocksize") == 0)
             blksize = atoi(optarg) * 1024;
          break;
	case 'v':
	  v = 1;
	  break;
	case 'h':
	  h = 1;
	  break;
	case 'l':
	  littleendian = 1;
	  break;
	case 'i':
	  includecfe = 1;
	  break;
        default:
          lose = 1;
          break;
      }
  }

  if (lose || optind < argc || argc < 2) 
  {
      /* Print error message and exit.  */
      fprintf (stderr, usage, progname);
      exit (1);
  }

  if (v) {
      /* Print version number.  */
    fprintf (stderr, "%s\n", version);
    if (! h)
        exit (0);
  }

  if (h) {
      /* Print help info and exit.  */
      fprintf (stderr, "%s\n", version);
      fprintf (stderr, usage, progname);
      fputs ("\n", stderr);
      fputs ("  -h, --help                  Print a summary of the options\n", stderr);
      fputs ("  -v, --version               Print the version number\n", stderr);
      fputs ("  -l, --littleendian          Build little endian image\n", stderr);
      fputs ("  --chip  <chip id>           Chip id\n", stderr);
      fputs ("  --board  <board name>       Board name\n", stderr);
      fputs ("  --blocksize  <block size>   Flash memory block size in KBytes\n", stderr);
      fputs ("  --output <filename>         Output image file name\n", stderr);
      fputs ("  --cfefile    <filename>     CFE imgage name\n", stderr);
      fputs ("  --kernelfile <filename>     Kernel image name\n", stderr);
      fputs ("  --rootfsfile <filename>     Root file system image name\n", stderr);
      fputs ("  -i, --include-cfe           Add CFE to kernel and rootfs image\n", stderr);
      exit (0);
  }


  if (strlen(chipid) == 0 || strlen(boardid) == 0 || strlen(outputfile) == 0 
      || strlen(cfefile) == 0 || (blksize != 64*1024 && blksize !=128*1024))
  {
      fprintf (stderr, "Required input parameters are missing\n\n");
      fprintf (stderr, usage, progname);
      exit (1);
  }
  if ((strlen(rootfsfile) == 0 && strlen(kernelfile) != 0) ||
      (strlen(rootfsfile) != 0 && strlen(kernelfile) == 0) )
  {
      fprintf (stderr, "Options --rootfsfile and --kernelfile must be used together\n\n");
      fprintf (stderr, usage, progname);
      exit (1);
  }
  if (strlen(chipid) > (CHIP_ID_LEN - 1))
  {
      fprintf (stderr, "Maximum chip id length is %d characters\n\n", (CHIP_ID_LEN - 1));
      fprintf (stderr, usage, progname);
      exit (1);
  }
  if (strlen(boardid) > 15)
  {
      fprintf (stderr, "Maximum board name length is 15 characters\n\n");
      fprintf (stderr, usage, progname);
      exit (1);
  }

  build_image(littleendian, chipid, boardid, blksize, outputfile, cfefile, rootfsfile, kernelfile, includecfe);

  exit(0);
}

