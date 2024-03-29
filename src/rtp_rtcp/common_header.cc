#include "common_header.h"

#include "../utils/byte_io.h"

using namespace qosrtp;
using namespace qosrtp::rtcp;

CommonHeader::CommonHeader() {
  packet_type_ = 0;
  count_or_format_ = 0;
  padding_size_ = 0;
  payload_size_ = 0;
  payload_ = nullptr;
}

CommonHeader::~CommonHeader() = default;

CommonHeader::CommonHeader(const CommonHeader&) = default;

CommonHeader& CommonHeader::operator=(const CommonHeader&) = default;

std::unique_ptr<Result> CommonHeader::StorePacket(const uint8_t* buffer,
                                                  uint32_t size_bytes) {
  uint8_t packet_type_parsed = 0;
  uint8_t count_or_format_parsed = 0;
  uint8_t padding_size_parsed = 0;
  uint32_t payload_size_parsed = 0;
  const uint8_t* payload_parsed = nullptr;
  const uint8_t kVersion = 2;
  if (size_bytes < kHeaderSizeBytes) {
    return Result::Create(-1, "Length less than rtcp header length.");
  }
  uint8_t version = buffer[0] >> 6;
  if (version != kVersion) {
    return Result::Create(-1, "The version of rtcp must be 2.");
  }
  bool has_padding = (buffer[0] & 0x20) != 0;
  count_or_format_parsed = buffer[0] & 0x1F;
  packet_type_parsed = buffer[1];
  payload_size_parsed = ByteReader<uint16_t>::ReadBigEndian(&buffer[2]) * 4;
  if (size_bytes < kHeaderSizeBytes + payload_size_parsed) {
    return Result::Create(
        -1, "The size passed in is smaller than the parsed length.");
  }
  if (has_padding) {
    if (payload_size_parsed == 0) {
      return Result::Create(
          -1,
          "Invalid RTCP header: Padding bit set but 0 payload "
          "size specified.");
    }
    padding_size_parsed = payload_[payload_size_parsed - 1];
    if (padding_size_parsed == 0) {
      return Result::Create(
          -1,
          "Invalid RTCP header: Padding bit set but 0 padding "
          "size specified.");
    }
    if (padding_size_parsed > payload_size_parsed) {
      return Result::Create(
          -1, "Invalid RTCP header: Padding size is bigger than payload size");
    }
    payload_size_parsed -= padding_size_;
  }
  if (payload_size_parsed > 0) {
    payload_parsed = buffer + kHeaderSizeBytes;
  }
  packet_type_ = packet_type_parsed;
  count_or_format_ = count_or_format_parsed;
  padding_size_ = padding_size_parsed;
  payload_size_ = payload_size_parsed;
  payload_ = payload_parsed;
  return Result::Create();
}