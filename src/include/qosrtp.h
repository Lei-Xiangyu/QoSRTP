#pragma once
#include <mutex>

#include "define.h"
#include "./data_buffer.h"
#include "./log.h"
#include "./result.h"
#include "./qosrtp_session.h"
#include "./forward_error_correction.h"

namespace qosrtp {
class QOSRTP_API QosrtpInterface {
 public:
  // log_level will only take effect when logger is nullptr
  static std::unique_ptr<Result> Initialize(
      std::unique_ptr<QosrtpLogger> logger, QosrtpLogger::Level log_level);
  static std::unique_ptr<Result> UnInitialize();
  ~QosrtpInterface();

 protected:
  QosrtpInterface();

 private:
  static std::unique_ptr<QosrtpInterface> interface_;
  static std::mutex mutex_;
};
}  // namespace qosrtp