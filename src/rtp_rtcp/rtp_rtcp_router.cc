#include "./rtp_rtcp_router.h"

#include <algorithm>
#include <functional>
#include <exception>

#include "../include/log.h"
#include "../utils/time_utils.h"

namespace qosrtp {
RtpRouterDst::RtpRouterDst() = default;

RtpRouterDst::~RtpRouterDst() = default;

RtcpRouterDst::RtcpRouterDst() = default;

RtcpRouterDst::~RtcpRouterDst() = default;

RtpRtcpRouter::RtpRtcpRouter(Thread* worker_thread) {
  if (nullptr == worker_thread)
    throw std::invalid_argument("worker_thread must not be nulltr");
  worker_thread_ = worker_thread;
}

RtpRtcpRouter::~RtpRtcpRouter() = default;

void RtpRtcpRouter::AddRtpDst(RtpRouterDst* dst) {
  std::lock_guard<std::mutex> lock(mutex_destinations_);
  if (rtp_destinations_.end() !=
      std::find(rtp_destinations_.begin(), rtp_destinations_.end(), dst)) {
    return;
  }
  rtp_destinations_.push_back(dst);
}

void RtpRtcpRouter::RemoveRtpDst(RtpRouterDst* dst) {
  std::lock_guard<std::mutex> lock(mutex_destinations_);
  auto iter =
      std::find(rtp_destinations_.begin(), rtp_destinations_.end(), dst);
  if (rtp_destinations_.end() == iter) {
    return;
  }
  rtp_destinations_.erase(iter);
}

void RtpRtcpRouter::AddRtcpDst(RtcpRouterDst* dst) {
  std::lock_guard<std::mutex> lock(mutex_destinations_);
  if (rtcp_destinations_.end() !=
      std::find(rtcp_destinations_.begin(), rtcp_destinations_.end(), dst)) {
    return;
  }
  rtcp_destinations_.push_back(dst);
}

void RtpRtcpRouter::RemoveRtcpDst(RtcpRouterDst* dst) {
  std::lock_guard<std::mutex> lock(mutex_destinations_);
  auto iter =
      std::find(rtcp_destinations_.begin(), rtcp_destinations_.end(), dst);
  if (rtcp_destinations_.end() == iter) {
    return;
  }
  rtcp_destinations_.erase(iter);
}

int Test(int a) { return a; }

void RtpRtcpRouter::OnRtp(std::vector<std::unique_ptr<RtpPacket>> packets) {
  if (packets.empty()) {
    return;
  }
  if (!worker_thread_->IsCurrent()) {
    worker_thread_->PushTask(
        CallableWrapper::Wrap(&RtpRtcpRouter::OnRtp, this, std::move(packets)));
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_destinations_);
  uint64_t trace_begin = UTCTimeMillis();
  for (auto iter = packets.begin(); iter != packets.end(); ++iter) {
    std::unique_ptr<RtpPacket> packet = std::move(*iter);
    for (auto& dst : rtp_destinations_) {
      if (!dst->IsExpectedRemoteSsrc(packet->ssrc())) continue;
      dst->OnRtpPacket(std::move(packet));
      break;
    }
  }
  uint64_t trace_end = UTCTimeMillis();
  QOSRTP_LOG(Trace, "RtpReceiver::OnRtpPacket cost: %lld ms, nb: %u",
             trace_end - trace_begin, (uint32_t)packets.size());
}

void RtpRtcpRouter::OnRtcp(
    std::vector<std::unique_ptr<DataBuffer>> data_buffers) {
  if (data_buffers.empty()) {
    return;
  }
  if (!worker_thread_->IsCurrent()) {
    worker_thread_->PushTask(CallableWrapper::Wrap(&RtpRtcpRouter::OnRtcp, this,
                                                   std::move(data_buffers)));
    return;
  }
  std::lock_guard<std::mutex> lock(mutex_destinations_);
  for (auto iter = data_buffers.begin(); iter != data_buffers.end(); ++iter) {
    std::unique_ptr<DataBuffer> data_buffer = std::move(*iter);
    for (auto& dst : rtcp_destinations_) {
      dst->OnRtcpPacket(data_buffer.get());
    }
  }
}
}  // namespace qosrtp