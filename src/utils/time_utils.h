#pragma once
#include <memory>
#if defined(_MSC_VER)
#include <windows.h>
#include <mmsystem.h>
#include <sys/timeb.h>
#if defined(max)
#undef max
#endif  // max
#if defined(min)
#undef min
#endif  // min
#endif

namespace qosrtp {
static const uint64_t kNumMillisecsPerSec = INT64_C(1000);
static const uint64_t kNumMicrosecsPerSec = INT64_C(1000000);
static const uint64_t kNumNanosecsPerSec = INT64_C(1000000000);

static const uint64_t kNumMicrosecsPerMillisec =
    kNumMicrosecsPerSec / kNumMillisecsPerSec;
static const uint64_t kNumNanosecsPerMillisec =
    kNumNanosecsPerSec / kNumMillisecsPerSec;
static const uint64_t kNumNanosecsPerMicrosec =
    kNumNanosecsPerSec / kNumMicrosecsPerSec;

static const uint64_t kNtpJan1970Millisecs =
    2'208'988'800 * kNumMillisecsPerSec;
static const uint64_t kNtpFractionsPerSecond = 0x100000000;

inline uint64_t UTCTimeMillis() {
  uint64_t milis;
#if defined(_MSC_VER)
  struct _timeb time;
  _ftime(&time);
  //milis = (static_cast<uint64_t>(time.time) * kNumMicrosecsPerSec +
  //         static_cast<uint64_t>(time.millitm) * kNumMicrosecsPerMillisec);
  milis = (static_cast<uint64_t>(time.time) * kNumMillisecsPerSec +
           static_cast<uint64_t>(time.millitm));
#else
#error "Unsupported compiler"
#endif
  return milis;
}

inline uint64_t MilisSince(uint64_t milis) { return UTCTimeMillis() - milis; }

inline uint64_t NtpTimeNow() {
  return (UTCTimeMillis() + kNtpJan1970Millisecs) *
         (kNtpFractionsPerSecond / kNumMillisecsPerSec);
}

}  // namespace qosrtp