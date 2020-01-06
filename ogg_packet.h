/***********************************************
Copyright           : 2015 leef.biant Inc.
Filename            : ogg_packet.h
Author              : bt731001@163.com
Description         : ---
Create              : 2020-01-06 16:41:11
Last Modified       : 2020-01-06 16:41:11
 ***********************************************/
#ifndef OGG_PACKET_H
#define OGG_PACKET_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ogg/ogg.h"
#include "util.h"

typedef struct {
  ogg_stream_state *stream;
  FILE *out;
  ogg_int64_t granulepos;
}state;

void le32(unsigned char *p, int v);
void le16(unsigned char *p, int v);
ogg_packet *op_opushead(int samplerate, int channels);
ogg_packet *op_opustags(void);
void op_free(ogg_packet *op);
ogg_packet *op_from_pkt(const unsigned char *pkt, int len);
int ogg_write(state *params);
int ogg_flush(state *params);
#endif
