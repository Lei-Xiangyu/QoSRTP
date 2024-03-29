#pragma once
#include <memory>
#include <string>
#include <utility>

#include "define.h"

namespace qosrtp {
class QOSRTP_API Result {
 public:
  static constexpr int32_t kSuccessCode = 0;
  static std::unique_ptr<Result> Create();
  static std::unique_ptr<Result> Create(int32_t code, std::string description);
  Result();
  virtual ~Result();
  virtual const int32_t& code() const = 0;
  virtual const std::string& description() const = 0;
  virtual bool ok() const = 0;
};
}  // namespace qosrtp