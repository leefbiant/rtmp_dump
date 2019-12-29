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
#include "rtmp_h264_aac.h"

void RtmpDump(const char* rtmp_url, const char* h264_file, const char* aac_file) {

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
    if (!fp_h264 || !fp_aac)     {
        debug("create file failed:%s!", strerror(errno));
        return ;
    }
    ADTSContext adts_ctx = {0};
    static char fix[] = {0x0, 0x0, 0x0, 0x1};
    RTMPPacket packet = {0};
    int failCount = 0;
    int aac_init = 0;
    int h264_init = 0;
    time_t last_stream_t = time(0);
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
                        int spsLen = (p[6] << 8) | p[7];
                        if (spsLen > 64)  {
                            debug("error spslen:%d", spsLen);
                            break;
                        }
                        char sps[64] = {0};
                        memcpy(sps, p + 8, spsLen);
                        int idx = 8 + spsLen;
                        int numPps = p[idx];
                        debug("profile %d levelid %d numSps %d numPps %d\n", profile, levelid, numSps, numPps);
                        idx++;
                        int ppslen = (p[idx] << 8) | p[idx + 1];
                        idx += 2;
                        char pps[64] = {0};
                        if (ppslen > 64)  {
                            debug("error ppslen:%d", ppslen);
                            break;
                        }
                        memcpy(pps, p + idx, ppslen);
                        fwrite(fix, sizeof(fix), 1, fp_h264);
                        fwrite(sps, spsLen, 1, fp_h264);
                        fwrite(fix, sizeof(fix), 1, fp_h264);
                        fwrite(pps, ppslen, 1, fp_h264);
                        fflush(fp_h264);
                        h264_init = 1;
                        debug("add sps pps");
                    } while (0);
                }
            } else if (h264_init &&  data[1] == 1)  {
                fwrite(fix, sizeof(fix), 1, fp_h264);
                fwrite(data + 9, size - 9, 1, fp_h264);
                fflush(fp_h264);
            }
        }
        RTMPPacket_Free(&packet);
        if (time(0) - last_stream_t > 30) {
            debug("inot recv stream exit....");
            break;
        }
    }

    if (rtmp)  {
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
        rtmp=NULL;
    }
    if (fp_aac) {
        fclose(fp_aac);   
        fp_aac = NULL;
    }
    if (fp_h264) {
        fclose(fp_h264);   
        fp_h264 = NULL;
    }
 
    debug("sucess");
    return ;
}
