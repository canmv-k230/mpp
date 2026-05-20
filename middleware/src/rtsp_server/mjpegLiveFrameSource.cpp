#include <iostream>
#include <MediaSink.hh>
#include "mjpegLiveFrameSource.h"

static struct timeval addUsToTimeval(const struct timeval &base, uint64_t delta_us) {
    struct timeval ref = base;
    ref.tv_sec += delta_us / 1000000;
    ref.tv_usec += delta_us % 1000000;
    if (ref.tv_usec >= 1000000) {
        ref.tv_sec++;
        ref.tv_usec -= 1000000;
    }
    return ref;
}

MjpegLiveVideoSource *MjpegLiveVideoSource::createNew(UsageEnvironment &env, size_t queue_size) {
    return new MjpegLiveVideoSource(env, queue_size);
}

MjpegLiveVideoSource::MjpegLiveVideoSource(UsageEnvironment &env, size_t queue_size) :
    JPEGVideoSource(env), fQueueSize(queue_size) {
    OutPacketBuffer::increaseMaxSizeTo(2 * 1024 * 1024);
    fEventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);
    fThread = std::thread([this](){
        while(this->fNeedReadFrame) {
            this->getFrame();
        }
    });
}

MjpegLiveVideoSource::~MjpegLiveVideoSource() {
    fNeedReadFrame.store(false);
    fCondRaw.notify_all();
    if(fThread.joinable()) {
        fThread.join();
    }
    while (!fFramePacketQueue.empty()) {
        fFramePacketQueue.pop_front();
    }
    while (!fRawDataQueue.empty()) {
        fRawDataQueue.pop_front();
    }
    if(fEventTriggerId) {
        envir().taskScheduler().deleteEventTrigger(fEventTriggerId);
        fEventTriggerId = 0;
    }
}

template <typename T>
std::shared_ptr<T> make_shared_array(size_t size) {
    return std::shared_ptr<T>(new T[size], std::default_delete<T[]>());
}
void MjpegLiveVideoSource::pushData(const uint8_t *data, size_t data_size, uint64_t timestamp) {
    if (data == nullptr || data_size == 0) {
        return;
    }
    if (data_size > maxFrameSize()) {
        std::cout << "MjpegLiveVideoSource::pushData() -- drop oversized frame " << data_size << std::endl;
        return;
    }
    std::shared_ptr<uint8_t> buf = make_shared_array<uint8_t>(data_size);
    memcpy(buf.get(), data, data_size); 

    RawData raw_data;
    raw_data.buffer_ = buf;
    raw_data.size_ = data_size;
    raw_data.timestamp_ = timestamp;
    std::unique_lock<std::mutex> lck(fMutexRaw);
    while (fRawDataQueue.size() >= fQueueSize) {
        fRawDataQueue.pop_front();
        fRawDropCount++;
    }
    fRawDataQueue.push_back(raw_data);
    if (fRawDataQueue.size() > fMaxRawDepth.load()) {
        fMaxRawDepth.store(fRawDataQueue.size());
    }
    if (fRawDataQueue.size() >= fQueueSize / 2) {
        std::cout << "MjpegLiveVideoSource raw queue depth " << fRawDataQueue.size() << "/"
                  << fQueueSize << ", raw_drop " << fRawDropCount.load()
                  << ", packet_drop " << fPacketDropCount.load() << std::endl;
    }
    lck.unlock();
    fCondRaw.notify_one();
}

int MjpegLiveVideoSource::getFrame() {
    RawData raw_data;
    std::unique_lock<std::mutex> lck(fMutexRaw);
    fCondRaw.wait(lck, [this]() {
        return !fNeedReadFrame || !fRawDataQueue.empty();
    });
    if (!fNeedReadFrame && fRawDataQueue.empty()) {
        return 0;
    }
    if (!fRawDataQueue.empty()) {
        raw_data = fRawDataQueue.front();
        fRawDataQueue.pop_front();
    }
    lck.unlock();

    int frameSize = 0;
    if (raw_data.buffer_ && raw_data.size_) {
        struct timeval ref = presentationTimeFor(raw_data.timestamp_);
        frameSize = raw_data.size_;
        processFrame(raw_data.buffer_, frameSize, ref);
    }
    return frameSize;
}

struct timeval MjpegLiveVideoSource::presentationTimeFor(uint64_t timestamp) {
    struct timeval now;
    gettimeofday(&now, NULL);

    if (timestamp == 0) {
        return now;
    }

    if (!fHaveTimestampBase || timestamp <= fLastInputTimestamp) {
        fHaveTimestampBase = true;
        fTimestampBaseInput = timestamp;
        fTimestampBaseTime = now;
        fLastInputTimestamp = timestamp;
        fTimestampScaleToUs = 0;
        return now;
    }

    uint64_t step = timestamp - fLastInputTimestamp;
    fLastInputTimestamp = timestamp;
    if (fTimestampScaleToUs == 0) {
        fTimestampScaleToUs = (step < 10000) ? 1000 : 1;
    }

    uint64_t delta = timestamp - fTimestampBaseInput;
    if (fTimestampScaleToUs == 1000 && delta > (~(uint64_t)0) / 1000) {
        return now;
    }
    return addUsToTimeval(fTimestampBaseTime, delta * fTimestampScaleToUs);
}

void MjpegLiveVideoSource::processFrame(std::shared_ptr<uint8_t> data, size_t size, const struct timeval &ref) {
    if (fParser.parse(const_cast<uint8_t*>(data.get()), size) == 0) {
        unsigned int len = 0;
        const uint8_t *frame_bits = fParser.scandata(len);
        FramePacket packet(data, frame_bits - data.get(), len, ref);
        queueFramePacket(packet);
    }
}

void MjpegLiveVideoSource::queueFramePacket(MjpegLiveVideoSource::FramePacket &packet) {
    std::unique_lock<std::mutex> lck(fMutex);
    while (fFramePacketQueue.size() >= fQueueSize) {
        fFramePacketQueue.pop_front();
        fPacketDropCount++;
    }
    fFramePacketQueue.push_back(packet);
    if (fFramePacketQueue.size() > fMaxPacketDepth.load()) {
        fMaxPacketDepth.store(fFramePacketQueue.size());
    }
    lck.unlock();
    // post an event to ask to deliver the frame
    envir().taskScheduler().triggerEvent(fEventTriggerId, this);
}

void MjpegLiveVideoSource::doGetNextFrame() {
    deliverFrame();
}

void MjpegLiveVideoSource::doStopGettingFrames() {
    FramedSource::doStopGettingFrames();
}

void MjpegLiveVideoSource::deliverFrame0(void *clientData) {
    ((MjpegLiveVideoSource*)clientData)->deliverFrame();
}

void MjpegLiveVideoSource::deliverFrame() {
    if (isCurrentlyAwaitingData()) {
        fDurationInMicroseconds = 0;
        fFrameSize = 0;

        FramePacket packet;
        std::unique_lock<std::mutex> lck(fMutex);
        if (!fFramePacketQueue.empty()) {
            packet = fFramePacketQueue.front();
            fFramePacketQueue.pop_front();
        }
        lck.unlock();

        if(packet.size_) {
            if (packet.size_ > fMaxSize) {
                fFrameSize = fMaxSize;
                fNumTruncatedBytes = packet.size_ - fMaxSize;
                std::cout << "MjpegLiveVideoSource::deliverFrame() -- truncate bytes " << fNumTruncatedBytes << std::endl;
            } else {
                fFrameSize = packet.size_;
            }

            fPresentationTime = packet.timestamp_;
            memcpy(fTo, packet.buffer_.get() + packet.offset_, fFrameSize);
        }

        if (fFrameSize > 0) {
            fDeliverCount++;
            if (fDeliverCount % 300 == 0) {
                std::cout << "MjpegLiveVideoSource stats delivered " << fDeliverCount.load()
                          << ", raw_drop " << fRawDropCount.load()
                          << ", packet_drop " << fPacketDropCount.load()
                          << ", max_raw_depth " << fMaxRawDepth.load()
                          << ", max_packet_depth " << fMaxPacketDepth.load() << std::endl;
            }
            FramedSource::afterGetting(this);
            // envir().taskScheduler().scheduleDelayedTask(0, (TaskFunc*)FramedSource::afterGetting, this);
        }
    }
}
