#include "app.h"

#include <sstream>

#include "../utils/byte_io.h"

using namespace qosrtp;
using namespace qosrtp::rtcp;

App::App() : sub_type_(0), name_(0), data_(nullptr) {}

App::~App() = default;

std::unique_ptr<Result> App::StorePacket(const CommonHeader& packet) {
  if (kPacketType != packet.type()) {
    return Result::Create(-1, "The type of rtcp is not app.");
  }
  if (packet.payload_size_bytes() < kAppBaseLength) {
    return Result::Create(-1, "Packet is too small to be a valid APP packet");
  }
  if (packet.payload_size_bytes() % 4 != 0) {
    return Result::Create(
        -1,
        "Packet payload must be 32 bits aligned to make a valid APP packet");
  }
  sub_type_ = packet.fmt();
  SetSenderSsrc(ByteReader<uint32_t>::ReadBigEndian(&packet.payload()[0]));
  name_ = ByteReader<uint32_t>::ReadBigEndian(&packet.payload()[4]);
  data_ = DataBuffer::Create(packet.payload_size_bytes() - kAppBaseLength);
  data_->SetSize(data_->capacity());
  data_->ModifyAt(0, packet.payload() + kAppBaseLength, data_->size());
  return Result::Create();
}

std::unique_ptr<Result> App::SetSubType(uint8_t subtype) {
  if (subtype > 0x1f) {
    return Result::Create(-1, "subtype must be less than 0x1f.");
  }
  sub_type_ = subtype;
  return Result::Create();
}

std::unique_ptr<Result> App::SetData(const uint8_t* data,
                                     uint32_t data_length) {
  if (!data || (data_length == 0)) {
    return Result::Create(-1, "data is nullptr of data_length is 0.");
  }
  if (0 != (data_length % 4)) {
    return Result::Create(-1, "Data must be 32 bits aligned.");
  }
  if (data_length > kMaxDataSize) {
    std::stringstream result_description;
    result_description << "App data size " << data_length
                       << " exceed maximum of " << kMaxDataSize << " bytes.";
    return Result::Create(-1, result_description.str());
  }
  data_ = DataBuffer::Create(data_length);
  data_->SetSize(data_length);
  data_->ModifyAt(0, data, data_length);
  return Result::Create();
}

uint32_t App::BlockLength() const {
  return kHeaderLength + kAppBaseLength + data_->size();
}

std::unique_ptr<Result> qosrtp::rtcp::App::LoadPacket(
    uint8_t* packet, uint32_t* pos, uint32_t max_length) const {
  if (*pos + BlockLength() > max_length) {
    return Result::Create(-1,
                          "The remaining buffer space is less than the length "
                          "of the loaded packet");
  }
  const size_t index_end = *pos + BlockLength();
  CreateHeader(sub_type_, kPacketType,
               (BlockLength() - kHeaderLength) / sizeof(uint32_t), packet, pos);
  ByteWriter<uint32_t>::WriteBigEndian(&packet[*pos + 0], sender_ssrc());
  ByteWriter<uint32_t>::WriteBigEndian(&packet[*pos + 4], name_);
  if (!data_) {
    memcpy(&packet[*pos + 8], data_->Get(), data_->size());
  }
  *pos += (8 + data_->size());
  return std::unique_ptr<Result>();
}
