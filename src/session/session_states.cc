#include "session_states.h"

namespace qosrtp {
std::mutex SessionStates::mutex_instance_;

SessionStates::Instance::Instance() {
  instance_ = new SessionStates();
}

SessionStates::Instance::~Instance() { delete instance_; }

SessionStates* SessionStates::Instance::GetInstance() { return instance_; }

std::unique_ptr<SessionStates::Instance> SessionStates::instance_(nullptr);

SessionStates* SessionStates::GetInstance() {
  std::lock_guard<std::mutex> lock(mutex_instance_);
  if (!instance_) {
    instance_ = std::make_unique<SessionStates::Instance>();
  }
  return instance_->GetInstance();
}

void SessionStates::NotifyByeSent() { has_sent_bye.store(true); }

bool SessionStates::HasSentBye() { return has_sent_bye.load(); }

SessionStates::SessionStates() : has_sent_bye(false) {}

SessionStates::~SessionStates() = default;
}  // namespace qosrtp