#include <iostream>
#include <mutex>
#include "LiveFrameSource.h"

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

LiveFrameSource* LiveFrameSource::createNew(UsageEnvironment &env, size_t queue_size) {
    return new LiveFrameSource(env, queue_size);
}

LiveFrameSource::LiveFrameSource(UsageEnvironment &env, size_t queue_size) : FramedSource(env), fQueueSize(queue_size) {
    fEventTriggerId = envir().taskScheduler().createEventTrigger(deliverFrame0);
    fThread = std::thread([this](){
        while(this->fNeedReadFrame) {
            this->getFrame();
        }
    });
}

void LiveFrameSource::pushData(const uint8_t *data, size_t data_size, uint64_t timestamp) {
    if (data == nullptr || data_size == 0) {
        return;
    }
    if (data_size > maxFrameSize()) {
        std::cout << "LiveFrameSource::pushData() -- drop oversized frame " << data_size << std::endl;
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
        std::cout << "LiveFrameSource raw queue depth " << fRawDataQueue.size() << "/"
                  << fQueueSize << ", raw_drop " << fRawDropCount.load()
                  << ", packet_drop " << fPacketDropCount.load() << std::endl;
    }
    lck.unlock();
    fCondRaw.notify_one();
}

int LiveFrameSource::getFrame() {
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

void LiveFrameSource::processFrame(std::shared_ptr<uint8_t> data, size_t size, const struct timeval &ref) {
    std::list<FramePacket> packetList = this->parseFrame(data, size, ref);
    if (packetList.empty()) {
        return;
    }
    uint64_t access_unit_id = ++fNextAccessUnitId;
    for (auto &packet : packetList) {
        packet.access_unit_id_ = access_unit_id;
    }
    queueFramePackets(packetList);
}

void LiveFrameSource::queueFramePacket(LiveFrameSource::FramePacket &packet) {
    std::list<FramePacket> packets;
    packets.push_back(packet);
    queueFramePackets(packets);
}

void LiveFrameSource::dropOldestAccessUnitLocked() {
    if (fFramePacketQueue.empty()) {
        return;
    }
    uint64_t drop_id = fFramePacketQueue.front().access_unit_id_;
    do {
        fFramePacketQueue.pop_front();
        fPacketDropCount++;
    } while (!fFramePacketQueue.empty() && fFramePacketQueue.front().access_unit_id_ == drop_id);
}

void LiveFrameSource::queueFramePackets(std::list<LiveFrameSource::FramePacket> &packets) {
    std::unique_lock<std::mutex> lck(fMutex);
    while (!fFramePacketQueue.empty() && fFramePacketQueue.size() + packets.size() > fQueueSize) {
        dropOldestAccessUnitLocked();
    }
    fFramePacketQueue.splice(fFramePacketQueue.end(), packets);
    if (fFramePacketQueue.size() > fMaxPacketDepth.load()) {
        fMaxPacketDepth.store(fFramePacketQueue.size());
    }
    lck.unlock();

    // post an event to ask to deliver the frame
    envir().taskScheduler().triggerEvent(fEventTriggerId, this);
}

 void LiveFrameSource::doGetNextFrame() {
    deliverFrame();
 }

void LiveFrameSource::doStopGettingFrames() {
    FramedSource::doStopGettingFrames();
}

LiveFrameSource::~LiveFrameSource() {
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

std::list<LiveFrameSource::FramePacket>
LiveFrameSource::parseFrame(std::shared_ptr<uint8_t> frame_data, size_t size, const struct timeval &ref) {
    std::list<FramePacket> frameList;
    if (frame_data != NULL) {
        FramePacket packet(frame_data, 0, size, ref);
        frameList.push_back(packet);
    } else {
        std::cout << "LiveFrameSource::parseFrame  frame empty" << std::endl;
    }
    return frameList;
}

struct timeval LiveFrameSource::presentationTimeFor(uint64_t timestamp) {
    struct timeval now;
    gettimeofday(&now, NULL);

    if (timestamp == 0) {
        return now;
    }

    // Map valid caller timestamps onto wall-clock time. Repeated or stale
    // values are placeholders in existing Python examples, so use arrival time.
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


void LiveFrameSource::deliverFrame0(void *clientData) {
    ((LiveFrameSource*)clientData)->deliverFrame();
}

void LiveFrameSource::deliverFrame() {
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
                std::cout << "LiveFrameSource::deliverFrame() -- truncate bytes " << fNumTruncatedBytes << std::endl;
            } else {
                fFrameSize = packet.size_;
            }
            fPresentationTime = packet.timestamp_;
            memcpy(fTo, packet.buffer_.get() + packet.offset_, fFrameSize);
        }

        if (fFrameSize > 0) {
            fDeliverCount++;
            if (fDeliverCount % 300 == 0) {
                std::cout << "LiveFrameSource stats delivered " << fDeliverCount.load()
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
