#pragma once
#include <memory>
#include <vector>
#include <mutex>

#include "../include/rtp_packet.h"
#include "../include/data_buffer.h"
#include "../include/result.h"
#include "../utils/thread.h"
#include "./rtp_rtcp_tranceiver.h"

namespace qosrtp {
class RtpRouterDst {
 public:
  virtual bool IsExpectedRemoteSsrc(uint32_t ssrc) const = 0;
  virtual void OnRtpPacket(std::unique_ptr<RtpPacket> packet) = 0;

 protected:
  RtpRouterDst();
  ~RtpRouterDst();
};

class RtcpRouterDst {
 public:
  virtual void OnRtcpPacket(const DataBuffer* data_buffer) = 0;

 protected:
  RtcpRouterDst();
  ~RtcpRouterDst();
};

class RtpRtcpRouter : public RtpRtcpTranceiverCallback {
 public:
  RtpRtcpRouter() = delete;
  /* worker_thread cannot be nullptr. If it is nullptr, std::invalid_argument
   * will be thrown */
  RtpRtcpRouter(Thread* worker_thread);
  ~RtpRtcpRouter();

  void AddRtpDst(RtpRouterDst* dst);
  void RemoveRtpDst(RtpRouterDst* dst);
  void AddRtcpDst(RtcpRouterDst* dst);
  void RemoveRtcpDst(RtcpRouterDst* dst);

  /* RtpRtcpTranceiverCallback override */
  /* Both OnRtp and OnRtcp will automatically switch to run on the worker_thread
   * set in the constructor. */
  virtual void OnRtp(std::vector<std::unique_ptr<RtpPacket>> packets) override;
  virtual void OnRtcp(
      std::vector<std::unique_ptr<DataBuffer>> data_buffers) override;

 private:
  Thread* worker_thread_;
  std::mutex mutex_destinations_;
  std::vector<RtpRouterDst*> rtp_destinations_;
  std::vector<RtcpRouterDst*> rtcp_destinations_;
};
}  // namespace qosrtp