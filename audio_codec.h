/***********************************************
Copyright           : 2015 Leef Inc.
Filename            : audio_codec.h
Author              : bt731001@163.com
Description         : ---
Create              : 2019-07-12 23:09:37
Last Modified       : 2019-07-12 23:09:37
***********************************************/
#ifndef OPUS_ENCODE_H
#define OPUS_ENCODE_H
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "faad.h"
#include "faac.h"
#include "opus/opus.h"
#include "ogg/ogg.h"

#include "util.h"
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


class AAcDecoder {
  public:
    AAcDecoder();
    ~AAcDecoder();

  public:
    bool Init(unsigned char* data, size_t len);
    int CDecDecode(Packet *packet);
    int GetPcmPacket(Packet **packet);

  public:
    NeAACDecFrameInfo frame_info;
  private:
    NeAACDecHandle decoder;

    bool m_init;

  public:
    CObjectPool<Packet> m_aac_queue;
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
    int EncodeOpus(unsigned char* pcm_bytes, uint32_t pcm_len);
    int EncodeOpus(PcmPacket* pcm_packet);
  public:
    OpusEncoder *encode_hander;
    bool m_init;
    int m_channels;
    int m_sample_rates;
    int m_input_channels;
    int m_input_sample_rates;
    
    int m_frame_size;
    unsigned char m_pcm_cache[16*1024];
    uint32_t m_cache_len;
    unsigned char* m_pcm_packet;
    uint32_t m_packet_len;
    CObjectPool<Packet> m_opus_queue;
};

class CopusDecode {
public:
    CopusDecode();
    CopusDecode(int sample_rates, int chan_num);
    virtual ~CopusDecode();
public:
    int Init(int sample_rates, int chan_num);
    int DecodeOpus(unsigned char* opus_bytes, uint32_t opus_len);
private:
    CObjectPool<Packet> m_pcm_queue;
    OpusDecoder *decode_hander;
public:
    int m_sample_rates;
    int m_chan_num;
    int m_init;
};


class CaacEncode {
public:
    CaacEncode();
    ~CaacEncode();
public:
    int EncodeConfig(int sample_rates, int chan_num);
    int Init(int sample_rates, int chan_num);
    int AacEncode(unsigned char* pcm_bytes, uint32_t pcm_len);
private:
    faacEncHandle encode_hander;
    Packet* m_cache;
    Packet* m_out;
    uint32_t m_pcm_cache_len;
    const int m_pcm_bit_size;
public:
    CObjectPool<Packet> m_aac_queue;
    int m_sample_rates;
    int m_chan_num;
    unsigned int m_pcm_buff_size;
    unsigned int m_samplesInput;
    int m_init;

    int m_encode_sample_rates;
    int m_encode_chan_num;
};

#endif
