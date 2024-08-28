/* LzmaUtil.c -- Test application for LZMA compression
2023-03-07 : Igor Pavlov : Public domain */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "LzmaUtil.h"



static const ISzAlloc g_bsAlloc = { bsAlloc, bsFree };

void *bsAlloc(ISzAllocPtr p, size_t size)
{
    UNUSED_VAR(p);
    return malloc(size);
}

void bsFree(ISzAllocPtr p, void *address)
{
    UNUSED_VAR(p);
    free(address);
}


#if defined(BSPATCH_EXECUTABLE)

static CLzmaDec lzmadec;
static decode_t dec;

SRes decodeInit(uint8_t *header, size_t size, uint32_t patchsize)
{
    int i;
    decode_t *decinf = &dec;
    CLzmaDec *state = &lzmadec;

    memset(decinf, 0, sizeof(decode_t));

    if(size < HEADER_SIZE)
        return SZ_ERROR_DATA;

    for(i = 0; i < HEADER_SIZE - LZMA_PROPS_SIZE; i++)
        decinf->unpackSize += (UInt64)header[LZMA_PROPS_SIZE + i] << (i * 8);

    decinf->patchsize = patchsize;
    
    printf("unpackSize %d patchsize %d\n", decinf->unpackSize, decinf->patchsize);

    LzmaDec_CONSTRUCT(state);
    RINOK(LzmaDec_Allocate(state, header, LZMA_PROPS_SIZE, &g_bsAlloc));

    LzmaDec_Init(state);

    if(decinf->inBuf == NULL)
        decinf->inBuf = (Byte *)ISzAlloc_Alloc(&g_bsAlloc, IN_BUF_SIZE);

    if(decinf->outBuf == NULL)
    {
        decinf->outBuf = ringbuffer_init(OUT_BUF_SIZE * 2);
        assert(decinf->outBuf != NULL);
    }

    return SZ_OK;
}

void decodeUninit(void)
{
    decode_t *decinf = &dec;
    CLzmaDec *state = &lzmadec;

    LzmaDec_Free(state, &g_bsAlloc);

    if(decinf->inBuf)
    {
        ISzAlloc_Free(&g_bsAlloc, decinf->inBuf);
        decinf->inBuf = NULL;
    }
    
    if(decinf->outBuf)
        ringbuffer_free(decinf->outBuf);
}

int decodeGetData(struct bspatch_stream* stream, void* buffer, int length)
{
    size_t residue;
    size_t copied;

    residue = ringbuffer_size(dec.outBuf);

    while(residue < (size_t)length && dec.unpackSize > 0)
    {
        if(SZ_OK != decodeRead(stream))
            break;
        
        residue = ringbuffer_size(dec.outBuf);
    }
    
    if(residue < (size_t)length)
    {
        length = residue;
    }

    copied = ringbuffer_pop(dec.outBuf, (uint8_t *)buffer, (size_t)length);
    assert(copied == (size_t)length);

    return copied;
}

int32_t decodeRead(struct bspatch_stream* stream)
{
    int32_t res;
    decode_t *decinf = &dec;
    SizeT inProcessed;
    SizeT outProcessed = OUT_BUF_SIZE;
    ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
    ELzmaStatus status;

    if(decinf->inPos == decinf->inSize && decinf->patchsize > 0)
    {
        if(decinf->patchsize >= IN_BUF_SIZE)
            decinf->inSize = IN_BUF_SIZE;
        else
            decinf->inSize = decinf->patchsize;
        
        if(stream->rpatch(stream, decinf->inBuf, decinf->inSize))
            errx(1, "patch file read failed");

        decinf->inPos = 0;
        decinf->patchsize -= decinf->inSize;
    }

    inProcessed = decinf->inSize - decinf->inPos;

    if(outProcessed > decinf->unpackSize)
    {
        outProcessed = (SizeT)decinf->unpackSize;
        finishMode = LZMA_FINISH_END;
    }

    res = LzmaDecToBuf(&lzmadec, (void *)decinf->outBuf, &outProcessed,
                              decinf->inBuf + decinf->inPos, &inProcessed, finishMode, &status);
    decinf->inPos += inProcessed;
    decinf->unpackSize -= outProcessed;

    if(res != SZ_OK || decinf->unpackSize == 0)
        return res;

    if(inProcessed == 0 && outProcessed == 0)
    {
        if(status != LZMA_STATUS_FINISHED_WITH_MARK)
            return SZ_ERROR_DATA;
    }

    return res;
}

int32_t LzmaDecToBuf(CLzmaDec *p, uint8_t *dest, SizeT *destLen, 
    const uint8_t *src, SizeT *srcLen, 
    ELzmaFinishMode finishMode, ELzmaStatus *status)
{
  SizeT outSize = *destLen;
  SizeT inSize = *srcLen;
  
  *srcLen = *destLen = 0;
  
  for (;;)
  {
    SizeT inSizeCur = inSize, outSizeCur, dicPos;
    ELzmaFinishMode curFinishMode;
    int32_t res;
    
    if (p->dicPos == p->dicBufSize)
      p->dicPos = 0;
    dicPos = p->dicPos;
    if (outSize > p->dicBufSize - dicPos)
    {
      outSizeCur = p->dicBufSize;
      curFinishMode = LZMA_FINISH_ANY;
    }
    else
    {
      outSizeCur = dicPos + outSize;
      curFinishMode = finishMode;
    }

    res = LzmaDec_DecodeToDic(p, outSizeCur, src, &inSizeCur, curFinishMode, status);
    src += inSizeCur;
    inSize -= inSizeCur;
    *srcLen += inSizeCur;
    outSizeCur = p->dicPos - dicPos;
    
    if(ringbuffer_available((ringbuffer_t *)dest) >= outSizeCur)
    {
        assert(outSizeCur == ringbuffer_insert((ringbuffer_t *)dest, p->dic + dicPos, outSizeCur));
    }

    outSize -= outSizeCur;
    *destLen += outSizeCur;
    
    if (res != 0)
      return res;
    
    if (outSizeCur == 0 || outSize == 0)
      return SZ_OK;
  }
}

#endif /* BSPATCH_EXECUTABLE */


#if defined(BSDIFF_EXECUTABLE)

static const char *const kCantReadMessage = "Cannot read input file";
static const char *const kCantWriteMessage = "Cannot write output file";
static const char *const kCantAllocateMessage = "Cannot allocate memory";
static const char *const kDataErrorMessage = "Data error";

static int32_t Encode(ISeqOutStreamPtr outStream, ISeqInStreamPtr inStream, UInt64 fileSize)
{
    CLzmaEncHandle enc;
    int32_t res;
    CLzmaEncProps props;

    enc = LzmaEnc_Create(&g_bsAlloc);

    if(enc == 0)
        return SZ_ERROR_MEM;

    LzmaEncProps_Init(&props);
    res = LzmaEnc_SetProps(enc, &props);

    if(res == SZ_OK)
    {
        uint8_t header[LZMA_PROPS_SIZE + 8];
        size_t headerSize = LZMA_PROPS_SIZE;
        int i;

        res = LzmaEnc_WriteProperties(enc, header, &headerSize);

        for(i = 0; i < 8; i++)
            header[headerSize++] = (uint8_t)(fileSize >> (8 * i));

        if(outStream->Write(outStream, header, headerSize) != headerSize)
            res = SZ_ERROR_WRITE;
        else
        {
            if(res == SZ_OK)
                res = LzmaEnc_Encode(enc, outStream, inStream, NULL, &g_bsAlloc, &g_bsAlloc);
        }
    }

    LzmaEnc_Destroy(enc, &g_bsAlloc, &g_bsAlloc);
    return res;
}

int lzma_encode(const char *out, const char *in)
{
    CFileSeqInStream inStream;
    CFileOutStream outStream;
    int res;
    WRes wres;

    LzFindPrepare();

    FileSeqInStream_CreateVTable(&inStream);
    File_Construct(&inStream.file);
    inStream.wres = 0;

    FileOutStream_CreateVTable(&outStream);
    File_Construct(&outStream.file);
    outStream.wres = 0;

    wres = InFile_Open(&inStream.file, in);
    if(wres != 0)
        return printf("Cannot open input file %d\n", wres);

    wres = OutFile_Open(&outStream.file, out);
    if(wres != 0)
        return printf("Cannot open output file %d\n", wres);

    UInt64 fileSize;
    wres = File_GetLength(&inStream.file, &fileSize);

    if(wres != 0)
        return printf("Cannot get file length %d\n", wres);

    res = Encode(&outStream.vt, &inStream.vt, fileSize);

    File_Close(&outStream.file);

    File_Close(&inStream.file);

    if(res != SZ_OK)
    {
        if(res == SZ_ERROR_MEM)
            return printf("\nError: %s\n", kCantAllocateMessage);
        else if(res == SZ_ERROR_DATA)
            return printf("\nError: %s\n", kDataErrorMessage);
        else if(res == SZ_ERROR_WRITE)
            return printf("%s %d\n", kCantWriteMessage, outStream.wres);
        else if(res == SZ_ERROR_READ)
            return printf("%s %d\n", kCantReadMessage, inStream.wres);

        return printf("\n7-Zip error code: %d\n", res);
    }

    return 0;
}

#endif /* BSDIFF_EXECUTABLE */


