#include "ntp_time.h"

namespace qosrtp {
NtpTime::NtpTime() : value_(0) {}

NtpTime::NtpTime(uint64_t value) : value_(value) {}

NtpTime::NtpTime(uint32_t seconds, uint32_t fractions)
    : value_(seconds * kFractionsPerSecond + fractions) {}

NtpTime::NtpTime(const NtpTime&) = default;

NtpTime::~NtpTime() = default;
}  // namespace qosrtp