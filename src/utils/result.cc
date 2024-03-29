#include "../include/result.h"

#include "result_impl.h"

using namespace qosrtp;

std::unique_ptr<Result> Result::Create() {
  return std::make_unique<ResultImpl>();
}

std::unique_ptr<Result> Result::Create(int32_t code, std::string description) {
  return std::make_unique<ResultImpl>(code, description);
}

Result::Result() = default;

Result::~Result() = default;