#pragma once
#include <vector>

#include "common_header.h"
#include "rtpfb.h"

namespace qosrtp {
namespace rtcp {
// RFC 4585: Feedback format.
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
//
// Generic NACK (RFC 4585).
//
// FCI:
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |            PID                |             BLP               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
class Nack : public Rtpfb {
 public:
  static constexpr uint8_t kFeedbackMessageType = 1;
  Nack();
  virtual ~Nack() override;

  // Parse assumes header is already parsed and validated.
  std::unique_ptr<Result> StorePacket(const CommonHeader& packet);
  // The number in nack_list must "increase" with index
  void SetPacketIds(const uint16_t* nack_list, uint32_t length);
  void SetPacketIds(std::vector<uint16_t> nack_list);

  const std::vector<uint16_t>& packet_ids() const { return packet_ids_; }
  virtual uint32_t BlockLength() const override;
  virtual std::unique_ptr<Result> LoadPacket(
      uint8_t* packet, uint32_t* pos, uint32_t max_length) const override;

 private:
  static constexpr uint8_t kNackItemLength = 4;
  struct PackedNack {
    uint16_t first_pid;
    uint16_t bitmask;
  };

  void Pack();    // Fills packed_ using packed_ids_. (used in SetPacketIds).
  void Unpack();  // Fills packet_ids_ using packed_. (used in Parse).

  std::vector<PackedNack> packed_;
  std::vector<uint16_t> packet_ids_;
};
}  // namespace rtcp
}  // namespace qosrtp