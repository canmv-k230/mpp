#include <functional>
#include <future>
#include <iostream>
#include <fstream>
#include <memory>
#include <list>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <new>
#include "rtsp_pusher.h"
#include "RtspPusherImpl.h"
#include "media.h"

#define MAX_LIVE_FRAME_CNT   32
#define MAX_LIVE_FRAME_SIZE  (2 * 1024 * 1024)
struct LiveFramePacket
{
    char *sData;
    int  dataLen;
    uint64_t timestamp;
    bool  key_frame;
};

typedef std::list<LiveFramePacket*>   LIVE_FRAME_PACKET_LIST;

class KdRtspPusher::Impl {
  public:
    Impl() {}
    ~Impl() { DeInit(); }

    int  Init(const RtspPusherInitParam &param);
    void DeInit();
    int  Open();
    void Close();

    int PushVideoData(const uint8_t *data, size_t size, bool key_frame,uint64_t timestamp);
    int PushVideoHeader(const uint8_t *data, size_t size);

  private:
    Impl(const Impl &) = delete;
    Impl& operator=(const Impl &) = delete;
    void _init_frame_free_queue();
    void _deinit_frame_free_queue();
    LiveFramePacket* _get_frame_from_free_queue();
    int  _put_frame_to_free_queue(LiveFramePacket* frame_packet);
    bool _drop_one_queued_frame_locked();
    static void* push_data_thread(void* pParam);
    int  _do_push_frame_data();

  private:
    RTSPPusherImpl   rtsp_pusher_;
    LIVE_FRAME_PACKET_LIST fLiveFrameFreeQueue_;
    LIVE_FRAME_PACKET_LIST fLiveFrameQueue_;
    std::mutex   fMutexFrame_;
    std::mutex   fMutexFreeFrame_;
    std::condition_variable fCondFrame_;
    pthread_t    hpushFrameThread_;
    std::atomic<bool> bStartPushFrame_{false};
    std::atomic<unsigned long long> dropped_frame_count_{0};
    std::atomic<unsigned long long> pushed_frame_count_{0};
    std::atomic<size_t> max_queue_depth_{0};
    RtspPusherInitParam PusherInitParam_;
    char         video_header_[1024];
    int          video_header_len_ = {0};

};

int KdRtspPusher::Impl::Init(const RtspPusherInitParam &param) {
  printf("rtsp_pusher url:%s\n",param.sRtspUrl);
  PusherInitParam_ = param;
  _init_frame_free_queue();
  rtsp_pusher_.init(param.sRtspUrl,param.video_width,param.video_height,param.video_fps,param.rtsp_transport);
  return 0;
}

void KdRtspPusher::Impl::DeInit() {
    Close();
    _deinit_frame_free_queue();
    rtsp_pusher_.deinit();
    return ;
}

int KdRtspPusher::Impl::Open() {
  if (rtsp_pusher_.open() != 0)
  {
      return -1;
  }
  bStartPushFrame_ = true;
  if (pthread_create(&hpushFrameThread_, NULL, push_data_thread, this) != 0)
  {
      bStartPushFrame_ = false;
      rtsp_pusher_.close();
      return -1;
  }

  return 0;
}

void KdRtspPusher::Impl::Close() {
  if (!bStartPushFrame_)
  {
      return;
  }
  bStartPushFrame_ = false;
  fCondFrame_.notify_all();
  pthread_join(hpushFrameThread_,nullptr);
  rtsp_pusher_.close();
}

int  KdRtspPusher::Impl::PushVideoHeader(const uint8_t *data, size_t size)
{
    if (size > 1024)
    {
        printf("push_venc_header size:%d(max size:%d)\n",size,1024);
        return -1;
    }
    memcpy(video_header_,data,size);
    video_header_len_ = size;
    return 0;
}

int KdRtspPusher::Impl::PushVideoData(const uint8_t *data, size_t size, bool key_frame,uint64_t timestamp) {

    if (!bStartPushFrame_)
    {
        return -1;
    }

  std::unique_lock<std::mutex> lck(fMutexFrame_);
  while (fLiveFrameQueue_.size() >= MAX_LIVE_FRAME_CNT)
  {
      if (!_drop_one_queued_frame_locked())
      {
          printf("%s fLiveFrameQueue_ is full and cannot drop\n",PusherInitParam_.sRtspUrl);
          return -1;
      }
  }
  if (size > MAX_LIVE_FRAME_SIZE)
  {
      printf("%s  push_venc_data size:%d(max size:%d)\n",PusherInitParam_.sRtspUrl,size,MAX_LIVE_FRAME_SIZE);
      return -1;
  }

  LiveFramePacket *livePacket = _get_frame_from_free_queue();
  if (livePacket == nullptr)
  {
      if (_drop_one_queued_frame_locked())
      {
          livePacket = _get_frame_from_free_queue();
      }
      if (livePacket == nullptr)
      {
          printf("%s _get_frame_from_free_queue failed:fLiveFrameQueue_ size:%d\n",PusherInitParam_.sRtspUrl,(int)fLiveFrameQueue_.size());
          return -1;
      }
  }

  livePacket->timestamp = timestamp;
  livePacket->dataLen = size;
  livePacket->key_frame = key_frame;
  if (video_header_len_ > 0)
  {
      if (size + video_header_len_ > MAX_LIVE_FRAME_SIZE)
      {
          printf("%s push_venc_data size:%d + header:%d(max size:%d)\n",PusherInitParam_.sRtspUrl,size,video_header_len_,MAX_LIVE_FRAME_SIZE);
          _put_frame_to_free_queue(livePacket);
          return -1;
      }
      memcpy(livePacket->sData,video_header_,video_header_len_);
      memcpy(livePacket->sData + video_header_len_,data,size);
      livePacket->dataLen += video_header_len_;
  }
  else
  {
      memcpy(livePacket->sData,data,size);
  }

  fLiveFrameQueue_.push_back(livePacket);
  if (fLiveFrameQueue_.size() > max_queue_depth_.load())
  {
      max_queue_depth_.store(fLiveFrameQueue_.size());
  }
  if ((dropped_frame_count_ > 0 && dropped_frame_count_ % 30 == 0) ||
      fLiveFrameQueue_.size() >= MAX_LIVE_FRAME_CNT / 2)
  {
      printf("%s rtsp pusher queue depth:%d/%d, dropped:%llu, max_depth:%d\n",
             PusherInitParam_.sRtspUrl,
             (int)fLiveFrameQueue_.size(),
             MAX_LIVE_FRAME_CNT,
             dropped_frame_count_.load(),
             (int)max_queue_depth_.load());
  }
  lck.unlock();
  fCondFrame_.notify_one();
  return 0;
}

void KdRtspPusher::Impl::_init_frame_free_queue()
{
    std::unique_lock<std::mutex> lck(fMutexFreeFrame_);
    for (int i =0;i < MAX_LIVE_FRAME_CNT;i ++)
    {
        LiveFramePacket *frame_packet = new (std::nothrow) LiveFramePacket();
        if (frame_packet == nullptr)
        {
            printf("new frame packet failed\n");
            continue;
        }
        frame_packet->dataLen = 0;
        frame_packet->timestamp = 0;
        frame_packet->sData = new (std::nothrow) char[MAX_LIVE_FRAME_SIZE];
        if (frame_packet->sData == nullptr)
        {
            printf("new frame packet failed\n");
            delete frame_packet;
            continue;
        }
        fLiveFrameFreeQueue_.push_back(frame_packet);
    }
}

void KdRtspPusher::Impl::_deinit_frame_free_queue()
{
    std::unique_lock<std::mutex> lck_frame(fMutexFrame_);
    std::unique_lock<std::mutex> lck(fMutexFreeFrame_);
    for (LIVE_FRAME_PACKET_LIST::iterator itr = fLiveFrameQueue_.begin();itr != fLiveFrameQueue_.end();itr ++)
    {
        fLiveFrameFreeQueue_.push_back(*itr);
    }
    fLiveFrameQueue_.clear();

    for (LIVE_FRAME_PACKET_LIST::iterator itr = fLiveFrameFreeQueue_.begin();itr != fLiveFrameFreeQueue_.end();itr ++)
    {
        delete[] (*itr)->sData;
        (*itr)->sData = nullptr;
        delete *itr;
    }
    fLiveFrameFreeQueue_.clear();
}

LiveFramePacket* KdRtspPusher::Impl::_get_frame_from_free_queue()
{
    std::unique_lock<std::mutex> lck(fMutexFreeFrame_);
    if (fLiveFrameFreeQueue_.size() > 0)
    {
        LiveFramePacket* frame_packet = fLiveFrameFreeQueue_.front();
        fLiveFrameFreeQueue_.pop_front();
        return frame_packet;
    }

    return nullptr;
}

int  KdRtspPusher::Impl::_put_frame_to_free_queue(LiveFramePacket* frame_packet)
{
    std::unique_lock<std::mutex> lck(fMutexFreeFrame_);
    if (fLiveFrameFreeQueue_.size() >= MAX_LIVE_FRAME_CNT)
    {
        return -1;
    }
    fLiveFrameFreeQueue_.push_back(frame_packet);
    return 0;
}

bool KdRtspPusher::Impl::_drop_one_queued_frame_locked()
{
    if (fLiveFrameQueue_.empty())
    {
        return false;
    }

    LIVE_FRAME_PACKET_LIST::iterator drop_itr = fLiveFrameQueue_.begin();
    for (LIVE_FRAME_PACKET_LIST::iterator itr = fLiveFrameQueue_.begin(); itr != fLiveFrameQueue_.end(); ++itr)
    {
        if (!(*itr)->key_frame)
        {
            drop_itr = itr;
            break;
        }
    }

    LiveFramePacket* dropped = *drop_itr;
    fLiveFrameQueue_.erase(drop_itr);
    dropped_frame_count_++;
    _put_frame_to_free_queue(dropped);
    return true;
}

int  KdRtspPusher::Impl::_do_push_frame_data()
{
    bool key_frame = false;
    int ncount = 0;
    while(bStartPushFrame_)
    {
        std::unique_lock<std::mutex> lck(fMutexFrame_);
        fCondFrame_.wait(lck, [this]() {
            return !bStartPushFrame_ || !fLiveFrameQueue_.empty();
        });
        if (!bStartPushFrame_ && fLiveFrameQueue_.empty())
        {
            break;
        }
        if (fLiveFrameQueue_.size() > 0)
        {
            if (++ ncount  % 1000 == 0)
            {
                printf("[%d]%s fLiveFrameQueue_ size:%d,free framequeue size:%d\n",ncount,PusherInitParam_.sRtspUrl, fLiveFrameQueue_.size(),fLiveFrameFreeQueue_.size());
            }

            LiveFramePacket* live_packet = fLiveFrameQueue_.front();
            fLiveFrameQueue_.pop_front();
            lck.unlock();

            key_frame = live_packet->key_frame;
            rtsp_pusher_.pushVideo(live_packet->sData,live_packet->dataLen,key_frame,live_packet->timestamp);
            pushed_frame_count_++;
            if (pushed_frame_count_ % 300 == 0)
            {
                std::unique_lock<std::mutex> stat_lck(fMutexFrame_);
                int queue_depth = (int)fLiveFrameQueue_.size();
                stat_lck.unlock();
                std::unique_lock<std::mutex> free_lck(fMutexFreeFrame_);
                int free_depth = (int)fLiveFrameFreeQueue_.size();
                free_lck.unlock();
                printf("%s rtsp pusher stats pushed:%llu dropped:%llu queue:%d free:%d max_depth:%d\n",
                       PusherInitParam_.sRtspUrl,
                       pushed_frame_count_.load(),
                       dropped_frame_count_.load(),
                       queue_depth,
                       free_depth,
                       (int)max_queue_depth_.load());
            }
            _put_frame_to_free_queue(live_packet);

        }

    }
    return 0;
}

void* KdRtspPusher::Impl::push_data_thread(void* pParam)
{
    KdRtspPusher::Impl* pthis = (KdRtspPusher::Impl*)pParam;
    pthis->_do_push_frame_data();
    return nullptr;
}



KdRtspPusher::KdRtspPusher() : impl_(std::make_unique<Impl>()) {}
KdRtspPusher::~KdRtspPusher() {}

int KdRtspPusher::Init(const RtspPusherInitParam &param) {
  return impl_->Init(param);
}

void KdRtspPusher::DeInit() {
  return impl_->DeInit();
}

int KdRtspPusher::Open() {
  return impl_->Open();
}

void KdRtspPusher::Close() {
  impl_->Close();
}

int KdRtspPusher::PushVideoData(const uint8_t *data, size_t size, bool key_frame,uint64_t timestamp) {
  return impl_->PushVideoData(data, size,key_frame, timestamp);
}

int KdRtspPusher::PushVideoHeader(const uint8_t *data, size_t size) {
  return impl_->PushVideoHeader(data, size);
}
