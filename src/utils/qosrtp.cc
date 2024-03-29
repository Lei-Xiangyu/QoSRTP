#include "../include/qosrtp.h"

#include <sstream>

#include "./log_default.h"

using namespace qosrtp;

std::mutex QosrtpInterface::mutex_;

std::unique_ptr<QosrtpInterface> QosrtpInterface::interface_ = nullptr;

QosrtpInterface::QosrtpInterface() = default;

QosrtpInterface::~QosrtpInterface() = default;

std::unique_ptr<Result> QosrtpInterface::Initialize(
    std::unique_ptr<QosrtpLogger> logger, QosrtpLogger::Level log_level) {
  std::lock_guard<std::mutex> lock(QosrtpInterface::mutex_);
  if (nullptr != QosrtpInterface::interface_.get()) {
    return Result::Create(-1, "Already initialized");
  }
  QosrtpInterface::interface_.reset(new QosrtpInterface());
  if (nullptr != logger.get()) {
    QosrtpLogger::SetLogger(std::move(logger));
  } else {
    QosrtpLogger::SetLogger(std::make_unique<QosrtpLoggerDefault>(log_level));
  }
#if defined(_MSC_VER)
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  if (result != 0) {
    std::stringstream result_description;
    result_description << "WSAStartup failed with error code: %d" << result;
    return Result::Create(-1, result_description.str());
  }
#endif
  return Result::Create();
}

std::unique_ptr<Result> QosrtpInterface::UnInitialize() {
  std::lock_guard<std::mutex> lock(QosrtpInterface::mutex_);
  QosrtpInterface::interface_.reset(nullptr);
  QosrtpLogger::SetLogger(nullptr);
#if defined(_MSC_VER)
  WSACleanup();
#endif
  return Result::Create();
}