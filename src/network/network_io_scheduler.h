#pragma once
#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>
#if defined(_MSC_VER)
#include <Winsock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#if defined(max)
#undef max
#endif  // max
#if defined(min)
#undef min
#endif  // min
#endif

#include "../utils/thread.h"

namespace qosrtp {
enum class NetworkIOEvent {
  kRead = 0x0001,
  kWrite = 0x0002,
  kConnect = 0x0004,
  kClose = 0x0008,
  kAccept = 0x0010,
};

class NetworkIOHandler {
 public:
  virtual uint32_t GetRequestedEvents() = 0;
  virtual void OnEvent(uint32_t ff, int err) = 0;
#if defined(_MSC_VER)
  virtual WSAEVENT GetWSAEvent() const = 0;
  virtual SOCKET GetSocket() const = 0;
#endif
 protected:
  NetworkIOHandler();
  ~NetworkIOHandler();
};

class NetworkIoSignaler;

class NetworkIoScheduler : public ThreadWaitTask {
 public:
  NetworkIoScheduler();
  ~NetworkIoScheduler();

  /* ThreadWaitTask override */
  virtual void WaitUp() override;
  virtual void Wait(uint64_t max_wait_duration_ms) override;

  void AddHandler(NetworkIOHandler* handler) {
    if (nullptr == handler) {
      return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto iter_find = std::find_if(
        handlers_.begin(), handlers_.end(),
        [&handler](const NetworkIOHandler* handler_in_vec) {
#if defined(_MSC_VER)
          return (handler == handler_in_vec) ||
                 (handler->GetSocket() == handler_in_vec->GetSocket());
#else
#error "Unsupported compiler"
#endif
        });
    if (iter_find != handlers_.end()) {
      return;
    }
    handlers_.push_back(handler);
  }

  void RemoveHandler(NetworkIOHandler* handler) {
    if (nullptr == handler) {
      return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto iter_find = std::find(handlers_.begin(), handlers_.end(), handler);
    if (iter_find == handlers_.end()) {
      return;
    }
    handlers_.erase(iter_find);
  }

 private:
  std::mutex mutex_;
  std::vector<NetworkIOHandler*> handlers_;
  std::atomic<bool> wait_break_;
  std::unique_ptr<NetworkIoSignaler> signaler_wakeup_;
#if defined(_MSC_VER)
  const WSAEVENT socket_event_;
#endif
};
}  // namespace qosrtp