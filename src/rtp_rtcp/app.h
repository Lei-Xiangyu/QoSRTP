#pragma once
#include "../include/data_buffer.h"
#include "common_header.h"
#include "rtcp_packet.h"

namespace qosrtp {
namespace rtcp {
// Application-Defined packet (APP) (RFC 3550).
//
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |V=2|P| subtype |   PT=APP=204  |             length            |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  0 |                           SSRC/CSRC                           |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  4 |                          name (ASCII)                         |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |                   application-dependent data                ...
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

class App : public RtcpPacket {
 public:
  static constexpr uint8_t kPacketType = 204;
  App();
  virtual ~App() override;

  // Parse assumes header is already parsed and validated.
  std::unique_ptr<Result> StorePacket(const CommonHeader& packet);

  std::unique_ptr<Result> SetSubType(uint8_t subtype);
  void SetName(uint32_t name) { name_ = name; }
  std::unique_ptr<Result> SetData(const uint8_t* data, uint32_t data_length);

  uint8_t sub_type() const { return sub_type_; }
  uint32_t name() const { return name_; }
  uint32_t data_size() const { return data_->size(); }
  const uint8_t* data() const { return data_->Get(); }

  virtual uint32_t BlockLength() const override;

  virtual std::unique_ptr<Result> LoadPacket(
      uint8_t* packet, uint32_t* pos, uint32_t max_length) const override;

  static inline constexpr uint32_t NameToInt(const char name[5]) {
    return static_cast<uint32_t>(name[0]) << 24 |
           static_cast<uint32_t>(name[1]) << 16 |
           static_cast<uint32_t>(name[2]) << 8 | static_cast<uint32_t>(name[3]);
  }

 private:
  static constexpr uint32_t kAppBaseLength = 8;  // Ssrc and Name.
  static constexpr uint32_t kMaxDataSize = 0xffff * 4 - kAppBaseLength;

  uint8_t sub_type_;
  uint32_t name_;
  std::unique_ptr<DataBuffer> data_;
};
}  // namespace rtcp
}  // namespace qosrtp