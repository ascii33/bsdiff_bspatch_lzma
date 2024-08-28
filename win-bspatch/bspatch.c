/*-
 * Copyright 2003-2005 Colin Percival
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include "bspatch.h"

#define BSPATCH_TRANSFER_SIZE    1024

static int32_t offtin(uint8_t *buf)
{
    int32_t y;

    y = buf[7] & 0x7F;
    y = y * 256; y += buf[6];
    y = y * 256; y += buf[5];
    y = y * 256; y += buf[4];
    y = y * 256; y += buf[3];
    y = y * 256; y += buf[2];
    y = y * 256; y += buf[1];
    y = y * 256; y += buf[0];

    if(buf[7] & 0x80) y = -y;

    return y;
}

int bspatch(struct bspatch_stream *stream, int32_t oldsize, int32_t newsize)
{
    uint8_t *buf, *told;
    int32_t oldpos, newpos, len;
    int32_t ctrl[3];
    int32_t i;

    buf = (uint8_t*)malloc(BSPATCH_TRANSFER_SIZE + 1);
    if(buf == NULL)
        return -1;

    told = (uint8_t*)malloc(BSPATCH_TRANSFER_SIZE + 1);
    if(told == NULL)
        return -1;

    oldpos = 0; newpos = 0;

    while(newpos < newsize)
    {
        /* Read control data */
        for(i = 0; i <= 2; i++)
        {
            if(stream->read(stream, buf, 8))
                return -1;

            ctrl[i] = offtin(buf);
        };

        /* Sanity-check */
        if(ctrl[0] < 0 || ctrl[0] > INT_MAX ||
                ctrl[1] < 0 || ctrl[1] > INT_MAX ||
                newpos + ctrl[0] > newsize)
            return -1;

#if 0
        /* Read diff string */
        if(stream->read(stream, pnew + newpos, ctrl[0]))
            return -1;

        /* Add pold data to diff string */
        for(i = 0; i < ctrl[0]; i++)
            if((oldpos + i >= 0) && (oldpos + i < oldsize))
                pnew[newpos + i] += pold[oldpos + i];

        /* Adjust pointers */
        newpos += ctrl[0];
        oldpos += ctrl[0];
#endif /* 0 */

        while(ctrl[0] > 0)
        {
            if(ctrl[0] > BSPATCH_TRANSFER_SIZE)
                len = BSPATCH_TRANSFER_SIZE;
            else
                len = ctrl[0];
            
            /* Read diff string */
            if (stream->read(stream, buf, len))
                return -1;
            
            /* Read old string */
            if (stream->rold(stream, oldpos, told, len))
                return -1;

            /* Add pold data to diff string */
            for (i = 0; i < len; i++)
            {
                if ((oldpos + i >= 0) && (oldpos + i < oldsize))
                {
                    buf[i] += told[i];
                    //buf[i] += pold[oldpos + i];
                }
            }

            stream->write(stream, buf, len);
            
            ctrl[0] -= len;
            oldpos += len;
            newpos += len;
        }

        /* Sanity-check */
        if(newpos + ctrl[1] > newsize)
            return -1;

        /* Read extra string */
#if 0
        if(stream->read(stream, pnew + newpos, ctrl[1]))
            return -1;
#endif /* 0 */
        while(ctrl[1] > 0)
        {
            if(ctrl[1] > BSPATCH_TRANSFER_SIZE)
                len = BSPATCH_TRANSFER_SIZE;
            else
                len = ctrl[1];
            
            if (stream->read(stream, buf, len))
                return -1;
            
            stream->write(stream, buf, len);
            ctrl[1] -= len;
            newpos += len;
        }

        /* Adjust pointers */
        //newpos += ctrl[1];
        oldpos += ctrl[2];
    };
    
    if(buf)
        free(buf);
    
    if(told)
        free(told);

    return 0;
}


//#define BSPATCH_EXECUTABLE

#if defined(BSPATCH_EXECUTABLE)

#include <string.h>
#include <stdarg.h>
#include "../lzma/LzmaUtil/LzmaUtil.h"

void err(int exitcode, const char *fmt, ...)
{
    va_list valist;
    va_start(valist, fmt);
    vprintf(fmt, valist);
    va_end(valist);
    exit(exitcode);
}

static int read_patch(struct bspatch_stream* stream, void *buf,  int count)
{
    if(fread(buf, 1, count, stream->opaque_r) == 0)
        return -1;

    return 0;
}

static int read_old(struct bspatch_stream* stream, uint32_t offset, void *buf,  int count)
{
    if(fseek(stream->opaque_old, offset, SEEK_SET) != 0)
        return -1;

    if(fread(buf, 1, count, stream->opaque_old) == 0)
        return -1;

    return 0;
}

static int lzma_read(struct bspatch_stream* stream, void* buffer, int length)
{
	int n = decodeGetData(stream, buffer, length);
    if (n != length)
        return -1;

	return 0;
}

static int data_write(struct bspatch_stream* stream, void* buffer, int length)
{
    if(fwrite(buffer, 1, length, stream->opaque_w) == 0)
		return -1;

	return 0;
}

static int get_header(unsigned char *header, uint32_t *oldsize, uint32_t *newsize, uint32_t *patchsize)
{
    uint32_t o, n, p;

    /* Header format:
        0	8	"BSDIFF40"
        8	8	old file size
        16	8	new file size
        24	8	patch file size */

    /* Check for appropriate magic */
    if(memcmp(header, "BSDIFF40", 8) != 0)
        errx(1, "Corrupt patch\n");

    /* Read lengths from header */
    o = offtin(header + 8);
    n = offtin(header + 16);
    p = offtin(header + 24);
    
    printf("old %d new %d patch %d\n", o, n, p);

    if((o == 0) || (n == 0) || (p == 0))
        errx(1, "Corrupt patch\n");

    *oldsize = o;
    *newsize = n;
    *patchsize = p;

    return 0;
}

int main(int argc, char *argv[])
{
    FILE *fpatch, *fold, *fnew;
    uint32_t oldsize, newsize, patchsize;
	struct bspatch_stream stream;
    unsigned char header[32];
    unsigned char dec_h[HEADER_SIZE];

    if(argc != 4) errx(1, "usage: %s oldfile newfile patchfile\n", argv[0]);

    /* Open patch file */
    if((fpatch = fopen(argv[3], "rb")) == NULL)
        errx(1, "fopen(%s)", argv[3]);

    if(fread(header, 1, sizeof(header), fpatch) == 0)
    {
        if(feof(fpatch))
            errx(1, "Corrupt patch\n");

        errx(1, "fread(%s)", argv[3]);
    }

    get_header(header, &oldsize, &newsize, &patchsize);

    /* Read decoder header */
    if(fread(dec_h, 1, sizeof(dec_h), fpatch) == 0)
    {
        if(feof(fpatch))
            errx(1, "Corrupt patch\n");

        errx(1, "fread(%s)", argv[3]);
    }

    /* create new file */
    fnew = fopen(argv[2], "wb+");
    if(fnew == NULL)errx(1, "Open failed :%s", argv[2]);

    /* Open old file */
    fold = fopen(argv[1], "rb+");
    if(fold == NULL)errx(1, "Open failed :%s", argv[1]);

    decodeInit(dec_h, sizeof(dec_h), patchsize);
    
	stream.read = lzma_read;
    stream.rpatch = read_patch;
	stream.opaque_r = fpatch;
    
    stream.write = data_write;
    stream.opaque_w = fnew;
    
    stream.rold = read_old;
	stream.opaque_old = fold;

    if (bspatch(&stream, oldsize, newsize))
		errx(1, "bspatch");

    decodeUninit();

    if(fclose(fold) == -1)
        errx(1, "Close failed :%s", argv[1]);
    
    if(fclose(fnew) == -1)
        errx(1, "fclose(%s)", argv[2]);

    if(fclose(fpatch) == -1)
        errx(1, "fclose(%s)", argv[3]);

    return 0;
}


#endif /* BSPATCH_EXECUTABLE */

