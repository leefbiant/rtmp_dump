/***********************************************
Copyright           : 2015 leef.biant Inc.
Filename            : ogg_packet.h
Author              : bt731001@163.com
Description         : ---
Create              : 2020-01-06 16:41:11
Last Modified       : 2020-01-06 16:41:11
 ***********************************************/
#include "ogg_packet.h"

void le32(unsigned char *p, int v) {
  p[0] = v & 0xff;
  p[1] = (v >> 8) & 0xff;
  p[2] = (v >> 16) & 0xff;
  p[3] = (v >> 24) & 0xff;
}

/* helper, write a little-endian 16 bit int to memory */
void le16(unsigned char *p, int v) {
  p[0] = v & 0xff;
  p[1] = (v >> 8) & 0xff;
}

ogg_packet *op_opushead(int samplerate, int channels) {
  int size = 19;
  unsigned char *data = (unsigned char*)malloc(size);
  ogg_packet *op = (ogg_packet*)malloc(sizeof(*op));

  if (!data) {
    debug("Couldn't allocate data buffer.\n");
    free(op);
    return NULL;
  }
  if (!op) {
    debug("Couldn't allocate Ogg packet.\n");
    free(data);
    return NULL;
  }

  memcpy(data, "OpusHead", 8);  /* identifier */
  data[8] = 1;                  /* version */
  data[9] = channels;           /* channels */
  le16(data+10, 0);             /* pre-skip */
  le32(data + 12, samplerate);  /* original sample rate */
  le16(data + 16, 0);           /* gain */
  data[18] = 0;                 /* channel mapping family */

  op->packet = data;
  op->bytes = size;
  op->b_o_s = 1;
  op->e_o_s = 0;
  op->granulepos = 0;
  op->packetno = 0;
  return op;
}

ogg_packet *op_opustags(void) {
  const char *identifier = "OpusTags";
  const char *vendor = "opus rtmp packet dump";
  int size = strlen(identifier) + 4 + strlen(vendor) + 4;
  unsigned char *data = (unsigned char*)malloc(size);
  ogg_packet *op = (ogg_packet*)malloc(sizeof(*op));

  if (!data) {
    debug("Couldn't allocate data buffer.\n");
    free(op);
    return NULL;
  }
  if (!op) {
    debug("Couldn't allocate Ogg packet.\n");
    free(data);
    return NULL;
  }

  memcpy(data, identifier, 8);
  le32(data + 8, strlen(vendor));
  memcpy(data + 12, vendor, strlen(vendor));
  le32(data + 12 + strlen(vendor), 0);

  op->packet = data;
  op->bytes = size;
  op->b_o_s = 0;
  op->e_o_s = 0;
  op->granulepos = 0;
  op->packetno = 1;
  return op;
}

void op_free(ogg_packet *op) {
  if (op) {
    if (op->packet) {
      free(op->packet);
    }
    free(op);
  }
}

ogg_packet *op_from_pkt(const unsigned char *pkt, int len) {
  ogg_packet *op = (ogg_packet*)malloc(sizeof(*op));
  if (!op) {
    debug("Couldn't allocate Ogg packet.\n");
    return NULL;
  }

  op->packet = (unsigned char *)pkt;
  op->bytes = len;
  op->b_o_s = 0;
  op->e_o_s = 0;
  return op;
}

int ogg_flush(state *params) {
  ogg_page page;
  size_t written;

  if (!params || !params->stream || !params->out) {
    return -1;
  }

  while (ogg_stream_flush(params->stream, &page)) {
    written = fwrite(page.header, 1, page.header_len, params->out);
    if (written != (size_t)page.header_len) {
      debug("Error writing Ogg page header\n");
      return -2;
    }
    written = fwrite(page.body, 1, page.body_len, params->out);
    if (written != (size_t)page.body_len) {
      debug("Error writing Ogg page body\n");
      return -3;
    }
  }

  return 0;
}

int ogg_write(state *params) {
  ogg_page page;
  size_t written;

  if (!params || !params->stream || !params->out) {
    return -1;
  }

  while (ogg_stream_pageout(params->stream, &page)) {
    written = fwrite(page.header, 1, page.header_len, params->out);
    if (written != (size_t)page.header_len) {
      debug("Error writing Ogg page header\n");
      return -2;
    }
    written = fwrite(page.body, 1, page.body_len, params->out);
    if (written != (size_t)page.body_len) {
      debug("Error writing Ogg page body\n");
      return -3;
    }
    fflush(params->out);
  }
  return 0;
}
