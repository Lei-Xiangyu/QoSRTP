#pragma once
#include "rtp_rtcp_tranceiver.h"
#include "rtp_rtcp_demuxer.h"
#include "../network/network_tranceiver.h"

namespace qosrtp {
class RtpRtcpTranceiverImpl : public RtpRtcpTranceiver {
 public:
  RtpRtcpTranceiverImpl();
  virtual ~RtpRtcpTranceiverImpl() override;

  virtual void SendRtp(std::unique_ptr<RtpPacket> packet) override;
  virtual void SendRtcp(std::vector<std::unique_ptr<rtcp::RtcpPacket>> packets,
                        bool is_bye) override;

 protected:
  virtual std::unique_ptr<Result> InitTranceiver(
      RtpRtcpTranceiverCallback* callback, Thread* network_thread,
      NetworkIoScheduler* scheduler, TransportAddress* local_address,
      TransportAddress* remote_address) override;
  std::unique_ptr<RtpRtcpPacketDemuxer> demuxer_;
  std::unique_ptr<NetworkTranceiver> network_tranceiver_;
  Thread* network_thread_;
};
}  // namespace qosrtp