#include "./network_io_scheduler.h"

#include "../include/log.h"
#include "../utils/thread.h"
#include "../utils/time_utils.h"

namespace qosrtp {
NetworkIOHandler::NetworkIOHandler() = default;

NetworkIOHandler ::~NetworkIOHandler() = default;

#if defined(_MSC_VER)
static uint32_t FlagsToEvents(uint32_t events) {
  uint32_t fd = FD_CLOSE;
  if (events & static_cast<uint32_t>(NetworkIOEvent::kRead)) fd |= FD_READ;
  if (events & static_cast<uint32_t>(NetworkIOEvent::kWrite)) fd |= FD_WRITE;
  if (events & static_cast<uint32_t>(NetworkIOEvent::kConnect))
    fd |= FD_CONNECT;
  if (events & static_cast<uint32_t>(NetworkIOEvent::kAccept)) fd |= FD_ACCEPT;
  return fd;
}

class NetworkIoSignaler : public NetworkIOHandler {
 public:
  NetworkIoSignaler() = delete;
  NetworkIoSignaler(NetworkIoScheduler* scheduler,
                    std::atomic<bool>& flag_event)
      : NetworkIOHandler(), flag_event_(flag_event), scheduler_(scheduler) {
    wsa_event_ = WSACreateEvent();
    if (wsa_event_) {
      scheduler_->AddHandler(this);
    }
  }
  ~NetworkIoSignaler() {
    if (wsa_event_ != nullptr) {
      scheduler_->RemoveHandler(this);
      WSACloseEvent(wsa_event_);
      wsa_event_ = nullptr;
    }
  }

  /* NetworkIOHandler override */
  virtual uint32_t GetRequestedEvents() override { return 0; }
  virtual SOCKET GetSocket() const override { return INVALID_SOCKET; }
  virtual WSAEVENT GetWSAEvent() const override { return wsa_event_; }
  virtual void OnEvent(uint32_t ff, int err) override {
    WSAResetEvent(wsa_event_);
    flag_event_.store(true);
  }

  void Signal() {
    if (wsa_event_ != nullptr) WSASetEvent(wsa_event_);
  }

 private:
  std::atomic<bool>& flag_event_;
  NetworkIoScheduler* scheduler_;
  WSAEVENT wsa_event_;
};
#endif

NetworkIoScheduler::NetworkIoScheduler()
    :
#if defined(_MSC_VER)
      socket_event_(WSACreateEvent()),
#endif
      wait_break_(false) {
  signaler_wakeup_ = std::make_unique<NetworkIoSignaler>(this, wait_break_);
}

NetworkIoScheduler::~NetworkIoScheduler() {
  signaler_wakeup_->Signal();
#if defined(_MSC_VER)
  WSACloseEvent(socket_event_);
#endif
}

void NetworkIoScheduler::WaitUp() { signaler_wakeup_->Signal(); }

void NetworkIoScheduler::Wait(uint64_t max_wait_duration_ms) {
  wait_break_.store(false);
  uint64_t time_wait_begin = UTCTimeMillis();
  uint64_t milis_wait_total = max_wait_duration_ms;
  uint64_t milis_elapsed = 0;
  while (!wait_break_.load()) {
    std::vector<WSAEVENT> events;
    std::vector<NetworkIOHandler*> event_owners;
    events.push_back(socket_event_);
    {
      std::lock_guard<std::mutex> lock(mutex_);
      for (auto handler_ptr : handlers_) {
        SOCKET s = handler_ptr->GetSocket();
        if (signaler_wakeup_.get() == handler_ptr) {
          events.push_back(handler_ptr->GetWSAEvent());
          event_owners.push_back(handler_ptr);
        } else if (INVALID_SOCKET != s) {
          WSAEventSelect(s, events[0],
                         FlagsToEvents(handler_ptr->GetRequestedEvents()));
        }
      }
    }
    DWORD milis_wait = -1;
    if (ThreadWaitTask::kForever != milis_wait_total) {
      milis_wait = std::max<int64_t>(0, milis_wait_total - milis_elapsed);
    }
    DWORD dw =
        WSAWaitForMultipleEvents(static_cast<DWORD>(events.size()), &events[0],
                                 false, static_cast<DWORD>(milis_wait), false);
    if (dw == WSA_WAIT_FAILED) {
      QOSRTP_LOG(
          Warning,
          "Failed to call WSAWaitForMultipleEvents, ret: WSA_WAIT_FAILED.");
      return;
    } else if (dw == WSA_WAIT_TIMEOUT) {
      return;
    } else {
      std::lock_guard<std::mutex> lock(mutex_);
      int32_t index = dw - WSA_WAIT_EVENT_0;
      if (index > 0) {
        --index;  // The first event is the socket event
        NetworkIOHandler* handler_ptr = event_owners[index];
        if (std::find(handlers_.begin(), handlers_.end(), handler_ptr) !=
            handlers_.end()) {
          handler_ptr->OnEvent(0, 0);
        }
        // The handler could have been removed while waiting for events.
      } else {
        for (auto handler_ptr : handlers_) {
          SOCKET s = handler_ptr->GetSocket();
          if (s == INVALID_SOCKET) continue;
          WSANETWORKEVENTS wsa_events;
          int err = WSAEnumNetworkEvents(s, events[0], &wsa_events);
          if (err == 0) {
            {
              if ((wsa_events.lNetworkEvents & FD_READ) &&
                  wsa_events.iErrorCode[FD_READ_BIT] != 0) {
                QOSRTP_LOG(Warning,
                           "NetworkIoScheduler got FD_READ_BIT error: %d",
                           wsa_events.iErrorCode[FD_READ_BIT]);
              }
              if ((wsa_events.lNetworkEvents & FD_WRITE) &&
                  wsa_events.iErrorCode[FD_WRITE_BIT] != 0) {
                QOSRTP_LOG(Warning,
                           "NetworkIoScheduler got FD_WRITE_BIT error: %d",
                           wsa_events.iErrorCode[FD_WRITE_BIT]);
              }
              if ((wsa_events.lNetworkEvents & FD_CONNECT) &&
                  wsa_events.iErrorCode[FD_CONNECT_BIT] != 0) {
                QOSRTP_LOG(Warning,
                           "NetworkIoScheduler got FD_CONNECT_BIT error: %d",
                           wsa_events.iErrorCode[FD_CONNECT_BIT]);
              }
              if ((wsa_events.lNetworkEvents & FD_ACCEPT) &&
                  wsa_events.iErrorCode[FD_ACCEPT_BIT] != 0) {
                QOSRTP_LOG(Warning,
                           "NetworkIoScheduler got FD_ACCEPT_BIT error: %d",
                           wsa_events.iErrorCode[FD_ACCEPT_BIT]);
              }
              if ((wsa_events.lNetworkEvents & FD_CLOSE) &&
                  wsa_events.iErrorCode[FD_CLOSE_BIT] != 0) {
                QOSRTP_LOG(Warning,
                           "NetworkIoScheduler got FD_CLOSE_BIT error: %d",
                           wsa_events.iErrorCode[FD_CLOSE_BIT]);
              }
            }
            uint32_t ff = 0;
            int errcode = 0;
            if (wsa_events.lNetworkEvents & FD_READ)
              ff |= static_cast<uint32_t>(NetworkIOEvent::kRead);
            if (wsa_events.lNetworkEvents & FD_WRITE)
              ff |= static_cast<uint32_t>(NetworkIOEvent::kWrite);
            if (wsa_events.lNetworkEvents & FD_CONNECT) {
              if (wsa_events.iErrorCode[FD_CONNECT_BIT] == 0) {
                ff |= static_cast<uint32_t>(NetworkIOEvent::kConnect);
              } else {
                ff |= static_cast<uint32_t>(NetworkIOEvent::kClose);
                errcode = wsa_events.iErrorCode[FD_CONNECT_BIT];
              }
            }
            if (wsa_events.lNetworkEvents & FD_ACCEPT)
              ff |= static_cast<uint32_t>(NetworkIOEvent::kAccept);
            if (wsa_events.lNetworkEvents & FD_CLOSE) {
              ff |= static_cast<uint32_t>(NetworkIOEvent::kClose);
              errcode = wsa_events.iErrorCode[FD_CLOSE_BIT];
            }
            if (ff != 0) {
              handler_ptr->OnEvent(ff, errcode);
            }
          }
        }
      }
    }
    WSAResetEvent(socket_event_);
    milis_elapsed = MilisSince(time_wait_begin);
    if ((milis_elapsed >= milis_wait_total) &&
        (ThreadWaitTask::kForever != milis_wait_total)) {
      break;
    }
  }
}
}  // namespace qosrtp