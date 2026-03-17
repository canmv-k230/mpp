#ifndef UNIT_TEST_MOCK_FRAMED_SOURCE_HH
#define UNIT_TEST_MOCK_FRAMED_SOURCE_HH

#include <cstddef>
#include <cstdint>
#include <sys/time.h>
#include <unordered_map>

using EventTriggerId = unsigned;
using TaskFunc = void(void*);

class TaskScheduler {
  public:
    EventTriggerId createEventTrigger(TaskFunc* func) {
        const EventTriggerId id = ++next_id_;
        triggers_[id] = func;
        return id;
    }

    void triggerEvent(EventTriggerId id, void* client_data) {
        auto it = triggers_.find(id);
        if (it != triggers_.end() && it->second) {
            it->second(client_data);
        }
    }

    void deleteEventTrigger(EventTriggerId id) {
        triggers_.erase(id);
    }

  private:
    EventTriggerId next_id_{0};
    std::unordered_map<EventTriggerId, TaskFunc*> triggers_;
};

class UsageEnvironment {
  public:
    TaskScheduler& taskScheduler() { return scheduler_; }

  private:
    TaskScheduler scheduler_;
};

class FramedSource {
  public:
    explicit FramedSource(UsageEnvironment& env) : env_(env) {}
    virtual ~FramedSource() = default;

    UsageEnvironment& envir() { return env_; }
    bool isCurrentlyAwaitingData() const { return awaiting_data_; }

    void mockSetFrameBuffer(unsigned char* to, unsigned max_size) {
        fTo = to;
        fMaxSize = max_size;
        awaiting_data_ = true;
    }

    void mockSetAwaitingData(bool awaiting) { awaiting_data_ = awaiting; }
    unsigned mockAfterGettingCount() const { return after_getting_count_; }

    static void afterGetting(FramedSource* source) {
        if (source) {
            source->after_getting_count_++;
        }
    }

    static void afterGetting(void* client_data) {
        afterGetting(static_cast<FramedSource*>(client_data));
    }

  protected:
    virtual void doStopGettingFrames() { awaiting_data_ = false; }

  protected:
    UsageEnvironment& env_;
    unsigned char* fTo{nullptr};
    unsigned fMaxSize{0};
    unsigned fFrameSize{0};
    unsigned fNumTruncatedBytes{0};
    unsigned fDurationInMicroseconds{0};
    struct timeval fPresentationTime{0, 0};

  private:
    bool awaiting_data_{false};
    unsigned after_getting_count_{0};
};

#endif
