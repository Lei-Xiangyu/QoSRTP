#include "./rtp_rtcp_tranceiver.h"

#include "./rtp_rtcp_tranceiver_impl.h"
#include "../include/log.h"

namespace qosrtp {
RtpRtcpTranceiverCallback::RtpRtcpTranceiverCallback() = default;

RtpRtcpTranceiverCallback::~RtpRtcpTranceiverCallback() = default;

RtpRtcpTranceiver::RtpRtcpTranceiver() = default;

RtpRtcpTranceiver::~RtpRtcpTranceiver() = default;

std::unique_ptr<RtpRtcpTranceiver> RtpRtcpTranceiver::Create(
    RtpRtcpTranceiverCallback* callback, Thread* network_thread,
    NetworkIoScheduler* scheduler, TransportAddress* local_address,
    TransportAddress* remote_address) {
  std::unique_ptr<RtpRtcpTranceiver> ret_tranceiver =
      std::make_unique<RtpRtcpTranceiverImpl>();
  std::unique_ptr<Result> result = ret_tranceiver->InitTranceiver(
      callback, network_thread, scheduler, local_address, remote_address);
  if (!result->ok()) {
    QOSRTP_LOG(Error, "Failed to initialize tranceiver, because: %s",
      result->description().c_str());
    return nullptr;
  }
  return ret_tranceiver;
}
}  // namespace qosrtp