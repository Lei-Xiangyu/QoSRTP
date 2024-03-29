#pragma once
#include <memory>

#include "../include/result.h"

namespace qosrtp {
namespace rtcp {
class CommonHeader {
 public:
  static constexpr uint32_t kHeaderSizeBytes = 4;

  CommonHeader();
  ~CommonHeader();
  CommonHeader(const CommonHeader&);
  CommonHeader& operator=(const CommonHeader&);

  std::unique_ptr<Result> StorePacket(const uint8_t* buffer,
                                      uint32_t size_bytes);

  uint8_t type() const { return packet_type_; }
  // Depending on packet type same header field can be used either as count or
  // as feedback message type (fmt). Caller expected to know how it is used.
  uint8_t fmt() const { return count_or_format_; }
  uint8_t count() const { return count_or_format_; }
  uint32_t payload_size_bytes() const { return payload_size_; }
  const uint8_t* payload() const { return payload_; }
  uint32_t packet_size_bytes() const {
    return kHeaderSizeBytes + payload_size_ + padding_size_;
  }
  // Returns pointer to the next RTCP packet in compound packet.
  const uint8_t* NextPacket() const {
    return payload_ + payload_size_ + padding_size_;
  }

 private:
  uint8_t packet_type_;
  uint8_t count_or_format_;
  uint8_t padding_size_;
  uint32_t payload_size_;
  const uint8_t* payload_;
};
}  // namespace rtcp
}  // namespace qosrtp