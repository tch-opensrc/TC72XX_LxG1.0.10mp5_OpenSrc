//*************************************************************************************
// Broadcom Corp. Confidential
// Copyright 2000, 2001, 2002 Broadcom Corp. All Rights Reserved.
//
// THIS SOFTWARE MAY ONLY BE USED SUBJECT TO AN EXECUTED
// SOFTWARE LICENSE AGREEMENT BETWEEN THE USER AND BROADCOM.
// YOU HAVE NO RIGHT TO USE OR EXPLOIT THIS MATERIAL EXCEPT
// SUBJECT TO THE TERMS OF SUCH AN AGREEMENT.
//
//**************************************************************************************
// File Name  : addvtoken.c
//
// Description: Add validation token - 20 bytes, to the firmware image file to 
//              be uploaded by CFE 'w' command. For now, just 4 byte crc in 
//              network byte order
//
// Created    : 08/18/2002  seanl
//**************************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>

typedef unsigned char byte;
typedef unsigned int UINT32;
#define BUF_SIZE        100 * 1024

#define  BCMTAG_EXE_USE
#include "bcmTag.h"

byte buffer[BUF_SIZE];
byte vToken[TOKEN_LEN];

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


int main(int argc, char **argv)
{
    FILE *stream_in, *stream_out;
    int total = 0, count, numWritten = 0;
    UINT32 imageCrc;

    if (argc != 3)
    {
        printf("Usage:\n");
        printf("addcrc input-file tag-output-file\n");
        exit(1);
    }
    if( (stream_in  = fopen(argv[1], "rb")) == NULL)
    {
	     printf("failed on open input file: %s\n", argv[1]);
		 exit(1);
	}
    if( (stream_out = fopen(argv[2], "wb+")) == NULL)
    {
	     printf("failed on open output file: %s\n", argv[2]);
	     exit(1);
    }

    total = count = 0;
    imageCrc = CRC32_INIT_VALUE;

    while( !feof( stream_in ) )
    {
        count = fread( buffer, sizeof( char ), BUF_SIZE, stream_in );
        if( ferror( stream_in ) )
        {
            perror( "Read error" );
            exit(1);
        }

        imageCrc = getCrc32(buffer, count, imageCrc);
        numWritten = fwrite(buffer, sizeof(char), count, stream_out);
        if (ferror(stream_out)) 
        {
            perror("addcrc: Write image file error");
            exit(1);
        }
        total += count;
    }
    
    // *** assume it is always in network byte order (big endia)
    imageCrc = htonl(imageCrc);
    memset(vToken, 0, TOKEN_LEN);
    memcpy(vToken, (byte*)&imageCrc, CRC_LEN);

    // write the crc to the end of the output file
    numWritten = fwrite(vToken, sizeof(char), TOKEN_LEN, stream_out);
    if (ferror(stream_out)) 
    {
        perror("addcrc: Write image file error");
        exit(1);
    }

    fclose(stream_in);
    fclose(stream_out);

    printf( "addvtoken: Output file size = %d with image crc = 0x%x\n", total+TOKEN_LEN, imageCrc);

	exit(0);
}
