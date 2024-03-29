#pragma once
#include <atomic>
#include <memory>
#include <mutex>

namespace qosrtp {
class SessionStates {
 public:
  static SessionStates* GetInstance();
  bool HasSentBye();
  void NotifyByeSent();

 private:
  SessionStates();
  ~SessionStates();
  class Instance {
   public:
    Instance();
    ~Instance();
    SessionStates* GetInstance();

   private:
    SessionStates* instance_;
  };
  // for instance_
  static std::mutex mutex_instance_;
  static std::unique_ptr<Instance> instance_;
  std::atomic<bool> has_sent_bye;
};
}  // namespace qosrtp