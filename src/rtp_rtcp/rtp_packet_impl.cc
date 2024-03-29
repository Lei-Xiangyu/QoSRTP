#include "rtp_packet_impl.h"

#include <limits>

#include "../utils/byte_io.h"

using namespace qosrtp;

RtpPacketImpl::~RtpPacketImpl() = default;

std::unique_ptr<Result> RtpPacketImpl::StorePacket(const uint8_t* packet,
                                                   uint32_t length) {
  if (length < kFixedBufferLength) {
    return Result::Create(-1, "Length less than fixed head length.");
  }
  const uint8_t* pos_fixed_head = packet;
  uint8_t version_parsed = ((*pos_fixed_head) >> 6);
  if (version_parsed != kVersion) {
    return Result::Create(-1, "The version of rtp must be 2.");
  }
  bool p_parsed = ((bool)((*pos_fixed_head) & (uint8_t)0x20));
  bool x_parsed = ((bool)((*pos_fixed_head) & (uint8_t)0x10));
  uint8_t cc_parsed = ((*pos_fixed_head) & (uint8_t)0x0f);
  uint32_t csrcs_length_byte = cc_parsed * sizeof(uint32_t);
  uint32_t length_parsed = kFixedBufferLength;
  const uint8_t* pos_csrcs_parsed = nullptr;
  if (cc_parsed > 0) {
    if ((length_parsed + csrcs_length_byte) > length) {
      return Result::Create(
          -1,
          "The incoming length is less than the parsed rtp packet length. "
          "Maybe you should check whether the cc is set correctly.");
    }
    pos_csrcs_parsed = pos_fixed_head + length_parsed;
    length_parsed += csrcs_length_byte;
  }
  const uint8_t* pos_extension_parsed = nullptr;
  std::unique_ptr<Extension> extension_parsed = nullptr;
  if (x_parsed) {
    if ((length_parsed + 4) > length) {
      return Result::Create(
          -1,
          "The incoming length is less than the parsed rtp packet length. "
          "Maybe you should check whether the x is set correctly.");
    }
    pos_extension_parsed = pos_fixed_head + length_parsed;
    uint16_t extension_length_dw_parsed =
        ByteReader<uint16_t>::ReadBigEndian(pos_extension_parsed + 2);
    uint32_t extension_length_byte_parsed = extension_length_dw_parsed * 4;
    if ((length_parsed + 4 + extension_length_byte_parsed) > length) {
      return Result::Create(
          -1,
          "The incoming length is less than the parsed rtp packet length. "
          "Maybe you should check whether the extension is set correctly.");
    }
    extension_parsed = std::make_unique<Extension>(extension_length_dw_parsed);
    std::memcpy(extension_parsed->name, pos_extension_parsed, 2);
    extension_parsed->length = extension_length_dw_parsed;
    extension_parsed->content->ModifyAt(0, pos_extension_parsed + 4,
                                        extension_length_byte_parsed);
    length_parsed += (4 + extension_length_byte_parsed);
  }
  uint8_t pad_size_parsed = 0;
  if (p_parsed) {
    if ((length_parsed + 1) > length) {
      return Result::Create(
          -1,
          "The incoming length is less than the parsed rtp packet length. "
          "Maybe you should check whether the p is set correctly.");
    }
    pad_size_parsed = *(packet + length - 1);
    if (p_parsed != (pad_size_parsed > 0)) {
      return Result::Create(-1, "p does not match pad_size.");
    }
    if ((length_parsed + pad_size_parsed) > length) {
      return Result::Create(
          -1,
          "The incoming length is less than the parsed rtp packet length. "
          "Maybe you should check whether the pad_size is set correctly.");
    }
  }
  uint32_t length_payload = length - (length_parsed + pad_size_parsed);
  const uint8_t* pos_payload_parsed = nullptr;
  if (length_payload > 0) {
    pos_payload_parsed = packet + length_parsed;
  }
  octet_m_and_payload_type_ = *(pos_fixed_head + 1);
  sequence_number_ = ByteReader<uint16_t>::ReadBigEndian(pos_fixed_head + 2);
  timestamp_ = ByteReader<uint32_t>::ReadBigEndian(pos_fixed_head + 4);
  ssrc_ = ByteReader<uint32_t>::ReadBigEndian(pos_fixed_head + 8);
  csrcs_.clear();
  for (int i = 0; i < cc_parsed; i++) {
    csrcs_.push_back(*(reinterpret_cast<const uint32_t*>(
        pos_csrcs_parsed + i * sizeof(uint32_t))));
  }
  extension_ = std::move(extension_parsed);
  payload_buffer_ = nullptr;
  if (length_payload > 0) {
    payload_buffer_ = DataBuffer::Create(length_payload);
    payload_buffer_->SetSize(length_payload);
    payload_buffer_->ModifyAt(0, pos_payload_parsed, length_payload);
    pad_size_ = pad_size_parsed;
  }
  return Result::Create();
}

std::unique_ptr<Result> RtpPacketImpl::StorePacket(
    uint8_t octet_m_and_payload_type, uint16_t sequence_number,
    uint32_t timestamp, uint32_t ssrc, const std::vector<uint32_t>& csrcs,
    std::unique_ptr<Extension> extension,
    std::unique_ptr<DataBuffer> payload_buffer, uint8_t pad_size) {
  uint64_t buffer_length = kFixedBufferLength;
  buffer_length += csrcs.size() * sizeof(uint32_t);
  if (extension) {
    buffer_length += (4 + extension->length * 4);
  }
  if (payload_buffer) {
    buffer_length += payload_buffer->size();
  }
  buffer_length += pad_size;
  if (buffer_length > std::numeric_limits<uint32_t>::max()) {
    return Result::Create(
        -1, "The length exceeds the upper limit allowed by uint32_t.");
  }
  octet_m_and_payload_type_ = octet_m_and_payload_type;
  sequence_number_ = sequence_number;
  timestamp_ = timestamp;
  ssrc_ = ssrc;
  csrcs_ = csrcs;
  extension_ = std::move(extension);
  payload_buffer_ = std::move(payload_buffer);
  pad_size_ = pad_size;
  return Result::Create();
}

void RtpPacketImpl::SetTimestamp(uint32_t timestamp) { timestamp_ = timestamp; }

void RtpPacketImpl::SetSequenceNumber(uint16_t seq) { sequence_number_ = seq; }

std::unique_ptr<DataBuffer> RtpPacketImpl::LoadPacket() const {
  uint64_t buffer_length = kFixedBufferLength;
  buffer_length += csrcs_.size() * sizeof(uint32_t);
  if (extension_) {
    buffer_length += (4 + extension_->length * 4);
  }
  if (payload_buffer_) {
    buffer_length += payload_buffer_->size();
  }
  buffer_length += pad_size_;

  std::unique_ptr<DataBuffer> buffer =
      std::move(DataBuffer::Create(buffer_length));
  buffer->SetSize(buffer_length);
  uint32_t write_pos = 0;
  uint8_t tmpt_copy = 0x80;
  if (extension_) {
    tmpt_copy |= 0x20;
  }
  if (pad_size_ > 0) {
    tmpt_copy |= 0x10;
  }
  tmpt_copy |= (uint8_t)csrcs_.size();
  buffer->ModifyAt(write_pos++, &tmpt_copy, 1);
  buffer->ModifyAt(write_pos++, &octet_m_and_payload_type_, 1);
  for (int i = sizeof(uint16_t) - 1; i >= 0; i--) {
    buffer->ModifyAt(write_pos++,
                     reinterpret_cast<const uint8_t*>(&sequence_number_) + i,
                     1);
  }
  for (int i = sizeof(uint32_t) - 1; i >= 0; i--) {
    buffer->ModifyAt(write_pos++,
                     reinterpret_cast<const uint8_t*>(&timestamp_) + i, 1);
  }
  for (int i = sizeof(uint32_t) - 1; i >= 0; i--) {
    buffer->ModifyAt(write_pos++, reinterpret_cast<const uint8_t*>(&ssrc_) + i,
                     1);
  }
  for (auto iter_csrc = csrcs_.begin(); iter_csrc != csrcs_.end();
       iter_csrc++) {
    for (int i = sizeof(uint32_t) - 1; i >= 0; i--) {
      buffer->ModifyAt(write_pos++,
                       reinterpret_cast<const uint8_t*>(&(*iter_csrc)) + i, 1);
    }
  }
  if (extension_) {
    buffer->ModifyAt(write_pos, extension_->name, 2);
    write_pos += 2;
    for (int i = sizeof(uint16_t) - 1; i >= 0; i--) {
      buffer->ModifyAt(write_pos++,
                       reinterpret_cast<uint8_t*>(&(extension_->length)) + i,
                       1);
    }
    buffer->ModifyAt(write_pos, extension_->content->Get(),
                     extension_->content->size());
    write_pos += extension_->content->size();
  }
  if (payload_buffer_) {
    buffer->ModifyAt(write_pos, payload_buffer_->Get(),
                     payload_buffer_->size());
    write_pos += payload_buffer_->size();
  }
  if (pad_size_ > 0) {
    buffer->MemSet(write_pos, 0, pad_size_ - 1);
    write_pos += (pad_size_ - 1);
    buffer->MemSet(write_pos++, pad_size_, 1);
  }
  return buffer;
}

bool RtpPacketImpl::p() const { return (pad_size_ > 0); }

bool RtpPacketImpl::x() const { return (extension_ != nullptr); }

uint8_t RtpPacketImpl::count_csrcs() const { return csrcs_.size(); }

uint8_t RtpPacketImpl::m() const {
  return (octet_m_and_payload_type_ >> kBitsizePayloadType);
}

uint8_t RtpPacketImpl::payload_type() const {
  return ((((uint8_t)0xff) >> (8 - kBitsizePayloadType)) &
          octet_m_and_payload_type_);
}

uint16_t RtpPacketImpl::sequence_number() const { return sequence_number_; }

uint32_t RtpPacketImpl::timestamp() const { return timestamp_; }

uint32_t RtpPacketImpl::ssrc() const { return ssrc_; }

std::unique_ptr<Result> RtpPacketImpl::csrc(uint8_t& csrc,
                                            uint8_t index) const {
  if (index > csrcs_.size()) {
    return Result::Create(
        -1, "Accessed subscript exceeds maximum length of vector.");
  }
  csrc = *(csrcs_.begin() + index);
  return Result::Create();
}

const RtpPacket::Extension* RtpPacketImpl::GetExtension() const {
  return (extension_ ? extension_.get() : nullptr);
}

const DataBuffer* RtpPacketImpl::GetPayloadBuffer() const {
  return (payload_buffer_ ? payload_buffer_.get() : nullptr);
}

uint8_t RtpPacketImpl::pad_size() const { return pad_size_; }