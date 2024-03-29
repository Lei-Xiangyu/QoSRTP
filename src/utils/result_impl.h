#pragma once
#include "../include/result.h"

namespace qosrtp {
class ResultImpl : public Result {
 public:
  ResultImpl() : Result() {
    code_ = Result::kSuccessCode;
    description_ = "Successful.";
  }
  ResultImpl(int32_t code, std::string description) : Result() {
    code_ = code;
    description_ = description;
  }
  virtual ~ResultImpl() override;
  virtual const int32_t& code() const override;
  virtual const std::string& description() const override;
  virtual bool ok() const override;

 private:
  int32_t code_;
  std::string description_;
};
}  // namespace qosrtp