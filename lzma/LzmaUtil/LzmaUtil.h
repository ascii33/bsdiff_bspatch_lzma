/* LzmaUtil.c -- Test application for LZMA compression
2023-03-07 : Igor Pavlov : Public domain */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __LZMAUTIL_H__
#define __LZMAUTIL_H__


#include "../7zFile.h"
#include "../7zVersion.h"
#include "../LzFind.h"
#include "../LzmaDec.h"
#include "../LzmaEnc.h"
#include "ringbuffer.h"
#include "../../win-bspatch/bspatch.h"


#undef MY_CPU_NAME


#define HEADER_SIZE (LZMA_PROPS_SIZE + 8)

#define IN_BUF_SIZE (1 << 10)
#define OUT_BUF_SIZE (1 << 10)

typedef struct
{
    size_t inPos;
    size_t inSize;
    size_t outPos;
    uint32_t unpackSize;
    uint32_t patchsize;
    
    uint8_t *inBuf;
    //uint8_t *outBuf;
    ringbuffer_t *outBuf;
} decode_t;


#define UNUSED_VAR(x) (void)x;

void *bsAlloc(ISzAllocPtr p, size_t size);

void bsFree(ISzAllocPtr p, void *address);

#if defined(BSPATCH_EXECUTABLE)
int32_t decodeInit(uint8_t *header, size_t size, uint32_t patchsize);

void decodeUninit(void);

int decodeGetData(struct bspatch_stream* stream, void* buffer, int length);

int32_t decodeRead(struct bspatch_stream* stream);

int32_t LzmaDecToBuf(CLzmaDec *p, uint8_t *dest, SizeT *destLen, 
    const uint8_t *src, SizeT *srcLen, 
    ELzmaFinishMode finishMode, ELzmaStatus *status);
#endif

#if defined(BSDIFF_EXECUTABLE)
int lzma_encode(const char *out, const char *in);
#endif

#endif /* __LZMAUTIL_H__ */


