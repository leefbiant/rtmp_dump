#include "string.h"
#include "mp4_encode.h"
#include "mediaparse.h"

CMp4Encode::CMp4Encode(void):
    m_bfindSps(false),
    m_bfindPps(false),
    m_nFrameRate(0),
    m_nTimeScale(0),
    m_tick(0),
    m_videoId(0),
    m_audioId(0) {

    hMp4File = NULL ;
}

CMp4Encode::~CMp4Encode(void) {
    if(hMp4File) {
        MP4Close(hMp4File, 0);
        hMp4File = NULL;
    }
}

int CMp4Encode::FileCreate(const char *pFileName,int width,int height,int timescale, float frameRate, int tick) {
    if(pFileName == NULL) {
        return -1;
    }

    // create mp4 file
    hMp4File = MP4Create(pFileName, 0);
    // hMp4File = MP4CreateEx(pFileName, 1, 1, 0, 0, 0, 0);
    if (hMp4File == MP4_INVALID_FILE_HANDLE)  {
        printf("ERROR:Open file fialed.\n");
        return -1;
    }

    m_nWidth = width;
    m_nHeight = height;
    m_nTimeScale = timescale;
    m_nFrameRate = frameRate;
    m_tick = tick;
    MP4SetTimeScale(hMp4File, m_nTimeScale);

    return 0;
}

int CMp4Encode::FileWrite(int frameType, unsigned char *pframeBuf, int frameLen) {
    if(!hMp4File || !pframeBuf) {
        return -1;
    }

    switch(frameType) {
        case MEDIA_FRAME_AUDIO:  {
            WriteAudioTrack(pframeBuf, frameLen);
            break;
        }
        case MEDIA_FRAME_VIDEO: {
            WriteVideoTrack(pframeBuf, frameLen);
            break;
        }
        default:
            break;
    }

    return 0;
}

int CMp4Encode::FileClose() {
    if(hMp4File)  {
        MP4Close(hMp4File, 0);
        hMp4File = NULL;
    }
    return 0;
}

int CMp4Encode::WriteAudioTrack(unsigned char *pframeBuf,int frameLen) {
    if (!m_audioId) {
        ADTSContext adts_ctx = {0};
        parser_adts_content(&adts_ctx, pframeBuf, 7);
        debug("parse adts profileID:%d sampleRateIdx:%d chanNum:%d", adts_ctx.profileID, adts_ctx.sampleRateIdx, adts_ctx.chanNum);
        int audioSample = GetSamples(adts_ctx.sampleRateIdx) * adts_ctx.chanNum;
        m_audioId = MP4AddAudioTrack(hMp4File, audioSample, 2048, MP4_MPEG4_AUDIO_TYPE);
        MP4SetAudioProfileLevel(hMp4File, 0x2);
    }
    if (!m_audioId) return -1;
    if(!MP4WriteSample(hMp4File, m_audioId, pframeBuf, frameLen, MP4_INVALID_DURATION, 0, 1)) {
        debug("MP4WriteSample audio failed");
        return -1;
    }
    return 0;
}

int CMp4Encode::WriteVideoTrack(unsigned char *pframeBuf,int frameLen) {
    int pos = 0, len = 0;
    MP4ENC_NaluUnit nalu;
    while ((len = ReadOneNaluFromBuf(pframeBuf, frameLen, pos, nalu)) > 0)  {
        // printf("nnalu.frameType:0x%02x nalu.frameLen:%d len:%d\n", nalu.frameType, nalu.frameLen, len);
        if(nalu.frameType == 0x07)  {// sps 
            if (m_bfindSps)
                goto continue_pos;

            m_videoId = MP4AddH264VideoTrack
                (hMp4File,
                m_nTimeScale,
                // m_nTimeScale / m_nFrameRate,
                m_tick * 2,
                m_nWidth,     // width
                m_nHeight,    // height
                nalu.pframeBuf[1], // sps[1] AVCProfileIndication
                nalu.pframeBuf[2], // sps[2] profile_compat
                nalu.pframeBuf[3], // sps[3] AVCLevelIndication
                3);           // 4 bytes length before each NAL unit
            if (m_videoId == MP4_INVALID_TRACK_ID)  {
                printf("add video track failed.\n");
                return 0;
            }
            MP4SetVideoProfileLevel(hMp4File, 0x7f); //  Simple Profile @ Level 3
            MP4AddH264SequenceParameterSet(hMp4File, m_videoId, nalu.pframeBuf, nalu.frameLen);

            m_bfindSps = true;
        }
        else if(nalu.frameType == 0x08) {// pps
            if (m_bfindPps)
                goto continue_pos;

            MP4AddH264PictureParameterSet(hMp4File, m_videoId,nalu.pframeBuf, nalu.frameLen);
            m_bfindPps = true;
        } else {
            if ((m_videoId == MP4_INVALID_TRACK_ID) || !m_bfindSps || !m_bfindPps) {
                goto continue_pos;
            }

            int datalen = nalu.frameLen+4;
            unsigned char *data = new unsigned char[datalen];
            data[0] = nalu.frameLen>>24;
            data[1] = nalu.frameLen>>16;
            data[2] = nalu.frameLen>>8;
            data[3] = nalu.frameLen&0xff;
            memcpy(data+4, nalu.pframeBuf, nalu.frameLen);
            if(!MP4WriteSample(hMp4File, m_videoId, data, datalen, MP4_INVALID_DURATION, 0, 1)) {
                delete []data;
                return 0;
            }
            delete []data;
        }
continue_pos:
        pos += len;
    }

    return pos;
}

int CMp4Encode::ReadOneNaluFromBuf(const unsigned char *buffer, unsigned int nBufferSize, unsigned int offSet, MP4ENC_NaluUnit &nalu) {
    unsigned int i = offSet;
    while(i < nBufferSize) {
        if(buffer[i++] == 0x00 && buffer[i++] == 0x00 && buffer[i++] == 0x00 && buffer[i++] == 0x01) {
            unsigned int pos = i;
            while (pos < nBufferSize) {
                if(buffer[pos++] == 0x00 && buffer[pos++] == 0x00 && buffer[pos++] == 0x00 && buffer[pos++] == 0x01) {
                    break;
                }
            }
            if(pos == nBufferSize)  {
                nalu.frameLen = pos-i;
            } else {
                nalu.frameLen = (pos - 4) - i;
            }
            nalu.frameType = buffer[i]&0x1f;
            nalu.pframeBuf = (unsigned char*)&buffer[i];
            return (nalu.frameLen+i-offSet);
        }
    }

    return 0;
}