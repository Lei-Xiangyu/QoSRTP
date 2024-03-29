#pragma once
#include <vector>

#include "common_header.h"
#include "rtcp_packet.h"

namespace qosrtp {
namespace rtcp {

// Source Description (SDES) (RFC 3550).
//
//         0                   1                   2                   3
//         0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//        +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// header |V=2|P|    SC   |  PT=SDES=202  |             length            |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// chunk  |                          SSRC/CSRC_1                          |
//   1    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |                           SDES items                          |
//        |                              ...                              |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
// chunk  |                          SSRC/CSRC_2                          |
//   2    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//        |                           SDES items                          |
//        |                              ...                              |
//        +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//
// Canonical End-Point Identifier SDES Item (CNAME)
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |    CNAME=1    |     length    | user and domain name        ...
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

// Do not parse any sde items except cname
class Sdes : public RtcpPacket {
 public:
  struct Chunk {
    uint32_t ssrc;
    std::string cname;
  };
  static constexpr uint8_t kPacketType = 202;
  static constexpr size_t kMaxNumberOfChunks = 0x1f;

  Sdes();
  virtual ~Sdes() override;

  // Parse assumes header is already parsed and validated.
  std::unique_ptr<Result> StorePacket(const CommonHeader& packet);

  std::unique_ptr<Result> AddCName(uint32_t ssrc, std::string cname);

  const std::vector<Chunk>& chunks() const { return chunks_; }

  virtual uint32_t BlockLength() const override;

  virtual std::unique_ptr<Result> LoadPacket(
      uint8_t* packet, uint32_t* pos, uint32_t max_length) const override;

 private:
  const uint8_t kTerminatorTag = 0;
  const uint8_t kCnameTag = 1;
  uint32_t ChunkSize(const Sdes::Chunk& chunk);

  std::vector<Chunk> chunks_;
  uint32_t chunks_buffer_length_;
};
}  // namespace rtcp
}  // namespace qosrtp