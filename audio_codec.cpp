/***********************************************
Copyright           : 2015 Leef Inc.
Filename            : opus_encode.cpp
Author              : bt731001@163.com
Description         : ---
Create              : 2019-07-12 23:09:12
Last Modified       : 2019-07-12 23:09:12
***********************************************/
#include "audio_codec.h"
#include <math.h>



AAcDecoder::AAcDecoder() {
  decoder = NeAACDecOpen();
  m_samplerate = 0;
  m_channels = 0;
  m_init = false;
}

AAcDecoder::~AAcDecoder() {
  if (decoder) {
    NeAACDecClose(decoder);
  }
  Packet* packet;
  while (0 == m_aac_queue.GetObject(&packet) && packet) {
    delete packet;
  }
  while (0 == m_pcm_queue.GetObject(&packet) && packet) {
    delete packet;
  }
}

bool AAcDecoder::Init(unsigned char* data, size_t len) {
  if (m_init) return true;
  NeAACDecInit(decoder, data, len, &m_samplerate, &m_channels);
  m_init = true;
  debug("init sucess channels:%u samplerate:%lu", m_channels, m_samplerate);
  return true;
}


int AAcDecoder::CDecDecode(Packet* packet) {
  if (!m_init) {
    if (!Init((unsigned char*)packet->data, packet->len)) {
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
      // debug("error packet");
      continue;
    }
    uint32_t pcm_len = frame_info.samples * frame_info.channels;
  
    PcmPacket* packet = new PcmPacket((const char *)pcm_data, pcm_len);
    packet->samples = frame_info.samples;
    packet->channels = frame_info.channels;
    m_pcm_queue.SetObject(packet);
  }
  return m_pcm_queue.GetNum();
}


int AAcDecoder::GetPcmPacket(Packet** packet) {
  m_pcm_queue.GetObject(packet);
  return (*packet) ? 0 : -1;
}

CopusEncode::CopusEncode() :m_sample_rates(SAMPLE_RATE), m_channels(1) {
  m_init = false;
  m_packet_len = 0;
  m_cache_len = 0;
  m_pcm_packet = NULL;
}


int CopusEncode::Init(int sample_rates, int chan_num) {
  if (m_init) return 0;
  this->m_input_channels = chan_num;
  this->m_input_sample_rates = sample_rates;
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
  m_packet_len = sizeof(short) * m_channels * m_frame_size;
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
  Packet *packet;
  while (0 == m_opus_queue.GetObject(&packet) && packet) {
    delete packet;
  }
}

int CopusEncode::packet_set(unsigned char* pcm_bytes, uint32_t pcm_len) {
  if (pcm_len + m_cache_len > sizeof(m_pcm_cache)) return -1;
  memcpy(m_pcm_cache+m_cache_len, pcm_bytes, pcm_len);
  m_cache_len += pcm_len;
  return m_cache_len;
}

int CopusEncode::packet_get() {
  if (m_cache_len <= 0 || m_packet_len <= 0 || m_cache_len < m_packet_len) return -1;
  memcpy(m_pcm_packet, m_pcm_cache, m_packet_len);
  if (m_cache_len > m_packet_len) {
    memmove(m_pcm_cache, m_pcm_cache+m_packet_len, m_cache_len-m_packet_len);
  }
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
  unsigned char* in_data = pcm_bytes;
  uint32_t in_len = pcm_len;
  //  重采样
  if (m_channels != m_input_channels || m_sample_rates != m_input_sample_rates) {
    static float in[4096] = {0};
    static float out[4096] = {0};
    src_short_to_float_array((short *)in_data, in, in_len / sizeof(short));
    static unsigned char sample_buff[16 * 1024] = {0};
    SRC_DATA src_data;
    memset(&src_data, 0x00, sizeof(SRC_DATA));
    src_data.src_ratio = 1.0 * m_sample_rates / m_input_sample_rates;
    src_data.end_of_input = 0;
    src_data.input_frames = in_len / m_input_channels;
    src_data.output_frames = sizeof(out);
    src_data.data_in = in;
    src_data.data_out = out;
    int error = 0;
    if ((error = src_simple(&src_data, SRC_ZERO_ORDER_HOLD, m_input_channels))) {
      debug("src_simple err frae_info.channels:%d : %s", m_input_channels, src_strerror(error));
      return -1;
    }
    uint32_t pcm_sample_len = src_data.output_frames_gen * m_input_channels;
    src_float_to_short_array(out, (short *)sample_buff, pcm_sample_len);
    if (m_input_channels == 2 && m_channels == 1) {
      short *audio_buff = (short *)sample_buff;
      for (uint32_t j = 0; j < pcm_sample_len / sizeof(short); ++j) {
        audio_buff[j] = (audio_buff[2 * j] + audio_buff[2 * j + 1]) >> 1;
      }
      pcm_sample_len /= 2;
    }
    in_data = sample_buff;
    in_len = pcm_sample_len;
  }

  if (packet_set(in_data, in_len) < 0) {
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
  return m_opus_queue.GetNum();
}

int CopusEncode::EncodeOpus(PcmPacket* pcm_packet) {
  if (!m_init) {
    if (0 != Init(0, 0)) {
      debug("not init");
      return -1;
    }
  }
  unsigned char* in_data = (unsigned char*)pcm_packet->data;
  uint32_t in_len = pcm_packet->len;
  //  重采样
  if (m_channels != m_input_channels || m_sample_rates != m_input_sample_rates) {
    static float in[8192] = {0};
    static float out[8192] = {0};
    src_short_to_float_array((short *)in_data, in, in_len / sizeof(short));
    static unsigned char sample_buff[16 * 1024] = {0};
    SRC_DATA src_data;
    memset(&src_data, 0x00, sizeof(SRC_DATA));
    src_data.src_ratio = 1.0 * m_sample_rates / m_input_sample_rates;
    src_data.end_of_input = 0;
    src_data.input_frames = pcm_packet->samples;
    src_data.output_frames = sizeof(out);
    src_data.data_in = in;
    src_data.data_out = out;
    int error = 0;
    if ((error = src_simple(&src_data, SRC_ZERO_ORDER_HOLD, pcm_packet->channels))) {
      debug("src_simple err frae_info.channels:%d : %s", m_input_channels, src_strerror(error));
      return -1;
    }
    uint32_t pcm_sample_len = src_data.output_frames_gen * pcm_packet->channels;
    src_float_to_short_array(out, (short *)sample_buff, pcm_sample_len);
    if (pcm_packet->channels == 2 && m_channels == 1) {
      short *audio_buff = (short *)sample_buff;
      for (uint32_t j = 0; j < pcm_sample_len / sizeof(short); ++j) {
        audio_buff[j] = (audio_buff[2 * j] + audio_buff[2 * j + 1]) >> 1;
      }
      pcm_sample_len /= 2;
    }
    in_data = sample_buff;
    in_len = pcm_sample_len;
  }

  if (packet_set(in_data, in_len) < 0) {
    debug("buff is full cache_len:%d pcm_len:%d", m_cache_len, pcm_packet->len);
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
  debug("cache len:%d queue num:%d", m_cache_len, m_opus_queue.GetNum());
  return m_opus_queue.GetNum();
}

CopusDecode::CopusDecode():m_sample_rates(16000), m_channels(1) {
  m_init = false;
  decode_hander = NULL;
}

CopusDecode::~CopusDecode() {
  if (decode_hander) {
    opus_decoder_destroy(decode_hander);
    decode_hander = NULL;
  }
  Packet* packet = NULL;
  while (0 == m_pcm_queue.GetObject(&packet) && packet) {
    delete packet;
  }
}

int CopusDecode::Init(int sample_rates, int chan_num) {
  m_sample_rates = sample_rates > 0 ? sample_rates : m_sample_rates;
  m_channels = chan_num > 0 ? chan_num : m_channels;
  int err;
  decode_hander = opus_decoder_create(m_sample_rates, m_channels, &err);
  if (err < 0) {
    debug("opus_decoder_create failed : %s", opus_strerror(err));
    return -1;
  }
  debug("CopusDecode::Init m_sample_rates:%d m_channels:%d", m_sample_rates, m_channels);
  m_init = true;
  return 0;
}

int CopusDecode::DecodeOpus(unsigned char* opus_bytes, uint32_t opus_len) {
  static opus_int16 out[MAX_FRAME_SIZE*2];
  do {
    int frame_size = opus_decode(decode_hander, opus_bytes, opus_len, out, MAX_FRAME_SIZE*2, 0);
    if (frame_size < 0) {
        // debug("decoder failed:  %s", opus_strerror(frame_size));
        break;
    }
    PcmPacket* packet = new PcmPacket(m_channels * frame_size * 2);
    for(int i=0; i < m_channels * frame_size; i++)   {
        packet->data[2*i] = out[i]&0xFF;
        packet->data[2*i+1] = (out[i]>>8)&0xFF;
    }
    packet->channels = m_channels;
    packet->samples = m_sample_rates;
    m_pcm_queue.SetObject(packet);
  } while (0);
  return m_pcm_queue.GetNum();
}

CaacEncode::CaacEncode(): m_sample_rates(44100), m_channels(1),m_pcm_bit_size(16) {
  m_cache = NULL;
  encode_hander = NULL;
  m_cache = NULL;
  m_out = NULL;
  m_pcm_buff_size = 0;
  m_pcm_cache_len = 0;
}
CaacEncode::~CaacEncode() {
  if (m_cache) {
    delete m_cache;
    m_cache = NULL;
  }
  if (encode_hander) {
    faacEncClose(encode_hander);
    encode_hander = NULL;
  }
  Packet* packet = NULL;
  while (0 == m_aac_queue.GetObject(&packet) && packet) {
    delete packet;
  }
  if (m_cache) {
    delete m_cache;
    m_cache = NULL;
  }
  if (m_out) {
    delete m_out;
    m_out = NULL;
  }
}
int CaacEncode::EncodeConfig(int sample_rates, int chan_num) {
  m_sample_rates = sample_rates;
  m_channels = chan_num;
  return 0;
}

int CaacEncode::Init(int sample_rates, int chan_num) {
  m_input_sample_rates = sample_rates;
  m_input_channels = chan_num;
  long maxOutputBytes = 0;
  debug("CaacEncode::Init m_input_sample_rates:%d m_input_channels:%d", m_input_sample_rates, m_input_channels);
  encode_hander = faacEncOpen(m_sample_rates, m_channels, (long unsigned int*)&m_samplesInput, (long unsigned int*)&maxOutputBytes);
  if (!encode_hander) {
    debug("faacEncOpen failed");
    return -1;
  }
  debug("m_sample_rates:%d m_channels:%d samplesInput:%d maxOutputBytes:%d",m_sample_rates, m_channels, m_samplesInput, maxOutputBytes);
  faacEncConfigurationPtr config = faacEncGetCurrentConfiguration(encode_hander);
  debug("default config: type:%d outputFormat:%d bitRate:%d", config->aacObjectType, config->outputFormat, config->inputFormat);
  config->inputFormat = FAAC_INPUT_16BIT;
  faacEncSetConfiguration(encode_hander, config); 
  m_pcm_buff_size = m_samplesInput * m_pcm_bit_size / 8;
  m_cache = new Packet(16 * m_pcm_buff_size);
  m_out = new Packet(maxOutputBytes);
  m_pcm_cache_len = 0;
  m_init = true;
  return 0;
}

int CaacEncode::AacEncode(unsigned char* pcm_bytes, uint32_t pcm_len) {
  if (!m_init) {
    debug("not init");
    return -1;
  }
  unsigned char* in_data = (unsigned char*)pcm_bytes;
  uint32_t in_len = pcm_len;
  if (m_channels != m_input_channels || m_sample_rates != m_input_sample_rates) {
    static float in[4096] = {0};
    static float out[4096] = {0};
    src_short_to_float_array((short *)in_data, in, in_len / sizeof(short));
    static unsigned char sample_buff[16 * 1024] = {0};
    SRC_DATA src_data;
    memset(&src_data, 0x00, sizeof(SRC_DATA));
    src_data.src_ratio = 1.0 * m_sample_rates / m_input_sample_rates;
    src_data.end_of_input = 0;
    src_data.input_frames = in_len / m_input_channels;
    src_data.output_frames = sizeof(out);
    src_data.data_in = in;
    src_data.data_out = out;
    int error = 0;
    if ((error = src_simple(&src_data, SRC_ZERO_ORDER_HOLD, m_input_channels))) {
      debug("src_simple err frae_info.channels:%d : %s", m_input_channels, src_strerror(error));
      return -1;
    }
    uint32_t pcm_sample_len = src_data.output_frames_gen * m_input_channels;
    src_float_to_short_array(out, (short *)sample_buff, pcm_sample_len);
    if (m_input_channels == 2 && m_channels == 1) {
      // 双声道转单声道
      short *audio_buff = (short *)sample_buff;
      for (uint32_t j = 0; j < pcm_sample_len / sizeof(short); ++j) {
        audio_buff[j] = (audio_buff[2 * j] + audio_buff[2 * j + 1]) >> 1;
      }
      pcm_sample_len /= 2;
    }
    in_data = sample_buff;
    in_len = pcm_sample_len;
  }
  if (in_len + m_pcm_cache_len > m_cache->len) {
    debug("buff is full cache buff len:%d cache len:%d in_len:%d m_pcm_buff_size:%d", m_cache->len, m_pcm_cache_len, in_len, m_pcm_buff_size);
  } else {
    memcpy(m_cache->data + m_pcm_cache_len, in_data, in_len);
    m_pcm_cache_len += in_len;
  }
  uint32_t offset = 0;
  const char* pdata = m_cache->data;
  while (m_pcm_cache_len - offset> m_pcm_buff_size) {
    unsigned long samplesInput = m_samplesInput;
    int ret = faacEncEncode(encode_hander, (int *)(pdata + offset), samplesInput, (unsigned char*)m_out->data, m_out->len);
    if (ret > 0) {
      Packet* packet = new Packet(m_out->data, ret);
      m_aac_queue.SetObject(packet);
    } else if (ret < 0){
      debug("faacEncEncode failed samplesInput:%d m_pcm_cache_len:%d", samplesInput, m_pcm_cache_len);
      return -1;
    } else {
      // debug("buff is not enough a frame");
      break;
    }
    offset += m_pcm_buff_size;
  }
  if (offset > 0) {
     m_pcm_cache_len -= offset;
    memmove(m_cache->data, pdata + offset, m_pcm_cache_len);
  }
  return m_aac_queue.GetNum();
}