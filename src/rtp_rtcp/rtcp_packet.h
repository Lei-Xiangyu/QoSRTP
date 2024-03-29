#pragma once
#include <memory>

#include "../include/data_buffer.h"
#include "../include/result.h"

namespace qosrtp {
namespace rtcp {
// The p of the buff loaded by the RtcpPacket class is all set to 0, and there
// will be no padding field at the end. These operations will be left to the
// compound packet to perform.
class RtcpPacket {
 public:
  // Size of the rtcp common header.
  static constexpr uint32_t kHeaderLength = 4;
  RtcpPacket();
  virtual ~RtcpPacket();
  // Size of this packet in bytes (including headers).
  virtual uint32_t BlockLength() const = 0;
  // When passed in, pos represents where the data is written. After returning,
  // pos represents the next position at the end of the packet.
  // max_length refers to the memory size allocated after the packet pointer
  virtual std::unique_ptr<Result> LoadPacket(uint8_t* packet, uint32_t* pos,
                                             uint32_t max_length) const = 0;
  void SetSenderSsrc(uint32_t ssrc) { sender_ssrc_ = ssrc; }
  uint32_t sender_ssrc() const { return sender_ssrc_; }

 protected:
  // From RFC 3550, RTP: A Transport Protocol for Real-Time Applications.
  //
  // RTP header format.
  //   0                   1                   2                   3
  //   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //  |V=2|P| RC/FMT  |      PT       |             length            |
  //  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  static std::unique_ptr<Result> CreateHeader(
      uint8_t count_or_format, uint8_t packet_type,
      uint16_t length,  // Payload size in 32bit words.
      uint8_t* buffer, uint32_t* pos) {
    if (count_or_format > (uint8_t)0x1f) {
      return Result::Create(-1, "count_or_format is bigger than 0x1f.");
    }
    constexpr uint8_t kVersionBits = 2 << 6;
    buffer[*pos + 0] = kVersionBits | count_or_format;
    buffer[*pos + 1] = packet_type;
    buffer[*pos + 2] = (length >> 8) & 0xff;
    buffer[*pos + 3] = length & 0xff;
    *pos += kHeaderLength;
    return Result::Create();
  }
  uint32_t sender_ssrc_;
};
}  // namespace rtcp
}  // namespace qosrtp