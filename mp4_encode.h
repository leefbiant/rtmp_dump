#ifndef CMP4ENCODE_H
#define CMP4ENCODE_H

#include "mp4v2/mp4v2.h"


typedef enum MEDIA_FRAME_TYPE {
    MEDIA_FRAME_UNVALID	= 0,
    MEDIA_FRAME_VIDEO	= 1, 
    MEDIA_FRAME_AUDIO	= 2
}MEDIA_FRAME_TYPE_E;

typedef struct MP4ENC_NaluUnit {
    int frameType;
    int frameLen;
    unsigned char *pframeBuf;
}MP4ENC_NaluUnit;

class CMp4Encode {
public:
    CMp4Encode(void);
    ~CMp4Encode(void);

    int FileCreate(const char *pFileName, int width, int height, int timescale, float frameRate, int tick);
    int FileWrite(int frameType, unsigned char *pframeBuf,int frameLen);
    int FileClose();

private:
    int WriteAudioTrack(unsigned char *pframeBuf,int frameLen);
    int WriteVideoTrack(unsigned char *pframeBuf,int frameLen);
    static int ReadOneNaluFromBuf(const unsigned char *buffer,unsigned int nBufferSize,unsigned int offSet, struct MP4ENC_NaluUnit &nalu);

private:
    bool m_bfindSps;
    bool m_bfindPps;
    int m_nWidth;
    int m_nHeight;
    int m_nFrameRate;
    float m_nTimeScale;
    int m_tick;
    MP4TrackId m_videoId;
    MP4TrackId m_audioId;
    MP4FileHandle hMp4File;
};

#endif // CMP4ENCODE_H
