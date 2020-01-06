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


void le32(unsigned char *p, int v)
{
  p[0] = v & 0xff;
  p[1] = (v >> 8) & 0xff;
  p[2] = (v >> 16) & 0xff;
  p[3] = (v >> 24) & 0xff;
}

/* helper, write a little-endian 16 bit int to memory */
void le16(unsigned char *p, int v)
{
  p[0] = v & 0xff;
  p[1] = (v >> 8) & 0xff;
}

ogg_packet *op_opushead(int samplerate, int channels)
{
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

ogg_packet *op_opustags(void)
{
  const char *identifier = "OpusTags";
  const char *vendor = "opus rtp packet dump";
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

CpcmCodec::CpcmCodec() {
  decoder = NeAACDecOpen();
  m_samplerate = 0;
  m_channels = 0;
  cache_len = 0;
  m_init = false;
  m_samplerate_encode = NULL;
}

CpcmCodec::~CpcmCodec() {
  if (decoder) {
    NeAACDecClose(decoder);
  }
  if (m_samplerate_encode) {
    src_delete(m_samplerate_encode);
  }
}

bool CpcmCodec::Init(unsigned char* data, size_t len) {
  if (m_init) return true;
  size_t size = FRAME_MAX_LEN;
  if(get_one_ADTS_frame(data, len, frame, &size) < 0) {
    LOG(ERROR) <<__func__<< " packet is not acc packet";
    debug("get_one_ADTS_frame failed");
    return false;
  }
  NeAACDecInit(decoder, frame, size, &m_samplerate, &m_channels);
  m_init = true;
  debug("init sucess channels:%d samplerate:%d", m_channels, m_samplerate);
  if (m_samplerate != SAMPLE_RATE) {
    int err = 0;
    m_samplerate_encode = src_new(SRC_LINEAR, m_channels, &err);
    if (!m_samplerate_encode) {
      debug("src_new faialed err:%d", err);
    }
  }
  return true;
}

int CpcmCodec::CDecDecode(unsigned char* data, size_t len) {
  if (!m_init) {
    if (!Init(data, len)) {
      debug("init failed");
      return -1;
    }
  }
  if (cache_len + len > FRAME_MAX_LEN * 2) {
    LOG(ERROR) <<__func__<< " buff is full";
    debug("buff is null cache len:%d input len:%d", cache_len, len);
    // return -9;
  } else {
    memcpy(buffer + cache_len, data, len);
    cache_len += len;
  }

  unsigned char* input_data = buffer;
  size_t data_size = cache_len;
  size_t size = FRAME_MAX_LEN;
  size_t decode_len = 0;

  while (get_one_ADTS_frame(input_data, data_size, frame, &size) == 0) {
    unsigned char *pcm_data = (unsigned char *)NeAACDecDecode(decoder, &frame_info, frame, size);
    if (frame_info.error == 0 && pcm_data && frame_info.samples > 0) {
      uint32_t pcm_len = frame_info.samples * frame_info.channels;
      Packet *packet = new Packet;
      // 重采样
      if (m_samplerate_encode) {
      //if (0) {
      static float in[8192] = {0};
      static float out[8192] = {0};
      for (uint32_t j = 0; j < pcm_len; j++) {
        in[j] = pcm_data[j];
      }

      static char sample_buff[16 * 1024] = {0};
      SRC_DATA src_data;
      src_data.src_ratio = 1.0 * SAMPLE_RATE / m_samplerate;
      src_data.end_of_input = 0;
      src_data.input_frames = frame_info.samples;
      src_data.output_frames = sizeof(out);
      src_data.data_in = in;
      src_data.data_out = out;
      int error = 0;
      do {
        if ((error = src_simple(&src_data, SRC_ZERO_ORDER_HOLD, frame_info.channels))) {
          debug("src_simple err:%s", src_strerror(error));
          break;
        }
        // debug("src_simple sucess inlen:%d out len:%d src_ratio:%2f", pcm_len, src_data.output_frames_gen * frame_info.channels, src_data.src_ratio);
        uint32_t pcm_sample_len = src_data.output_frames_gen * frame_info.channels;
        if(frame_info.channels == 2) {
          int i = 0;
           for (uint32_t j = 0; j < pcm_sample_len; j += 4) {
              sample_buff[i++] = out[j];
              sample_buff[i++] = out[j+1];
           }
           pcm_sample_len /= 2;

         } else {
           memcpy(sample_buff, out, pcm_sample_len);
         }
        // packet->len = src_data.output_frames_gen * frame_info.channels;
        packet->len = pcm_sample_len;
        packet->data = new char[packet->len];
        memcpy(packet->data, sample_buff, packet->len);
        m_pcm_queue.SetObject(packet);
        } while (0);
      }  else {
        packet->len = pcm_len;
        packet->data = new char[packet->len];
        memcpy(packet->data, pcm_data, packet->len);
        m_pcm_queue.SetObject(packet);
      }
    }
    data_size -= size;
    input_data += size;
    decode_len += size;
  }
  if (decode_len > 0) {
    size_t last_len = cache_len - decode_len;
    memmove(buffer, buffer + decode_len, last_len);
    cache_len = last_len;
  }
  // debug("cache len:%d queue num:%d", cache_len, m_pcm_queue.GetNum());
  return m_pcm_queue.GetNum();
}

int CpcmCodec::GetPcmPacket(Packet** packet) {
  m_pcm_queue.GetObject(packet);
  return (*packet) ? 0 : -1;
}

CopusEncode::CopusEncode() {
  m_init = false;
  m_ogg_init = false;
  m_ogg_file = fopen("test_ogg.opus", "wb");
  params = NULL;
}


int CopusEncode::Init(int sample_rates, int chan_num) {
  if (m_init) return 0;
  /// this->m_channels = chan_num ? chan_num : CHANNELS;
  this->m_channels = 1;
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
  if (m_ogg_file) {
    fclose(m_ogg_file);
    m_ogg_file = NULL;
  }
  if (params) {
    ogg_stream_destroy(params->stream);
    free(params);
    params = NULL;
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


int CopusEncode::EncodeAAc(unsigned char* pcm_bytes, uint32_t pcm_len) {
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
    Packet *opus_packet = new Packet;
    opus_packet->len = nbBytes;
    opus_packet->data = new char[opus_packet->len];
    memcpy(opus_packet->data, cbits, opus_packet->len);
    m_opus_queue.SetObject(opus_packet);
    // for ogg init
    do {
      if (m_ogg_init) break;
      params = (state*)malloc(sizeof(state));
      params->stream = (ogg_stream_state*)malloc(sizeof(ogg_stream_state));;
      params->out = m_ogg_file;
      params->granulepos = 0;
      if (ogg_stream_init(params->stream, rand()) < 0) {
        debug("ogg_stream_init init failed");
        break;
      }
      ogg_packet *op = op_opushead(m_sample_rates, m_channels);
      ogg_stream_packetin(params->stream, op);
      op_free(op);
      op = op_opustags();
      ogg_stream_packetin(params->stream, op);
      op_free(op);
      ogg_flush(params);
      m_ogg_init = true;
      debug("ogg init sucess");
    } while (0);
    // write packet
    if (m_ogg_init) {
      ogg_packet *op = op_from_pkt(cbits, nbBytes);
      int samples = opus_packet_get_nb_samples(cbits, nbBytes, 48000);
      if (samples > 0) params->granulepos += samples;
      op->granulepos = params->granulepos;
      ogg_stream_packetin(params->stream, op);
      free(op);
      ogg_write(params);
    }
  }
  // debug("cache len:%d queue num:%d", cache_len, m_opus_queue.GetNum());
  return m_opus_queue.GetNum();
}
