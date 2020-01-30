#ifndef MEDIAPARSE_H
#define MEDIAPARSE_H

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "util.h"

#define ADTS_HEADER_SIZE    7

typedef struct {
    unsigned char* p;
    int len;
    int bit_size;
    int bit_pos;
}BitBuf;

typedef struct
{
    int profileID;
    int sampleRateIdx;
    int chanNum;
}ADTSContext;
#define BIT_SET(a, x, n) (a)


int GetSamples(size_t idx);

int InitBitBuf(BitBuf* root, unsigned char* p, int size) ;

int WriteBit(BitBuf* root, int val, int bit_size);

int ReadBit(BitBuf* root, int *val, int bit_size);

void parse(ADTSContext* p, const char *pHeaderData);

int aac_decode_extradata(ADTSContext *adts, unsigned char *pbuf, int bufsize, int data_size);
int parser_adts_content(ADTSContext* adts, unsigned char *pbuf, int bufsize);
#endif 
