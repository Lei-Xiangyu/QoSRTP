#pragma once
#include <vector>

#include "../include/qosrtp_session.h"
#include "../include/result.h"
#include "../include/rtp_packet.h"
#include "../network/network_io_scheduler.h"
#include "../utils/thread.h"
#include "./rtcp_packet.h"

namespace qosrtp {
class RtpRtcpTranceiverCallback {
 public:
  virtual void OnRtp(std::vector<std::unique_ptr<RtpPacket>> packets) = 0;
  virtual void OnRtcp(std::vector<std::unique_ptr<DataBuffer>> data_buffers) = 0;
  
 protected:
  RtpRtcpTranceiverCallback();
  ~RtpRtcpTranceiverCallback();
};

class RtpRtcpTranceiver {
 public:
  static std::unique_ptr<RtpRtcpTranceiver> Create(
      RtpRtcpTranceiverCallback* callback, Thread* network_thread,
      NetworkIoScheduler* scheduler, TransportAddress* local_address,
      TransportAddress* remote_address);
  RtpRtcpTranceiver();
  virtual ~RtpRtcpTranceiver();
  /* Both SendRtp and SendRtcp will automatically switch to run on the
   * network_thread set in the Create. */
  virtual void SendRtp(std::unique_ptr<RtpPacket> packet) = 0;
  virtual void SendRtcp(
      std::vector<std::unique_ptr<rtcp::RtcpPacket>> packets, bool is_bye = false) = 0;

 protected:
  virtual std::unique_ptr<Result> InitTranceiver(
      RtpRtcpTranceiverCallback* callback, Thread* network_thread,
      NetworkIoScheduler* scheduler, TransportAddress* local_address,
      TransportAddress* remote_address) = 0;
};
}  // namespace qosrtp