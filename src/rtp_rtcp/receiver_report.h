#pragma once
#include <memory>
#include <vector>

#include "../include/result.h"
#include "common_header.h"
#include "report_block.h"
#include "rtcp_packet.h"

namespace qosrtp {
namespace rtcp {

// RTCP receiver report (RFC 3550).
//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P|    RC   |   PT=RR=201   |             length            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                     SSRC of packet sender                     |
//  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  |                         report block(s)                       |
//  |                            ....                               |
class ReceiverReport : public RtcpPacket {
 public:
  static constexpr uint8_t kPacketType = 201;
  static constexpr size_t kMaxNumberOfReportBlocks = 0x1f;
  ReceiverReport();
  virtual ~ReceiverReport() override;

  virtual uint32_t BlockLength() const override;

  virtual std::unique_ptr<Result> LoadPacket(
      uint8_t* packet, uint32_t* pos, uint32_t max_length) const override;
  const std::vector<const ReportBlock*> report_blocks() const {
    std::vector<const ReportBlock*> ret;
    for (auto& block : report_blocks_) {
      ret.push_back(block.get());
    }
    return ret;
  }

  std::unique_ptr<Result> StorePacket(const CommonHeader& packet);
  std::unique_ptr<Result> AddReportBlock(std::unique_ptr<ReportBlock> block);
  std::unique_ptr<Result> SetReportBlocks(
      std::vector<std::unique_ptr<ReportBlock>> blocks);

 private:
  static const size_t kRrBaseLength = 4;
  std::vector<std::unique_ptr<ReportBlock>> report_blocks_;
};
}  // namespace rtcp
}  // namespace qosrtp