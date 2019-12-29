#ifndef FLVPARSE_H
#define FLVPARSE_H

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define ADTS_HEADER_SIZE    7

#ifndef debug
#define debug(fmt, args...) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt "\n", \
    __FILE__, __LINE__, __func__, ##args)
#endif

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


int InitBitBuf(BitBuf* root, unsigned char* p, int size) ;

int WriteBit(BitBuf* root, int val, int bit_size);

void parse(ADTSContext* p, const char *pHeaderData);

int aac_decode_extradata(ADTSContext *adts, unsigned char *pbuf, int bufsize, int data_size);
#endif 
