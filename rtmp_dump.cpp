/***********************************************
Copyright           : 2015 leef.biant Inc.
Filename            : rtmp_h264_aac.cc
Author              : bt731001@163.com
Description         : ---
Create              : 2019-12-28 22:59:02
Last Modified       : 2019-12-28 22:59:02
***********************************************/


#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "librtmp/rtmp.h"
#include "librtmp/log.h"
#include "util.h"
#include "rtmp_dump.h"
#include "sps_pps.h"
#include "mp4_encode.h"
#include "mp4_encode.h"

#include "converter.h"

using namespace std;
using namespace Cnvt;


void RtmpDump(const char* rtmp_url, const char* h264_file, const char* aac_file, const char* opus_file) {

    int readBytes = 0;
    
    RTMP *rtmp = RTMP_Alloc();
    RTMP_Init(rtmp);
    rtmp->Link.timeout=10;
    
    char rtmp_url_str[512] = {0};
    strncpy(rtmp_url_str, rtmp_url, 511);
    if(!RTMP_SetupURL(rtmp, rtmp_url_str))
    {
        RTMP_Log(RTMP_LOGERROR,"SetupURL Err\n");
        RTMP_Free(rtmp);
        return ;
    }

    rtmp->Link.lFlags |= RTMP_LF_LIVE;

    RTMP_SetBufferMS(rtmp, 3600*1000);
    
    if(!RTMP_Connect(rtmp,NULL)){
        RTMP_Log(RTMP_LOGERROR,"Connect Err\n");
        RTMP_Free(rtmp);
        return ;
    }
    
    if(!RTMP_ConnectStream(rtmp,0)){
        RTMP_Log(RTMP_LOGERROR,"ConnectStream Err\n");
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
        return ;
    }

    FILE *fp_h264 = fopen(h264_file, "wb");
    FILE *fp_aac = fopen(aac_file, "wb");
    FILE* fp_opus = fopen(opus_file, "wb");
    if (!fp_h264 || !fp_aac || !fp_opus)     {
        debug("create file failed:%s!", strerror(errno));
        return ;
    }
    ADTSContext adts_ctx = {0};
    static char fix[] = {0x0, 0x0, 0x0, 0x1};
    char sps[64] = {0};
    char pps[64] = {0};
    int spsLen = 0;
    int ppslen = 0;

    RTMPPacket packet = {0};
    int failCount = 0;
    int aac_init = 0;
    int h264_init = 0;
    time_t last_stream_t = time(0);

    AAcDecoder aac_decoder_handle;
    CopusEncode opus_encoder_handle;
    size_t frame_size = 0;
    bool aac_decode_init = false;
    state *params = NULL;

    // pcm
    FILE* fp_pcm = fopen("test.pcm", "wb");

    // mp4
    CMp4Encode mp4_encode;
    static unsigned char frame_buff[128 * 1024];
    bool mp_init = false;
    float framerate = 0;
    get_bit_context buffer;
    SPS h264_sps_context;

    // flv
    CConverter flv_stream;
    uint32_t flv_audio_timestamp = 0;
    uint32_t flv_video_timestamp = 0;
    flv_stream.Open("test.flv", 1, 1);
    bool flv_init = false;

    // opus decoder
    CopusDecode opus_decoder_handle;
    // aac encoder
    CaacEncode aac_encoder_handle;
    FILE* fp_aac2 = fopen("test2.aac", "wb");

    while (RTMP_IsConnected(rtmp)) {
        readBytes = RTMP_ReadPacket(rtmp, &packet);
        if (packet.m_body == NULL ||
            packet.m_nBodySize == 0 ||
            readBytes <= 0 ) {
            usleep(50);
            if(failCount++ < 20 * 5) {
                continue;
            }  else {
                debug("not recv stream exit....");
                break;
            }
        }
        failCount = 0;
        // RTMP_LogPrintf("Receive Packet: %5d Packet, Total: %5.2fkB\n",readBytes, packet.m_nBodySize*1.0/1024);

        const char *data = packet.m_body;
        int size = packet.m_nBodySize;

        if (packet.m_packetType == RTMP_PACKET_TYPE_AUDIO) {
            last_stream_t = time(0);
            if (size >= 2 && data[1] == 0x00)  {
                parse(&adts_ctx, data + 2);
                aac_init = 1;
            }
            if (aac_init && size >= 2 && data[1] == 0x01)  {
                unsigned char adts[7] = {0};
                aac_decode_extradata(&adts_ctx, adts, 7, size - 2);
                fwrite(adts, 7, 1, fp_aac);
                fwrite(data + 2, size - 2, 1, fp_aac);
                fflush(fp_aac);

                // for opus
                frame_size = 7 + size - 2;
                Packet* comm_packet = new Packet(frame_size);
                memcpy(comm_packet->data, adts, 7);
                memcpy(comm_packet->data + 7, data + 2, comm_packet->len - 7);

                // for flv
                flv_stream.ConvertAAC(comm_packet->data, comm_packet->len, flv_audio_timestamp);
                // flv_audio_timestamp += double(1024 * 1000) / double(GetSamples(adts_ctx.sampleRateIdx));

                // for mp4
                if (mp_init) {
                    mp4_encode.FileWrite(MEDIA_FRAME_AUDIO, (unsigned char*)comm_packet->data, frame_size);
                }
                
                if (!aac_decode_init) {
                    if (aac_decoder_handle.Init((unsigned char*)comm_packet->data, comm_packet->len)) {
                        opus_encoder_handle.Init(aac_decoder_handle.m_samplerate, aac_decoder_handle.m_channels);
                        aac_decode_init = true;
                    }
                }
                if (!aac_decode_init) {
                    delete comm_packet;
                    continue;
                }
                int ret = aac_decoder_handle.CDecDecode(comm_packet);
                if (ret < 0) {
                    debug("pcm codec failed");
                    continue;
                }
                comm_packet = NULL;
                while (0 == aac_decoder_handle.m_pcm_queue.GetObject(&comm_packet) && comm_packet) {
                    // write pcm
                    fwrite(comm_packet->data, 1, comm_packet->len, fp_pcm);
                    fflush(fp_pcm);

                    ret = opus_encoder_handle.EncodeOpus((unsigned char*)comm_packet->data, comm_packet->len);
                    delete comm_packet;
                    comm_packet = NULL;
                    if (ret < 0) {
                        debug("aac encode failed");
                        continue; 
                    }
                    while (0 == opus_encoder_handle.m_opus_queue.GetObject(&comm_packet) && comm_packet) {
                        // for opus decode
                        do {
                            if (!opus_decoder_handle.m_init) {
                                if (opus_decoder_handle.Init(opus_encoder_handle.m_sample_rates, opus_encoder_handle.m_channels) < 0) {
                                    debug("opus_decoder_handle init failed");
                                    break;
                                }
                            } 
                            int ret = opus_decoder_handle.DecodeOpus((unsigned char*)comm_packet->data, comm_packet->len);
                            if (ret <= 0) break;
                            Packet* pcm_packet = NULL;
                            while (0 == opus_decoder_handle.m_pcm_queue.GetObject(&pcm_packet) && pcm_packet) {
                                // for aac encode
                                do {
                                    if (!aac_encoder_handle.m_init) {
                                        if (aac_encoder_handle.Init(opus_decoder_handle.m_sample_rates, opus_decoder_handle.m_channels) < 0) {
                                            debug("aac_encoder_handle init failed");
                                            break;
                                        }
                                    }
                                    int ret = aac_encoder_handle.AacEncode((unsigned char*)pcm_packet->data, pcm_packet->len);
                                   if (ret <= 0) break;
                                    Packet* aac_packet = NULL;
                                    while (0 == aac_encoder_handle.m_aac_queue.GetObject(&aac_packet) && aac_packet) {
                                        fwrite(aac_packet->data, 1, aac_packet->len, fp_aac2);
                                        fflush(fp_aac2);
                                        delete aac_packet;
                                    }
                                } while (0);
                                delete pcm_packet;
                            }
                        } while (0);

                        // for ogg 
                        do {
                            if (params) break;
                            params = (state*)malloc(sizeof(state));
                            params->stream = (ogg_stream_state*)malloc(sizeof(ogg_stream_state));;
                            params->out = fp_opus;
                            params->granulepos = 0;
                            if (ogg_stream_init(params->stream, rand()) < 0) {
                                debug("ogg_stream_init init failed");
                                if (params) {
                                    ogg_stream_destroy(params->stream);
                                    free(params);
                                    params = NULL;
                                }
                                break;
                            }
                            ogg_packet *op = op_opushead(opus_encoder_handle.m_sample_rates, opus_encoder_handle.m_channels);
                            ogg_stream_packetin(params->stream, op);
                            op_free(op);
                            op = op_opustags();
                            ogg_stream_packetin(params->stream, op);
                            op_free(op);
                            ogg_flush(params);
                            debug("ogg init sucess sample_rates:%d channels:%d", opus_encoder_handle.m_sample_rates, opus_encoder_handle.m_channels);
                        } while (0);
                        // write packet
                        if (params) {
                            ogg_packet *op = op_from_pkt((const unsigned char*)comm_packet->data, comm_packet->len);
                            int samples = opus_packet_get_nb_samples((const unsigned char*)comm_packet->data, comm_packet->len, opus_encoder_handle.m_sample_rates);
                            if (samples > 0) params->granulepos += samples;
                            op->granulepos = params->granulepos;
                            ogg_stream_packetin(params->stream, op);
                            free(op);
                            ogg_write(params);
                        }
                        delete comm_packet;
                        comm_packet = NULL; 
                    }
                }
            }
        }  else if (packet.m_packetType == RTMP_PACKET_TYPE_VIDEO)  {
            last_stream_t = time(0);
            if ((data[1] & 0xff) == 0)  {
                if ((data[0] & 0xf) == 7)  {
                    do {
                        const char *p = data + 5;
                        int profile = p[0];
                        int levelid = p[3];
                        int numSps = p[5] & 0x1f;
                        spsLen = (p[6] << 8) | p[7];
                        if (spsLen > 64)  {
                            debug("error spslen:%d", spsLen);
                            break;
                        }
                        memcpy(sps, p + 8, spsLen);
                        int idx = 8 + spsLen;
                        int numPps = p[idx];
                        debug("profile %d levelid %d numSps %d numPps %d", profile, levelid, numSps, numPps);
                        idx++;
                        ppslen = (p[idx] << 8) | p[idx + 1];
                        idx += 2;
                        if (ppslen > 64)  {
                            debug("error ppslen:%d", ppslen);
                            break;
                        }
                        memcpy(pps, p + idx, ppslen);
                        h264_init = 1;
                        debug("add sps pps");

                        memset(&buffer, 0, sizeof(get_bit_context));
                        buffer.buf = (uint8_t *)sps + 1;
                        buffer.buf_size = spsLen - 1;
                        h264dec_seq_parameter_set(&buffer, &h264_sps_context);
                        h264_get_framerate(&framerate, &h264_sps_context);

                        // for flv
                        if (!flv_init) {
                            debug("framerate:%.2f titck:%.2f", framerate, h264_sps_context.vui_parameters.time_scale / (2 * h264_sps_context.vui_parameters.num_units_in_tick));
                            memcpy(frame_buff, fix, sizeof(fix));
                            memcpy(frame_buff+sizeof(fix), sps, spsLen);
                            flv_stream.ConvertH264((char*)frame_buff, spsLen + sizeof(fix), flv_video_timestamp);

                            memcpy(frame_buff+sizeof(fix), pps, ppslen);
                            flv_stream.ConvertH264((char*)frame_buff, ppslen + sizeof(fix), flv_video_timestamp);
                            flv_init = true;
                        }
                    } while (0);

                }
            } else if (h264_init &&  data[1] == 1)  {
                if (((data[0] >> 4) & 0x0f) == 1) {
                    fwrite(fix, sizeof(fix), 1, fp_h264);
                    fwrite(sps, spsLen, 1, fp_h264);
                    fwrite(fix, sizeof(fix), 1, fp_h264);
                    fwrite(pps, ppslen, 1, fp_h264);
                }
                fwrite(fix, sizeof(fix), 1, fp_h264);
                fwrite(data + 9, size - 9, 1, fp_h264);
                fflush(fp_h264);

                if (flv_init) {
                    memcpy(frame_buff, fix, sizeof(fix));
                    memcpy(frame_buff + sizeof(fix), data + 9, size - 9);
                    flv_stream.ConvertH264((char *)frame_buff, size - 9 + sizeof(fix), flv_video_timestamp);
                    // flv_video_timestamp += 1000 / framerate;
                    flv_video_timestamp += 29;
                    // debug("flv_video_timestamp:%u", flv_video_timestamp);
                }
                do {
                    if (mp_init)  break;
                    // for mp4
                    int width = (h264_sps_context.pic_width_in_mbs_minus1 + 1) * 16;
                    int height = (h264_sps_context.pic_height_in_map_units_minus1 + 1) * 16 * (2 - h264_sps_context.frame_mbs_only_flag);
                    int timescale = h264_sps_context.vui_parameters.time_scale;
                    int tick = h264_sps_context.vui_parameters.num_units_in_tick;
                    
                    debug("timescale:%d tick:%d framerate:%.1f", timescale, tick, framerate);
                    int ret = mp4_encode.FileCreate("test.mp4",
                                                width,
                                                height,
                                                timescale,
                                                framerate, 
                                                tick);
                    if (0 != ret) {
                        debug("create mp4 file failed");
                        break;
                    }
                    memcpy(frame_buff, fix, sizeof(fix));
                    memcpy(frame_buff + sizeof(fix), sps, spsLen);
                    mp4_encode.FileWrite(MEDIA_FRAME_VIDEO, frame_buff, spsLen + sizeof(fix));

                    memcpy(frame_buff, fix, sizeof(fix));
                    memcpy(frame_buff + sizeof(fix), pps, ppslen);
                    mp4_encode.FileWrite(MEDIA_FRAME_VIDEO, frame_buff, ppslen + sizeof(fix));
                    mp_init = true;
                }  while (0);

                if (mp_init) {
                    memcpy(frame_buff, fix, sizeof(fix));
                    memcpy(frame_buff + sizeof(fix), data + 9, size - 9);
                    mp4_encode.FileWrite(MEDIA_FRAME_VIDEO, frame_buff, size - 9 + sizeof(fix));
                }
            }
        }
        RTMPPacket_Free(&packet);
        if (time(0) - last_stream_t > 5) {
            debug("timeout recv stream exit....");
            break;
        }
    }

    mp4_encode.FileClose();

    if (rtmp)  {
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
    }
    if (fp_aac) {
        fclose(fp_aac);   
        fp_aac = NULL;
    }
    if (fp_h264) {
        fclose(fp_h264);   
        fp_h264 = NULL;
    }
    if (params) {
        ogg_stream_destroy(params->stream);
        free(params);
        params = NULL;
    }

    if (fp_aac2) {
        fclose(fp_aac2);
        fp_aac2 = NULL;
    }

    debug("sucess");
    return ;
}
