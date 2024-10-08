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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "bsdiff.h"

#define MIN(x,y) (((x)<(y)) ? (x) : (y))

static void split(int32_t *I, int32_t *V, int32_t start, int32_t len, int32_t h)
{
    int32_t i, j, k, x, tmp, jj, kk;

    if(len < 16)
    {
        for(k = start; k < start + len; k += j)
        {
            j = 1; x = V[I[k] + h];

            for(i = 1; k + i < start + len; i++)
            {
                if(V[I[k + i] + h] < x)
                {
                    x = V[I[k + i] + h];
                    j = 0;
                };

                if(V[I[k + i] + h] == x)
                {
                    tmp = I[k + j]; I[k + j] = I[k + i]; I[k + i] = tmp;
                    j++;
                };
            };

            for(i = 0; i < j; i++) V[I[k + i]] = k + j - 1;

            if(j == 1) I[k] = -1;
        };

        return;
    };

    x = V[I[start + len / 2] + h];
    jj = 0; kk = 0;

    for(i = start; i < start + len; i++)
    {
        if(V[I[i] + h] < x) jj++;

        if(V[I[i] + h] == x) kk++;
    };

    jj += start; kk += jj;

    i = start; j = 0; k = 0;

    while(i < jj)
    {
        if(V[I[i] + h] < x)
        {
            i++;
        }
        else if(V[I[i] + h] == x)
        {
            tmp = I[i]; I[i] = I[jj + j]; I[jj + j] = tmp;
            j++;
        }
        else
        {
            tmp = I[i]; I[i] = I[kk + k]; I[kk + k] = tmp;
            k++;
        };
    };

    while(jj + j < kk)
    {
        if(V[I[jj + j] + h] == x)
        {
            j++;
        }
        else
        {
            tmp = I[jj + j]; I[jj + j] = I[kk + k]; I[kk + k] = tmp;
            k++;
        };
    };

    if(jj > start) split(I, V, start, jj - start, h);

    for(i = 0; i < kk - jj; i++) V[I[jj + i]] = kk - 1;

    if(jj == kk - 1) I[jj] = -1;

    if(start + len > kk) split(I, V, kk, start + len - kk, h);
}

static void qsufsort(int32_t *I, int32_t *V, const uint8_t *pold, int32_t oldsize)
{
    int32_t buckets[256];
    int32_t i, h, len;

    for(i = 0; i < 256; i++) buckets[i] = 0;

    for(i = 0; i < oldsize; i++) buckets[pold[i]]++;

    for(i = 1; i < 256; i++) buckets[i] += buckets[i - 1];

    for(i = 255; i > 0; i--) buckets[i] = buckets[i - 1];

    buckets[0] = 0;

    for(i = 0; i < oldsize; i++) I[++buckets[pold[i]]] = i;

    I[0] = oldsize;

    for(i = 0; i < oldsize; i++) V[i] = buckets[pold[i]];

    V[oldsize] = 0;

    for(i = 1; i < 256; i++) if(buckets[i] == buckets[i - 1] + 1) I[buckets[i]] = -1;

    I[0] = -1;

    for(h = 1; I[0] != -(oldsize + 1); h += h)
    {
        len = 0;

        for(i = 0; i < oldsize + 1;)
        {
            if(I[i] < 0)
            {
                len -= I[i];
                i -= I[i];
            }
            else
            {
                if(len) I[i - len] = -len;

                len = V[I[i]] + 1 - i;
                split(I, V, i, len, h);
                i += len;
                len = 0;
            };
        };

        if(len) I[i - len] = -len;
    };

    for(i = 0; i < oldsize + 1; i++) I[V[i]] = i;
}

static int32_t matchlen(const uint8_t *pold, int32_t oldsize, const uint8_t *pnew, int32_t newsize)
{
    int32_t i;

    for(i = 0; (i < oldsize) && (i < newsize); i++)
        if(pold[i] != pnew[i]) break;

    return i;
}

static int32_t search(const int32_t *I, const uint8_t *pold, int32_t oldsize,
                      const uint8_t *pnew, int32_t newsize, int32_t st, int32_t en, int32_t *pos)
{
    int32_t x, y;

    if(en - st < 2)
    {
        x = matchlen(pold + I[st], oldsize - I[st], pnew, newsize);
        y = matchlen(pold + I[en], oldsize - I[en], pnew, newsize);

        if(x > y)
        {
            *pos = I[st];
            return x;
        }
        else
        {
            *pos = I[en];
            return y;
        }
    };

    x = st + (en - st) / 2;

    if(memcmp(pold + I[x], pnew, MIN(oldsize - I[x], newsize)) < 0)
    {
        return search(I, pold, oldsize, pnew, newsize, x, en, pos);
    }
    else
    {
        return search(I, pold, oldsize, pnew, newsize, st, x, pos);
    };
}

static void offtout(int32_t x, uint8_t *buf)
{
    int32_t y;

    if(x < 0) y = -x; else y = x;

    buf[0] = y % 256; y -= buf[0];
    y = y / 256; buf[1] = y % 256; y -= buf[1];
    y = y / 256; buf[2] = y % 256; y -= buf[2];
    y = y / 256; buf[3] = y % 256; y -= buf[3];
    y = y / 256; buf[4] = y % 256; y -= buf[4];
    y = y / 256; buf[5] = y % 256; y -= buf[5];
    y = y / 256; buf[6] = y % 256; y -= buf[6];
    y = y / 256; buf[7] = y % 256;

    if(x < 0) buf[7] |= 0x80;
}

static int32_t writedata(struct bsdiff_stream *stream, const void *buffer, int32_t length)
{
    int32_t result = 0;

    while(length > 0)
    {
        const int smallsize = (int)MIN(length, INT_MAX);
        const int writeresult = stream->write(stream, buffer, smallsize);

        if(writeresult == -1)
        {
            return -1;
        }

        result += writeresult;
        length -= smallsize;
        buffer = (uint8_t *)buffer + smallsize;
    }

    return result;
}

struct bsdiff_request
{
    const uint8_t *old;
    int32_t oldsize;
    const uint8_t *new;
    int32_t newsize;
    struct bsdiff_stream *stream;
    int32_t *I;
    uint8_t *buffer;
};

static int bsdiff_internal(const struct bsdiff_request req)
{
    int32_t *I, *V;
    int32_t scan, pos, len;
    int32_t lastscan, lastpos, lastoffset;
    int32_t oldscore, scsc;
    int32_t s, Sf, lenf, Sb, lenb;
    int32_t overlap, Ss, lens;
    int32_t i;
    uint8_t *buffer;
    uint8_t buf[8 * 3];

    if((V = req.stream->malloc((req.oldsize + 1) * sizeof(int32_t))) == NULL) return -1;

    I = req.I;

    qsufsort(I, V, req.old, req.oldsize);
    req.stream->free(V);

    buffer = req.buffer;

    /* Compute the differences, writing ctrl as we go */
    scan = 0; len = 0; pos = 0;
    lastscan = 0; lastpos = 0; lastoffset = 0;

    while(scan < req.newsize)
    {
        oldscore = 0;

        for(scsc = scan += len; scan < req.newsize; scan++)
        {
            len = search(I, req.old, req.oldsize, req.new + scan, req.newsize - scan,
                         0, req.oldsize, &pos);

            for(; scsc < scan + len; scsc++)
                if((scsc + lastoffset < req.oldsize) &&
                        (req.old[scsc + lastoffset] == req.new[scsc]))
                    oldscore++;

            if(((len == oldscore) && (len != 0)) ||
                    (len > oldscore + 8)) break;

            if((scan + lastoffset < req.oldsize) &&
                    (req.old[scan + lastoffset] == req.new[scan]))
                oldscore--;
        };

        if((len != oldscore) || (scan == req.newsize))
        {
            s = 0; Sf = 0; lenf = 0;

            for(i = 0; (lastscan + i < scan) && (lastpos + i < req.oldsize);)
            {
                if(req.old[lastpos + i] == req.new[lastscan + i]) s++;

                i++;

                if(s * 2 - i > Sf * 2 - lenf)
                {
                    Sf = s;
                    lenf = i;
                };
            };

            lenb = 0;

            if(scan < req.newsize)
            {
                s = 0; Sb = 0;

                for(i = 1; (scan >= lastscan + i) && (pos >= i); i++)
                {
                    if(req.old[pos - i] == req.new[scan - i]) s++;

                    if(s * 2 - i > Sb * 2 - lenb)
                    {
                        Sb = s;
                        lenb = i;
                    };
                };
            };

            if(lastscan + lenf > scan - lenb)
            {
                overlap = (lastscan + lenf) - (scan - lenb);
                s = 0; Ss = 0; lens = 0;

                for(i = 0; i < overlap; i++)
                {
                    if(req.new[lastscan + lenf - overlap + i] ==
                            req.old[lastpos + lenf - overlap + i]) s++;

                    if(req.new[scan - lenb + i] ==
                            req.old[pos - lenb + i]) s--;

                    if(s > Ss)
                    {
                        Ss = s;
                        lens = i + 1;
                    };
                };

                lenf += lens - overlap;

                lenb -= lens;
            };

            offtout(lenf, buf);

            offtout((scan - lenb) - (lastscan + lenf), buf + 8);

            offtout((pos - lenb) - (lastpos + lenf), buf + 16);

            /* Write control data */
            if(writedata(req.stream, buf, sizeof(buf)))
                return -1;

            /* Write diff data */
            for(i = 0; i < lenf; i++)
                buffer[i] = req.new[lastscan + i] - req.old[lastpos + i];

            if(writedata(req.stream, buffer, lenf))
                return -1;

            /* Write extra data */
            for(i = 0; i < (scan - lenb) - (lastscan + lenf); i++)
                buffer[i] = req.new[lastscan + lenf + i];

            if(writedata(req.stream, buffer, (scan - lenb) - (lastscan + lenf)))
                return -1;

            lastscan = scan - lenb;
            lastpos = pos - lenb;
            lastoffset = pos - scan;
        };
    };

    return 0;
}

int bsdiff(const uint8_t *pold, int32_t oldsize, const uint8_t *pnew, int32_t newsize, struct bsdiff_stream *stream)
{
    int result;
    struct bsdiff_request req;

    if((req.I = stream->malloc((oldsize + 1) * sizeof(int32_t))) == NULL)
        return -1;

    if((req.buffer = stream->malloc(newsize + 1)) == NULL)
    {
        stream->free(req.I);
        return -1;
    }

    req.old = pold;
    req.oldsize = oldsize;
    req.new = pnew;
    req.newsize = newsize;
    req.stream = stream;

    result = bsdiff_internal(req);

    stream->free(req.buffer);
    stream->free(req.I);

    return result;
}


//#define BSDIFF_EXECUTABLE

#if defined(BSDIFF_EXECUTABLE)

#include <stdarg.h>
#include <stdio.h>
#include "../lzma/LzmaUtil/LzmaUtil.h"

//#define errx err
void err(int exitcode, const char *fmt, ...)
{
    va_list valist;
    va_start(valist, fmt);
    vprintf(fmt, valist);
    va_end(valist);
    exit(exitcode);
}

static int lzma_write(struct bsdiff_stream *stream, const void *buffer, int size)
{
    memcpy(stream->opaque + stream->size, buffer, size);
    stream->size += size;

    return 0;
}

static int read_finfo(const char *f, unsigned char **p, int32_t *size)
{
    FILE *fs;
    int32_t len;
    unsigned char *pf = NULL;

    /* Allocate oldsize+1 bytes instead of oldsize bytes to ensure
    	that we never try to malloc(0) and get a NULL pointer */
    fs = fopen(f, "rb");

    if(fs == NULL)errx(1, "Open failed :%s", f);

    if(fseek(fs, 0, SEEK_END) != 0)errx(1, "Seek failed :%s", f);

    len = ftell(fs);

    if(size)
        *size = len;

    if(p)
    {
        pf = (unsigned char *)malloc(len + 1);

        if(pf == NULL)	errx(1, "Malloc failed :%s", f);

        fseek(fs, 0, SEEK_SET);

        if(fread(pf, 1, len, fs) == 0)	errx(1, "Read failed :%s", f);
        
        *p = pf;
    }
    
    if(fclose(fs) == -1)	errx(1, "Close failed :%s", f);


    return len;
}

static void set_header(unsigned char *header, int32_t oldsize, int32_t newsize, int32_t patchsize)
{
    /* Header is
    	0	8	 "BSDIFF40"
    	8	8	length of old file
    	16	8	length of new file
    	24	8	length of patch file */

    memcpy(header, "BSDIFF40", 8);
    offtout(oldsize, header + 8);
    offtout(newsize, header + 16);
    offtout(patchsize, header + 24);
}

static int patch_write(const char *fp, unsigned char *data, int32_t size, int32_t offset)
{
    FILE *fs;
    /* Create and write the temp patch file */
    if(offset == -1)
    {
        if((fs = fopen(fp, "wb+")) == NULL)
            errx(1, "Open failed (%s)", fp);
    }
    else
    {
        if((fs = fopen(fp, "rb+")) == NULL)
            errx(1, "Open failed (%s)", fp);
        
        if(fseek(fs, offset, SEEK_SET) != 0)
            errx(1, "offset failed (%s)", fp);
    }

    if(fwrite(data, size, 1, fs) != 1)
        errx(1, "fwrite failed (%s)", fp);

    if(fclose(fs))
        errx(1, "fclose failed (%s)", fp);

    return 0;
}

int main(int argc, char *argv[])
{
    const char *tmp_patch = "tmp_patch";
    unsigned char header[32];
    unsigned char *pold, *pnew, *ppatch;
    int32_t oldsize, newsize, patchsize;
    int32_t len;

    struct bsdiff_stream stream;

    if(argc != 4) errx(1, "usage: %s oldfile newfile patchfile\n", argv[0]);

    read_finfo(argv[1], &pold, &oldsize);
    read_finfo(argv[2], &pnew, &newsize);

    len = LZMA_PROPS_SIZE + newsize + newsize / 3 + 128;
    ppatch = (unsigned char *)malloc(len);
    assert(ppatch != NULL);

    stream.malloc = malloc;
    stream.free = free;
    stream.write = lzma_write;
    stream.opaque = ppatch;
    stream.size = 0;

    if(bsdiff(pold, oldsize, pnew, newsize, &stream))
        errx(1, "bsdiff error !!!");

    patch_write(tmp_patch, ppatch, stream.size, -1);

    free(pold);
    free(pnew);
    free(ppatch);

    if(lzma_encode(argv[3], tmp_patch))
        errx(1, "lzma error !!!");

    read_finfo(argv[3], &ppatch, &patchsize);
    set_header(header, oldsize, newsize, patchsize);
    
    patch_write(argv[3], header, sizeof(header), -1);
    patch_write(argv[3], ppatch, patchsize, sizeof(header));
    
    free(ppatch);

    return 0;
}


#endif

