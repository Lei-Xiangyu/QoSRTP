#include "nack.h"

#include "../utils/byte_io.h"

using namespace qosrtp;
using namespace rtcp;

Nack::Nack() = default;

Nack::~Nack() = default;

uint32_t Nack::BlockLength() const {
  return kHeaderLength + kCommonFeedbackLength +
         packed_.size() * kNackItemLength;
}

void Nack::SetPacketIds(const uint16_t* nack_list, uint32_t length) {
  SetPacketIds(std::vector<uint16_t>(nack_list, nack_list + length));
}

void Nack::SetPacketIds(std::vector<uint16_t> nack_list) {
  packet_ids_ = std::move(nack_list);
  Pack();
}

void Nack::Pack() {
  auto it = packet_ids_.begin();
  const auto end = packet_ids_.end();
  while (it != end) {
    PackedNack item;
    item.first_pid = *it++;
    // Bitmask specifies losses in any of the 16 packets following the pid.
    item.bitmask = 0;
    while (it != end) {
      if ((*it) < item.first_pid) {
        break;
      }
      uint16_t shift = static_cast<uint16_t>(*it - item.first_pid - 1);
      if (shift <= 15) {
        item.bitmask |= (1 << shift);
        ++it;
      } else {
        break;
      }
    }
    packed_.push_back(item);
  }
}

void Nack::Unpack() {
  for (const PackedNack& item : packed_) {
    packet_ids_.push_back(item.first_pid);
    uint16_t pid = item.first_pid + 1;
    for (uint16_t bitmask = item.bitmask; bitmask != 0; bitmask >>= 1, ++pid) {
      if (bitmask & 1) packet_ids_.push_back(pid);
    }
  }
}

std::unique_ptr<Result> Nack::LoadPacket(uint8_t* packet, uint32_t* pos,
                                         uint32_t max_length) const {
  if (packed_.empty()) {
    return Result::Create(
        -1, "A nack package can only be generated when FCI is not empty.");
  }
  if (*pos + BlockLength() > max_length) {
    return Result::Create(-1,
                          "The remaining buffer space is less than the length "
                          "of the loaded packet");
  }
  uint32_t payload_size_32bits =
      (BlockLength() - kHeaderLength) / sizeof(uint32_t);
  CreateHeader(kFeedbackMessageType, kPacketType, payload_size_32bits, packet,
               pos);
  LoadCommonFeedback(packet + *pos);
  *pos += kCommonFeedbackLength;
  for (uint32_t nack_index = 0; nack_index < packed_.size(); ++nack_index) {
    const PackedNack& item = packed_[nack_index];
    ByteWriter<uint16_t>::WriteBigEndian(packet + *pos + 0, item.first_pid);
    ByteWriter<uint16_t>::WriteBigEndian(packet + *pos + 2, item.bitmask);
    *pos += kNackItemLength;
  }
  return Result::Create();
}

std::unique_ptr<Result> Nack::StorePacket(const CommonHeader& packet) {
  if (kPacketType != packet.type() || kFeedbackMessageType != packet.fmt()) {
    return Result::Create(
        -1, "The settings of PT and FMT are inconsistent with nack.");
  }
  if (packet.payload_size_bytes() < kCommonFeedbackLength + kNackItemLength) {
    return Result::Create(-1,
                          "The packet length is less than the minimum "
                          "effective length of the nack packet.");
  }
  size_t nack_items =
      (packet.payload_size_bytes() - kCommonFeedbackLength) / kNackItemLength;
  StoreCommonFeedback(packet.payload());
  const uint8_t* next_nack = packet.payload() + kCommonFeedbackLength;
  packet_ids_.clear();
  packed_.resize(nack_items);
  for (size_t index = 0; index < nack_items; ++index) {
    packed_[index].first_pid = ByteReader<uint16_t>::ReadBigEndian(next_nack);
    packed_[index].bitmask = ByteReader<uint16_t>::ReadBigEndian(next_nack + 2);
    next_nack += kNackItemLength;
  }
  Unpack();
  return Result::Create();
}