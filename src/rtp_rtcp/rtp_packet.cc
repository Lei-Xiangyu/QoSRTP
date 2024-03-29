#include "../include/rtp_packet.h"
#include "rtp_packet_impl.h"
#include "../include/log.h"

namespace qosrtp {
std::unique_ptr<RtpPacket> RtpPacket::Create() {
  return std::make_unique<RtpPacketImpl>();
}

std::unique_ptr<RtpPacket> RtpPacket::Create(const RtpPacket* other) {
  std::unique_ptr<RtpPacket> packet_return = std::make_unique<RtpPacketImpl>();
  uint8_t m_payload_type_octet = other->payload_type();
  m_payload_type_octet |= other->m() ? 0x80 : 0x00;
  std::vector<uint32_t> csrcs;
  uint8_t count_csrcs = other->count_csrcs();
  uint8_t csrc = 0;
  for (uint8_t i = 0; i < count_csrcs; i++) {
    std::unique_ptr<Result> result = other->csrc(csrc, i);
    if (!result->ok()) {
      QOSRTP_LOG(Error, "Failed to get csrc, because: %s.",
                 result->description().c_str());
      return nullptr;
    }
    csrcs.push_back(csrc);
  }
  std::unique_ptr<RtpPacket::Extension> extension_copy = nullptr;
  if (other->x()) {
    const RtpPacket::Extension* extension_src = other->GetExtension();
    extension_copy =
        std::make_unique<RtpPacket::Extension>(extension_src->length);
    memcpy(extension_copy->name, extension_src->name, 2);
    extension_copy->content->ModifyAt(
        0, extension_src->content->Get(),
        extension_copy->content->size());
  }
  std::unique_ptr<DataBuffer> payload_buffer_copy =
      other->GetPayloadBuffer()
          ? DataBuffer::Create(other->GetPayloadBuffer()->size())
          : nullptr;
  if (payload_buffer_copy) {
    payload_buffer_copy->SetSize(payload_buffer_copy->capacity());
    payload_buffer_copy->ModifyAt(0, other->GetPayloadBuffer()->Get(),
                                  other->GetPayloadBuffer()->size());
  }
  packet_return->StorePacket(m_payload_type_octet, other->sequence_number(),
                             other->timestamp(), other->ssrc(), csrcs,
                             std::move(extension_copy),
                             std::move(payload_buffer_copy), other->pad_size());
  return packet_return;
}

RtpPacket::RtpPacket() = default;

RtpPacket::~RtpPacket() = default;

RtpPacket::Extension::Extension(uint16_t length_content_dw)
    : content(std::move(DataBuffer::Create(sizeof(uint32_t) / sizeof(uint8_t) *
                                           length_content_dw))) {
  length = length_content_dw;
  content->SetSize(content->capacity());
  std::memset(name, 0, 2);
}
}  // namespace qosrtp
