#pragma once

#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

struct MockRtspPusherCall {
    std::vector<char> data;
    bool key_frame{false};
    unsigned long long timestamp{0};
};

class RTSPPusherImpl {
  public:
    inline static std::mutex mock_mutex_;
    inline static std::vector<MockRtspPusherCall> push_calls_;
    inline static int init_calls_{0};
    inline static int open_calls_{0};
    inline static int close_calls_{0};
    inline static int deinit_calls_{0};

    static void ResetMockState() {
        std::lock_guard<std::mutex> lock(mock_mutex_);
        push_calls_.clear();
        init_calls_ = 0;
        open_calls_ = 0;
        close_calls_ = 0;
        deinit_calls_ = 0;
    }

    RTSPPusherImpl() = default;
    ~RTSPPusherImpl() = default;

    int init(const char* url, int, int, int, const char*) {
        std::lock_guard<std::mutex> lock(mock_mutex_);
        ++init_calls_;
        last_url_ = (url ? url : "");
        return 0;
    }

    int pushVideo(char* buf, int len, bool key, unsigned long long ts) {
        std::lock_guard<std::mutex> lock(mock_mutex_);
        MockRtspPusherCall call;
        if (buf && len > 0) {
            call.data.assign(buf, buf + len);
        }
        call.key_frame = key;
        call.timestamp = ts;
        push_calls_.push_back(call);
        return 0;
    }

    int open() {
        std::lock_guard<std::mutex> lock(mock_mutex_);
        ++open_calls_;
        return 0;
    }

    void close() {
        std::lock_guard<std::mutex> lock(mock_mutex_);
        ++close_calls_;
    }

    int deinit() {
        std::lock_guard<std::mutex> lock(mock_mutex_);
        ++deinit_calls_;
        return 0;
    }

    void setSPSPPS(char*, int, char*, int) {}
    void setSPSPPS_EX(char*, int) {}

  private:
    std::string last_url_;
};
