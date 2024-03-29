#pragma once
#include <cassert>
#include <memory>
#include <shared_mutex>

#include "define.h"

#define QOSRTP_LOG(level, log_string, ...)   \
  (!qosrtp::QosrtpLogger::logger())          \
      ? assert(false)                        \
      : qosrtp::QosrtpLogger::logger()->Log( \
            qosrtp::QosrtpLogger::Level::k##level, log_string, ##__VA_ARGS__);

namespace qosrtp {
class QOSRTP_API QosrtpLogger {
 public:
  static void SetLogger(std::unique_ptr<QosrtpLogger> logger);
  static QosrtpLogger* logger();
  enum class Level {
    kTrace = 0,
    kInfo,
    kWarning,
    kError,
    kFatal,
  };
  QosrtpLogger(Level min_log_level);
  virtual ~QosrtpLogger();
  virtual void Log(Level log_level, const char* format, ...) = 0;

 protected:
  Level min_log_level_;

 private:
  static std::unique_ptr<QosrtpLogger> logger_;
  static std::shared_mutex rw_mutex_;
};
}  // namespace qosrtp