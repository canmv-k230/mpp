#ifndef _KD_RTSP_PUSHER_H
#define _KD_RTSP_PUSHER_H

#include <unistd.h>
#include <cstddef>
#include <cstdint>
#include <memory>

class IRtspPusherEvent {
  public:
    virtual ~IRtspPusherEvent() {}
    virtual void OnRtspPushEvent(int event) = 0; // event 0: connect  ok; event 1:disconnet ;   event 2:reconnect ok
};

struct RtspPusherInitParam {
    int video_width;
    int video_height;
    char sRtspUrl[256];
    int video_fps{25};
    char rtsp_transport[8]{"tcp"};
    IRtspPusherEvent *on_event{nullptr};
};

class KdRtspPusher {
  public:
    KdRtspPusher();
    ~KdRtspPusher();

    int  Init(const RtspPusherInitParam &param);
    void DeInit();
    int  Open();
    void Close();

    int  PushVideoData(const uint8_t *data, size_t size, bool key_frame,uint64_t timestamp);
    int  PushVideoHeader(const uint8_t *data, size_t size);

  private:
    KdRtspPusher(const KdRtspPusher &) = delete;
    KdRtspPusher& operator=(const KdRtspPusher &) = delete;

  private:
    class Impl;
    std::unique_ptr<Impl> impl_{nullptr};
};

#endif // _KD_RTSP_PUSHER_H
