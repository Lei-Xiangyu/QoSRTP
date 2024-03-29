#include "./result_impl.h"

namespace qosrtp {
ResultImpl::~ResultImpl() {}
const int32_t& ResultImpl::code() const { return code_; }
const std::string& ResultImpl::description() const { return description_; }
bool ResultImpl::ok() const { return (kSuccessCode == code_); }
}  // namespace qosrtp