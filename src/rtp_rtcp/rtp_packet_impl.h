#pragma once
#include <iostream>
#include <memory>
#include <vector>

#include "../include/rtp_packet.h"
/*
  Rtp buffer
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           timestamp                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           synchronization source (SSRC) identifier            |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   |            contributing source (CSRC) identifiers             |
   |                             ....                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   Extension buffer
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      defined by profile       |           length              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        header extension                       |
   |                             ....                              |
*/
namespace qosrtp {
class RtpPacketImpl : public RtpPacket {
 public:
  RtpPacketImpl() {
    octet_m_and_payload_type_ = 0;
    sequence_number_ = 0;
    timestamp_ = 0;
    ssrc_ = 0;
    extension_ = nullptr;
    payload_buffer_ = nullptr;
    pad_size_ = 0;
  }
  virtual ~RtpPacketImpl();
  virtual std::unique_ptr<Result> StorePacket(const uint8_t* packet,
                                              uint32_t length) override;
  virtual std::unique_ptr<Result> StorePacket(
      uint8_t octet_m_and_payload_type, uint16_t sequence_number,
      uint32_t timestamp, uint32_t ssrc, const std::vector<uint32_t>& csrcs,
      std::unique_ptr<Extension> extension,
      std::unique_ptr<DataBuffer> payload_buffer, uint8_t pad_size) override;
  virtual void SetTimestamp(uint32_t timestamp) override;
  virtual void SetSequenceNumber(uint16_t seq) override;

  virtual std::unique_ptr<DataBuffer> LoadPacket() const override;
  virtual bool p() const override;
  virtual bool x() const override;
  virtual uint8_t count_csrcs() const override;
  virtual uint8_t m() const override;
  virtual uint8_t payload_type() const override;
  virtual uint16_t sequence_number() const override;
  virtual uint32_t timestamp() const override;
  virtual uint32_t ssrc() const override;
  virtual std::unique_ptr<Result> csrc(uint8_t& csrc,
                                       uint8_t index) const override;
  virtual const Extension* GetExtension() const override;
  virtual const DataBuffer* GetPayloadBuffer() const override;
  virtual uint8_t pad_size() const override;

 private:
  uint8_t octet_m_and_payload_type_;
  uint16_t sequence_number_;
  uint32_t timestamp_;
  uint32_t ssrc_;
  std::vector<uint32_t> csrcs_;
  std::unique_ptr<Extension> extension_;
  std::unique_ptr<DataBuffer> payload_buffer_;
  uint8_t pad_size_;
};
}  // namespace qosrtp