#pragma once
#include <iostream>
#include <memory>
#include <vector>

#include "define.h"
#include "data_buffer.h"
#include "result.h"
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
class QOSRTP_API RtpPacket {
 public:
  struct Extension {
    uint8_t name[2];
    uint16_t length;  // The unit is four bytes
    std::unique_ptr<DataBuffer> content;
    Extension() = delete;
    Extension(uint16_t length_content_dw);
  };

  static constexpr uint8_t kVersion = 2;
  static constexpr uint8_t kFixedBufferLength = 12;
  static constexpr uint8_t kBitsizePayloadType = 7;

  static std::unique_ptr<RtpPacket> Create();
  static std::unique_ptr<RtpPacket> Create(const RtpPacket* other);

  RtpPacket();
  virtual ~RtpPacket();

  virtual std::unique_ptr<Result> StorePacket(const uint8_t* packet,
                                              uint32_t length) = 0;
  virtual std::unique_ptr<Result> StorePacket(
      uint8_t octet_m_and_payload_type, uint16_t sequence_number,
      uint32_t timestamp, uint32_t ssrc, const std::vector<uint32_t>& csrcs,
      std::unique_ptr<Extension> extension,
      std::unique_ptr<DataBuffer> payload_buffer, uint8_t pad_size) = 0;
  virtual void SetTimestamp(uint32_t timestamp) = 0;
  virtual void SetSequenceNumber(uint16_t seq) = 0;

  virtual std::unique_ptr<DataBuffer> LoadPacket() const = 0;
  virtual bool p() const = 0;
  virtual bool x() const = 0;
  virtual uint8_t count_csrcs() const = 0;
  virtual uint8_t m() const = 0;
  virtual uint8_t payload_type() const = 0;
  virtual uint16_t sequence_number() const = 0;
  virtual uint32_t timestamp() const = 0;
  virtual uint32_t ssrc() const = 0;
  virtual std::unique_ptr<Result> csrc(uint8_t& csrc, uint8_t index) const = 0;
  virtual uint8_t pad_size() const = 0;
  virtual const Extension* GetExtension() const = 0;
  virtual const DataBuffer* GetPayloadBuffer() const = 0;
};
}  // namespace qosrtp