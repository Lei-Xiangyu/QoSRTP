#pragma once
#include <limits>
#include <memory>

#ifdef max
#undef max
#endif  // max

namespace qosrtp {
// Determine whether b is within the range specified by range_num after a
// The range_num passed in must be less than or equal to
// std::numeric_limits<uint16_t>::max() >> 1
inline bool IsSeqAfterInRange(uint16_t a, uint16_t b, uint16_t range_num) {
  constexpr uint16_t max_seq = std::numeric_limits<uint16_t>::max();
  if (a == b) {
    return false;
  } else if (b > a) {
    return (b - a) <= range_num;
  } else /*if (b < a)*/ {
    return (max_seq - a + b + 1) <= range_num;
  }
}

// Determine whether a is within range_num before b
inline bool IsSeqBeforeInRange(uint16_t a, uint16_t b, uint16_t range_num) {
  return IsSeqAfterInRange(a, b, range_num);
}

inline bool IsNextSeq(uint16_t a, uint16_t b) {
  return IsSeqAfterInRange(a, b, 1);
}

// Determine whether b comes after a
inline bool IsSeqAfter(uint16_t a, uint16_t b) {
  constexpr uint16_t max_seq = std::numeric_limits<uint16_t>::max();
  return IsSeqAfterInRange(a, b, max_seq >> 1);
}

inline bool IsSeqBefore(uint16_t a, uint16_t b) { return IsSeqAfter(a, b); }

inline uint16_t DiffSeq(uint16_t a, uint16_t b) {
  if (b >= a) {
    return b - a;
  }
  return b + std::numeric_limits<uint16_t>::max() - a + 1;
}
}  // namespace qosrtp