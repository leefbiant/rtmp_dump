/***********************************************
Copyright           : 2015 youme.im Inc.
Filename            : opus_encode.h
Author              : leef@youme.im
Description         : ---
Create              : 2019-07-12 23:09:37
Last Modified       : 2019-07-12 23:09:37
***********************************************/
#ifndef OPUS_ENCODE_H
#define OPUS_ENCODE_H
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <opus.h>
#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "faad.h"
#include "Util.h"
#include <ogg/ogg.h>
#include "thirdparty/glog/logging.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "samplerate.h"

#ifdef __cplusplus 
}
#endif

/*The frame size is hardcoded for this sample code but it doesn't have to be*/
#define FRAME_SIZE 960
#define SAMPLE_RATE 48000
#define CHANNELS 1
#define APPLICATION OPUS_APPLICATION_AUDIO
#define BITRATE 64000
#define MAX_FRAME_SIZE 6*960
#define MAX_PACKET_SIZE (3*1276) 

#ifndef debug
#define debug(fmt, args...) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt "\n", \
    __FILE__, __LINE__, __func__, ##args)
#endif

typedef struct {
  ogg_stream_state *stream;
  FILE *out;
  ogg_int64_t granulepos;
} state;


class CpcmCodec {
  public:
    CpcmCodec();
    ~CpcmCodec();

  public:
    bool Init(unsigned char* data, size_t len);
    int CDecDecode(unsigned char* data, size_t len);
    int GetPcmPacket(Packet** packet);
  private:
    NeAACDecFrameInfo frame_info;
    NeAACDecHandle decoder;

    unsigned char frame[FRAME_MAX_LEN];
    unsigned char buffer[FRAME_MAX_LEN * 2];
    uint32_t cache_len;
    bool m_init;
    SRC_STATE* m_samplerate_encode;
  public:
    CObjectPool<Packet> m_pcm_queue;
    unsigned long m_samplerate;
    unsigned char m_channels;
};

class CopusEncode {
  public:
    CopusEncode();
    CopusEncode(int sample_rates, int chan_num);
    ~CopusEncode();
  public:
    int Init(int sample_rates, int chan_num);
    int packet_set(unsigned char* pcm_bytes, uint32_t pcm_len);
    int packet_get();
    int EncodeAAc(unsigned char* pcm_bytes, uint32_t pcm_len);
  public:
    OpusEncoder *encode_hander;
    bool m_init;
    int m_channels;
    int m_sample_rates;
    int m_frame_size;
    unsigned char m_pcm_cache[16*1024];
    uint32_t m_cache_len;
    unsigned char* m_pcm_packet;
    uint32_t m_packet_len;
    CObjectPool<Packet> m_opus_queue;
    bool m_ogg_init;
    FILE* m_ogg_file;
    state *params;
};
#endif
