#pragma once
#include <vector>

#include "rtp_rtcp_router.h"
#include "./common_header.h"
#include "./sender_report.h"
#include "./receiver_report.h"
#include "./sdes.h"
#include "./bye.h"
#include "./nack.h"

namespace qosrtp {
class RtcpReceiverCallback {
 public:
  virtual void NotifyByeReceived() = 0;
  virtual void NotifyNackReceived(const std::vector<uint16_t>& packet_seqs) = 0;

 protected:
  RtcpReceiverCallback();
  ~RtcpReceiverCallback();
};

struct RtcpReceiverConfig {
  RtcpReceiverConfig();
  ~RtcpReceiverConfig();
  uint32_t remote_ssrc;
  uint32_t local_ssrc;
};

class RtcpReceiver : public RtcpRouterDst {
 public:
  RtcpReceiver();
  ~RtcpReceiver();
  /* Call it first, otherwise it will lead to unexpected results */
  std::unique_ptr<Result> Initialize(
      RtcpReceiverCallback* receiver_callback,
      std::unique_ptr<RtcpReceiverConfig> config);
  virtual void OnRtcpPacket(const DataBuffer* data_buffer) override;
  void GetSrInfo(uint32_t& lsr, uint32_t& dlsr);

 private:
  enum class RTCPPacketType : uint32_t {
    kRtcpReport = 0x0001,
    kRtcpSr = 0x0002,
    kRtcpRr = 0x0004,
    kRtcpSdes = 0x0008,
    kRtcpBye = 0x0010,
    kRtcpNack = 0x0020,
  };
  struct PacketInformation {
    PacketInformation();
    ~PacketInformation();
    uint32_t type_flags;
  };
  std::unique_ptr<PacketInformation> Parse(const DataBuffer* data_buffer);
  void ParseSenderReport(rtcp::CommonHeader* header, PacketInformation* info);
  void ParseReceiverReport(rtcp::CommonHeader* header, PacketInformation* info);
  void ParseSdes(rtcp::CommonHeader* header, PacketInformation* info);
  void ParseBye(rtcp::CommonHeader* header, PacketInformation* info);
  void ParseNacks(rtcp::CommonHeader* header, PacketInformation* info);
  std::unique_ptr<RtcpReceiverConfig> config_;
  RtcpReceiverCallback* receiver_callback_;
  bool has_received_bye_;
  std::mutex mutex_;
  bool has_received_sender_report_;
  NtpTime ntp_last_sender_report_;
  uint64_t ms_receive_last_sr_;
};
}  // namespace qosrtp