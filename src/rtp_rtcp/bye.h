#pragma once
#include <vector>

#include "../include/data_buffer.h"
#include "common_header.h"
#include "rtcp_packet.h"

namespace qosrtp {
namespace rtcp {
// Bye packet (BYE) (RFC 3550).
//
//        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |V=2|P|    SC   |   PT=BYE=203  |             length            |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |                           SSRC/CSRC                           |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       :                              ...                              :
//       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// (opt) |     length    |               reason for leaving            ...
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class Bye : public RtcpPacket {
 public:
  static constexpr uint8_t kPacketType = 203;

  Bye();
  virtual ~Bye() override;

  // Parse assumes header is already parsed and validated.
  std::unique_ptr<Result> StorePacket(const CommonHeader& packet);

  std::unique_ptr<Result> SetCsrcs(std::vector<uint32_t> csrcs);
  std::unique_ptr<Result> SetReason(std::string reason);

  const std::vector<uint32_t>& csrcs() const { return csrcs_; }
  const std::string& reason() const { return reason_; }

  virtual uint32_t BlockLength() const override;

  virtual std::unique_ptr<Result> LoadPacket(
      uint8_t* packet, uint32_t* pos, uint32_t max_length) const override;

 private:
  static const int kMaxNumberOfCsrcs = 0x1f - 1;  // First item is sender SSRC.

  std::vector<uint32_t> csrcs_;
  std::string reason_;
};
}  // namespace rtcp
}  // namespace qosrtp