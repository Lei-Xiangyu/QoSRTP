#pragma once
#include <memory>
#include <vector>

#include "../include/result.h"
#include "../utils/ntp_time.h"
#include "common_header.h"
#include "report_block.h"
#include "rtcp_packet.h"

namespace qosrtp {
namespace rtcp {

//    Sender report (SR) (RFC 3550).
//     0                   1                   2                   3
//     0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//    |V=2|P|    RC   |   PT=SR=200   |             length            |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  0 |                         SSRC of sender                        |
//    +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//  4 |              NTP timestamp, most significant word             |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  8 |             NTP timestamp, least significant word             |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 12 |                         RTP timestamp                         |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 16 |                     sender's packet count                     |
//    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 20 |                      sender's octet count                     |
// 24 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
class SenderReport : public RtcpPacket {
 public:
  static constexpr uint8_t kPacketType = 200;
  static constexpr size_t kMaxNumberOfReportBlocks = 0x1f;

  SenderReport();
  virtual ~SenderReport() override;

  virtual uint32_t BlockLength() const override;

  virtual std::unique_ptr<Result> LoadPacket(
      uint8_t* packet, uint32_t* pos, uint32_t max_length) const override;
  NtpTime ntp() const { return ntp_; }
  uint32_t rtp_timestamp() const { return rtp_timestamp_; }
  uint32_t sender_packet_count() const { return sender_packet_count_; }
  uint32_t sender_octet_count() const { return sender_octet_count_; }
  const std::vector<const ReportBlock*> report_blocks() const {
    std::vector<const ReportBlock*> ret;
    for (auto& block : report_blocks_) {
      ret.push_back(block.get());
    }
    return ret;
  }

  // StorePacket assumes header is already parsed and validated.
  std::unique_ptr<Result> StorePacket(const CommonHeader& packet);
  void SetNtp(NtpTime ntp) { ntp_ = ntp; }
  void SetRtpTimestamp(uint32_t rtp_timestamp) {
    rtp_timestamp_ = rtp_timestamp;
  }
  void SetPacketCount(uint32_t packet_count) {
    sender_packet_count_ = packet_count;
  }
  void SetOctetCount(uint32_t octet_count) {
    sender_octet_count_ = octet_count;
  }
  std::unique_ptr<Result> AddReportBlock(std::unique_ptr<ReportBlock> block);
  std::unique_ptr<Result> SetReportBlocks(
      std::vector<std::unique_ptr<ReportBlock>> blocks);
  void ClearReportBlocks() { report_blocks_.clear(); }

 private:
  static constexpr size_t kSenderBaseLength = 24;
  NtpTime ntp_;
  uint32_t rtp_timestamp_;
  uint32_t sender_packet_count_;
  uint32_t sender_octet_count_;
  std::vector<std::unique_ptr<ReportBlock>> report_blocks_;
};
}  // namespace rtcp
}  // namespace qosrtp