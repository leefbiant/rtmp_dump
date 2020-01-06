/***********************************************
Copyright           : 2015 youme.im Inc.
Filename            : opus_encode.cpp
Author              : leef@youme.im
Description         : ---
Create              : 2019-07-12 23:09:12
Last Modified       : 2019-07-12 23:09:12
***********************************************/
#include "opus_encode.h"
#include <math.h>

int sampleRates [] = {96000,
    88200, 64000, 48000, 44100, 32000,24000,
    22050, 16000, 12000, 11025, 8000, 7350};

CpcmCodec::CpcmCodec() {
  decoder = NeAACDecOpen();
  m_samplerate = 0;
  m_channels = 0;
  m_init = false;
  m_dst_channels = 2;
}

CpcmCodec::~CpcmCodec() {
  if (decoder) {
    NeAACDecClose(decoder);
  }
}

bool CpcmCodec::Init(unsigned char* data, size_t len, int dst_channels) {
  if (m_init) return true;
  m_dst_channels = dst_channels;
  NeAACDecInit(decoder, data, len, &m_samplerate, &m_channels);
  m_init = true;
  debug("init sucess channels:%u samplerate:%lu", m_channels, m_samplerate);
  return true;
}


int CpcmCodec::CDecDecode(Packet* packet) {
  if (!m_init) {
    if (!Init((unsigned char*)packet->len, packet->len)) {
      debug("init failed");
      return -1;
    }
  }

  // check adts ????

  m_aac_queue.SetObject(packet);
  while (0 == m_aac_queue.GetObject(&packet) && packet) {
    unsigned char *pcm_data = (unsigned char *)NeAACDecDecode(decoder, &frame_info, (unsigned char*)packet->data, packet->len);
    delete packet;
    if (frame_info.error != 0 || !pcm_data || frame_info.samples <= 0) {
      debug("error packet");
      continue;
    }
    uint32_t pcm_len = frame_info.samples * frame_info.channels;
    packet = new Packet;
    // 重采样
    if (m_samplerate != SAMPLE_RATE) {
      static float in[4096] = {0};
      static float out[4096] = {0};
      src_short_to_float_array((short *)pcm_data, in, pcm_len / sizeof(short));
      static char sample_buff[16 * 1024] = {0};
      SRC_DATA src_data;
      memset(&src_data, 0x00, sizeof(SRC_DATA));
      src_data.src_ratio = 1.0 * SAMPLE_RATE / m_samplerate;
      src_data.end_of_input = 0;
      src_data.input_frames = frame_info.samples;
      src_data.output_frames = sizeof(out);
      src_data.data_in = in;
      src_data.data_out = out;
      int error = 0;
      do  {
        if ((error = src_simple(&src_data, SRC_ZERO_ORDER_HOLD, frame_info.channels)))  {
          debug("src_simple err frae_info.channels:%d : %s", frame_info.channels, src_strerror(error));
          delete packet;
          break;
        }
        uint32_t pcm_sample_len = src_data.output_frames_gen * frame_info.channels;
        src_float_to_short_array(out, (short *)sample_buff, pcm_sample_len);
        if (frame_info.channels == 2 && m_dst_channels == 1)
        {
          short *audio_buff = (short *)sample_buff;
          for (uint32_t j = 0; j < pcm_sample_len / sizeof(short); ++j)
          {
            audio_buff[j] = (audio_buff[2 * j] + audio_buff[2 * j + 1]) >> 1;
          }
          pcm_sample_len /= 2;
        }
        packet = new Packet(sample_buff, pcm_sample_len);
        m_pcm_queue.SetObject(packet);
      } while (0);
    }  else {
      packet = new Packet((const char*)pcm_data, pcm_len);
      m_pcm_queue.SetObject(packet);
    }
  }
  return m_pcm_queue.GetNum();
}


int CpcmCodec::GetPcmPacket(Packet** packet) {
  m_pcm_queue.GetObject(packet);
  return (*packet) ? 0 : -1;
}

CopusEncode::CopusEncode() {
  m_init = false;
}


int CopusEncode::Init(int sample_rates, int chan_num) {
  if (m_init) return 0;
  this->m_channels = chan_num ? chan_num : CHANNELS;
  // this->m_sample_rates = sample_rates ? sample_rates : SAMPLE_RATE;
  this->m_sample_rates = SAMPLE_RATE;
  this->m_frame_size = FRAME_SIZE;

  int err = 0;
  encode_hander = opus_encoder_create(m_sample_rates, m_channels, APPLICATION, &err);
  if (err < 0) {
    debug("opus_encoder_create failed channels:%d ample_rates:%d", m_channels, m_sample_rates);
    return -1;
  }
  err = opus_encoder_ctl(encode_hander, OPUS_SET_BITRATE(BITRATE)); 
  if (err < 0) {
    debug("opus_encoder_ctl failed");
    return -1;
  }
  m_packet_len = sizeof(short)*m_channels * m_frame_size;
  m_pcm_packet = new unsigned char[m_packet_len];
  m_init = true;
  debug("channels:%d ample_rates:%d, packet_len:%d", m_channels, m_sample_rates, m_packet_len);
  return 0;
}

CopusEncode::~CopusEncode() {
  if (encode_hander) {
    opus_encoder_destroy(encode_hander);
  }
  if (m_pcm_packet) {
    delete[] m_pcm_packet;
    m_pcm_packet = NULL;
  }
}

int CopusEncode::packet_set(unsigned char* pcm_bytes, uint32_t pcm_len) {
  if (pcm_len + m_cache_len > sizeof(m_pcm_cache)) return -1;
  memcpy(m_pcm_cache+m_cache_len, pcm_bytes, pcm_len);
  m_cache_len += pcm_len;
  return m_cache_len;
}

int CopusEncode::packet_get() {
  if (m_cache_len < m_packet_len) return -1;
  memcpy(m_pcm_packet, m_pcm_cache, m_packet_len);
  memmove(m_pcm_cache, m_pcm_cache+m_packet_len, m_cache_len-m_packet_len);
  m_cache_len -= m_packet_len;
 // debug("packet_get len:%d last len:%d", m_cache_len, cache_len);
  return m_cache_len;
}


int CopusEncode::EncodeOpus(unsigned char* pcm_bytes, uint32_t pcm_len) {
  if (!m_init) {
    if (0 != Init(0, 0)) {
      debug("not init");
      return -1;
    }
  }
  if (packet_set(pcm_bytes, pcm_len) < 0) {
    debug("buff is full cache_len:%d pcm_len:%d", m_cache_len, pcm_len);
  }
  unsigned char cbits[3 * 1276] = {0};
  unsigned char nbBytes = 0;
  while (packet_get() >= 0) {
    static opus_int16 in[8192];
    for (int i=0;i< m_channels * m_frame_size; i++) {
      in[i] = m_pcm_packet[2*i+1]<<8|m_pcm_packet[2*i];
    } 
    nbBytes = opus_encode(encode_hander, in, m_frame_size, cbits, MAX_PACKET_SIZE);
    if (nbBytes <= 0) {
      debug("encode failed: %s\n", opus_strerror(nbBytes));
      return -1;
    }
    Packet *opus_packet = new Packet((const char*)cbits, nbBytes);
    m_opus_queue.SetObject(opus_packet);
 
  }
  // debug("cache len:%d queue num:%d", cache_len, m_opus_queue.GetNum());
  return m_opus_queue.GetNum();
}
