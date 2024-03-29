#pragma once
#include "common_header.h"
#include "rtcp_packet.h"

namespace qosrtp {
namespace rtcp {
// RFC 4585, Section 6.1: Feedback format.
//
// Common packet format:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |V=2|P|   FMT   |       PT      |          length               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 0 |                  SSRC of packet sender                        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 4 |                  SSRC of media source                         |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   :            Feedback Control Information (FCI)                 :
//   :                                                               :
class Rtpfb : public RtcpPacket {
 public:
  static constexpr uint8_t kPacketType = 205;

  Rtpfb();
  virtual ~Rtpfb() override;

  void SetMediaSsrc(uint32_t ssrc) { media_ssrc_ = ssrc; }

  uint32_t media_ssrc() const { return media_ssrc_; }

 protected:
  static constexpr uint32_t kCommonFeedbackLength = 8;
  void StoreCommonFeedback(const uint8_t* payload);
  void LoadCommonFeedback(uint8_t* payload) const;

 private:
  uint32_t media_ssrc_ = 0;
};
}  // namespace rtcp
}  // namespace qosrtp