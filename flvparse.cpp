#include "flvparse.h"

int InitBitBuf(BitBuf* root, unsigned char* p, int size) {
    root->bit_size = 0;
    root->len = size;
    root->bit_size = 8 * root->len;
    root->p = p;
    return 0;
}

int WriteBit(BitBuf* root, int val, int bit_size) {
    if (bit_size > root->bit_size) {
        debug("input bit size:%d buff bit size:%d", bit_size, root->bit_size);
        return -1;
    }
    if (bit_size > root->bit_size - root->bit_pos) {
        debug("input bit size:%d buff bit size:%d pos:%d", bit_size, root->bit_size, root->bit_pos);
        return -1;
    }
    while (root->bit_pos <= root->bit_size && bit_size > 0) {
        int byte_pos = root->bit_pos / 8;
        int shift_bit  = (8 - (root->bit_pos % 8)) -1;
        root->p[byte_pos] |= ((val >> (bit_size -1)) & 0x1) << shift_bit;
        root->bit_pos++;
        bit_size--;
    }
    return 0;
}

void parse(ADTSContext* p, const char *pHeaderData) {
    p->profileID = 1;
    p->sampleRateIdx = (pHeaderData[0] & 07) << 1 | pHeaderData[1] >> 7;
    p->chanNum = (pHeaderData[1] >> 3) & 0xf;
    debug("init adts cfg sampleRateIdx:%d chanNum:%d", p->sampleRateIdx, p->chanNum); 
}

int aac_decode_extradata(ADTSContext *adts, unsigned char *pbuf, int bufsize, int data_size) {
    BitBuf root;
    memset(&root, 0x00, sizeof(BitBuf));
    InitBitBuf(&root, pbuf, bufsize);
    WriteBit(&root, 0xfff, 12);
    WriteBit(&root, 0, 1);
    WriteBit(&root, 0, 2);
    WriteBit(&root, 1, 1);


    WriteBit(&root, adts->profileID, 2);
    WriteBit(&root, adts->sampleRateIdx, 4);
    WriteBit(&root, 0, 1);
    WriteBit(&root, adts->chanNum, 3);
    WriteBit(&root, 0, 1);
    WriteBit(&root, 0, 1);
    WriteBit(&root, 0, 1);
    WriteBit(&root, 0, 1);

    WriteBit(&root, data_size+7, 13);
    WriteBit(&root, 0x7ff, 11);
    WriteBit(&root, 0, 2);
    return 0;
}

