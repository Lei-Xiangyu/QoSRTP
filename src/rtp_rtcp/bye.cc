#include "bye.h"

#include <sstream>

#include "../utils/byte_io.h"

using namespace qosrtp;
using namespace qosrtp::rtcp;

Bye::Bye() = default;

Bye::~Bye() = default;

std::unique_ptr<Result> Bye::SetReason(std::string reason) {
  if (reason.size() > 0xffu) {
    return Result::Create(-1, "The length of reason must be less than 0xffu.");
  }
  reason_ = std::string(reason);
  return Result::Create();
}

std::unique_ptr<Result> Bye::SetCsrcs(std::vector<uint32_t> csrcs) {
  if (csrcs.size() > kMaxNumberOfCsrcs) {
    return Result::Create(-1, "Too many CSRCs for Bye packet.");
  }
  csrcs_ = std::move(csrcs);
  return Result::Create();
}

uint32_t Bye::BlockLength() const {
  uint32_t src_count = (1 + csrcs_.size());
  uint32_t reason_size_in_32bits =
      reason_.empty() ? 0 : (reason_.size() / 4 + 1);
  return kHeaderLength + 4 * (src_count + reason_size_in_32bits);
}

std::unique_ptr<Result> Bye::LoadPacket(uint8_t* packet, uint32_t* pos,
                                        uint32_t max_length) const {
  /*TODO Need review!*/
  if (*pos + BlockLength() > max_length) {
    return Result::Create(-1,
                          "The remaining buffer space is less than the length "
                          "of the loaded packet");
  }
  const size_t index_end = *pos + BlockLength();
  CreateHeader(1 + csrcs_.size(), kPacketType,
               (BlockLength() - kHeaderLength) / sizeof(uint32_t), packet, pos);
  // Store srcs of the leaving clients.
  ByteWriter<uint32_t>::WriteBigEndian(&packet[*pos], sender_ssrc());
  *pos += sizeof(uint32_t);
  for (uint32_t csrc : csrcs_) {
    ByteWriter<uint32_t>::WriteBigEndian(&packet[*pos], csrc);
    *pos += sizeof(uint32_t);
  }
  // Store the reason to leave.
  if (!reason_.empty()) {
    uint8_t reason_length = static_cast<uint8_t>(reason_.size());
    packet[(*pos)++] = reason_length;
    memcpy(&packet[*pos], reason_.data(), reason_length);
    *pos += reason_length;
    // Add padding bytes if needed.
    uint32_t bytes_to_pad = index_end - *pos;
    if (bytes_to_pad > 0) {
      memset(&packet[*pos], 0, bytes_to_pad);
      *pos += bytes_to_pad;
    }
  }
  return Result::Create();
}

std::unique_ptr<Result> Bye::StorePacket(const CommonHeader& packet) {
  if (kPacketType != packet.type()) {
    return Result::Create(-1, "The type of rtcp is not app.");
  }
  const uint8_t src_count = packet.count();
  // Validate packet.
  if (packet.payload_size_bytes() < 4u * src_count) {
    return Result::Create(
        -1, "Packet is too small to contain CSRCs it promise to have.");
  }
  const uint8_t* const payload = packet.payload();
  bool has_reason = packet.payload_size_bytes() > 4u * src_count;
  uint8_t reason_length = 0;
  if (has_reason) {
    reason_length = payload[4u * src_count];
    if (packet.payload_size_bytes() - 4u * src_count < 1u + reason_length) {
      return Result::Create(-1, "Invalid reason length.");
    }
  }
  // Once sure packet is valid, copy values.
  if (src_count == 0) {  // A count value of zero is valid, but useless.
    SetSenderSsrc(0);
    csrcs_.clear();
  } else {
    SetSenderSsrc(ByteReader<uint32_t>::ReadBigEndian(payload));
    csrcs_.resize(src_count - 1);
    for (size_t i = 1; i < src_count; ++i)
      csrcs_[i - 1] = ByteReader<uint32_t>::ReadBigEndian(&payload[4 * i]);
  }

  if (has_reason) {
    reason_.assign(reinterpret_cast<const char*>(&payload[4u * src_count + 1]),
                   reason_length);
  } else {
    reason_.clear();
  }
  return Result::Create();
}