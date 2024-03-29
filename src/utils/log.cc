#include "../include/log.h"

using namespace qosrtp;

std::unique_ptr<QosrtpLogger> QosrtpLogger::logger_ = nullptr;

std::shared_mutex QosrtpLogger::rw_mutex_;

void QosrtpLogger::SetLogger(std::unique_ptr<QosrtpLogger> logger) {
  std::unique_lock<std::shared_mutex> write_lock(QosrtpLogger::rw_mutex_);
  QosrtpLogger::logger_ = std::move(logger);
}

QosrtpLogger* QosrtpLogger::logger() {
  std::shared_lock<std::shared_mutex> read_lock(QosrtpLogger::rw_mutex_);
  return QosrtpLogger::logger_.get();
}

QosrtpLogger::QosrtpLogger(Level min_log_level)
    : min_log_level_(min_log_level) {}

QosrtpLogger::~QosrtpLogger() = default;