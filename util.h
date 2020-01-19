/***********************************************
Copyright           : 2015 leef.biant Inc.
Filename            : util.h
Author              : bt731001@163.com
Description         : ---
Create              : 2020-01-06 15:11:02
Last Modified       : 2020-01-06 15:11:02
***********************************************/

#ifndef UTIL_H_
#define UTIL_H_

#include <pthread.h>
#include <math.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>


inline char* GetCurDateTimeStr() {
  static char timedatestamp[32] = {0};
  struct timeval stLogTv;
  gettimeofday(&stLogTv, NULL);
  struct tm strc_now = *localtime(&stLogTv.tv_sec);
  snprintf(timedatestamp, sizeof(timedatestamp) -1, "%04d-%02d-%02d %02d:%02d:%02d:%03ld:%03ld"
      , strc_now.tm_year + 1900, strc_now.tm_mon + 1, strc_now.tm_mday
      , strc_now.tm_hour, strc_now.tm_min, strc_now.tm_sec, stLogTv.tv_usec/1000,  stLogTv.tv_usec%1000);
    return timedatestamp;
}

#ifndef debug
#define debug(fmt, args...) \
do { \
  const char* time_str = GetCurDateTimeStr(); \
  fprintf(stderr, "[%s]|%d|%ld(%s:%d:%s):" fmt "\n", \
    time_str, getpid(),syscall(SYS_gettid), __FILE__, __LINE__, __func__, ##args);\
} while (0)
#endif

#define FRAME_MAX_LEN 1024*5
#define BUFFER_MAX_LEN 1024*1024

struct Packet {
  char* data;
  int len;
  Packet():data(NULL), len(0) {
  }
  Packet(int len) {
    this->len = len;
    this->data = new char[this->len];;
  }
  Packet(const char* data, int len) {
    this->len = len;
    this->data = new char[this->len];;
    memcpy(this->data, data, this->len);
  }
  virtual ~Packet() {
    if (data) {
      delete[] data;
    }
  }
};

struct PcmPacket : public Packet {
  int samples; 
  int channels; 
  PcmPacket() :Packet() {
  }
  PcmPacket(int len) : Packet(len) {
  }
  PcmPacket(const char* data, int len) : Packet(data, len) {
  }
  virtual ~PcmPacket() {
  }
};

class CLock {
  public:
    CLock() {
      pthread_mutex_init(&m_mutex, NULL);
    }

    ~CLock() {
      pthread_mutex_destroy(&m_mutex);
    }

  public:
    inline void Lock() {
      pthread_mutex_lock(&m_mutex);
    }

    inline void UnLock() {
      pthread_mutex_unlock(&m_mutex);
    }

    inline pthread_mutex_t* GetHandle() {
      return &m_mutex;
    }

  private:
    pthread_mutex_t m_mutex;
};

class CAutoLock {
  public:
    CAutoLock(pthread_mutex_t* mutex) {
      m_mutex = mutex;
      pthread_mutex_lock(m_mutex);
    }
    ~CAutoLock() {
      pthread_mutex_unlock(m_mutex);
    }
  private:
    pthread_mutex_t* m_mutex;
};

typedef void (*CallbackDel)(void* pVoid);
template<typename T>
class CObjectPool {
struct Node_t {
  T*    pData;
  Node_t* pNext;

  Node_t(T* data, Node_t* p):pData(data), pNext(p) {
  }
  Node_t():pNext(NULL) {
  }
};

public:
  CObjectPool();
  virtual ~CObjectPool();

public:
  void  Clear();
  int   SetObject(T* pInObj);
  int   GetObject(T** ppOutObj);
  int   GetNum(); 

private:
  CallbackDel m_cbDel;
  Node_t *m_pHead, *m_pTail;
  CLock   m_orLock;
  CLock   m_owLock;
  uint32_t    m_dwMaxSize;
  uint32_t    m_dwMySize;
};

template<typename T>
CObjectPool<T>::CObjectPool() {
  m_dwMaxSize = 0;
  m_dwMySize = 0;
  m_pHead = m_pTail = new Node_t();
}

template<typename T>
CObjectPool<T>::~CObjectPool() {
  Node_t* pCur = NULL;
  while (m_pHead) {
    pCur = m_pHead;
    m_pHead = m_pHead->pNext;
    delete pCur;
  }
  m_pHead = m_pTail = NULL;
  m_dwMySize = 0;
}

template<typename T>
void CObjectPool<T>::Clear() {
  Node_t* pCur = NULL;
  while (m_pHead) {
    pCur = m_pHead;
    m_pHead = m_pHead->pNext;
    delete pCur;
  }
  m_pHead = m_pTail = new Node_t();
  m_dwMySize = 0;
}

template<typename T>
int CObjectPool<T>::SetObject(T* pInObj) {
  if(pInObj == NULL)
    return -1;

  Node_t* pCur = new Node_t(pInObj, NULL);
  {
    CAutoLock lock(m_owLock.GetHandle());
    m_pTail->pNext = pCur;
    m_pTail = m_pTail->pNext;
  }
  __sync_fetch_and_add(&m_dwMySize, 1);
  return 0;
}

template<typename T>
int CObjectPool<T>::GetObject(T** ppOutObj) {
  Node_t* oldvalue = NULL;
  {
    CAutoLock lock(m_orLock.GetHandle());
    if (m_pHead == m_pTail) {
      return -1;
    }
    oldvalue = m_pHead;
    m_pHead = m_pHead->pNext;
    *ppOutObj = m_pHead->pData;
  }
  __sync_sub_and_fetch(&m_dwMySize,1);
  delete oldvalue;
  return 0;
}

template<typename T>
int CObjectPool<T>::GetNum() {
  return m_dwMySize;
}


inline int get_one_ADTS_frame(unsigned char* buffer, size_t buf_size,
    unsigned char* data ,size_t* data_size) {
  size_t size = 0;
  const unsigned char* p = buffer;
  if(!buffer || !data || !data_size ) {
    return -1;
  }
  while(1) {
    if(buf_size  < 7 ) {
      return -1;
    }
    if((buffer[0] == 0xff) && ((buffer[1] & 0xf0) == 0xf0) ) {
      size |= ((buffer[3] & 0x03) <<11);      //high 2 bit
      size |= buffer[4]<<3;                  //middle 8 bit
      size |= ((buffer[5] & 0xe0)>>5);        //low 3bit
      break;
    }
    --buf_size;
    ++buffer;
  }
  if(buf_size < size) {
    return -1;
  }
  memcpy(data, buffer, size);
  *data_size = size;
  return buffer + size - p;
}

#endif

