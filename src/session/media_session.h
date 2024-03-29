#pragma once
#include "../include/qosrtp_session.h"
#include "../rtp_rtcp/rtcp_receiver.h"
#include "../rtp_rtcp/rtcp_sender.h"
#include "../rtp_rtcp/rtp_receiver.h"
#include "../rtp_rtcp/rtp_sender.h"
#include "../utils/thread.h"
#include "../rtp_rtcp/rtp_rtcp_tranceiver.h"
#include "../rtp_rtcp/rtp_rtcp_router.h"

namespace qosrtp {
class MediaSession : public RtcpReceiverCallback,
                     public RtcpSenderCallback,
                     public RtpReceiverCallback,
                     public RtpSenderCallback {
 public:
  MediaSession() = delete;
  MediaSession(const std::string& cname);
  ~MediaSession();
  // Assume the config is valid
  std::unique_ptr<Result> Initialize(const MediaSessionConfig* config,
                                     Thread* signal_thread,
                                     Thread* worker_thread,
                                     RtpRtcpTranceiver* rtp_rtcp_tranceiver,
                                     RtpRtcpRouter* rtp_rtcp_router);
  void SendRtpPacket(std::unique_ptr<RtpPacket> pkt);
  void SendBye();

  uint32_t GetLocalSsrc() { return config_->ssrc_media_local(); }

  /* RtcpReceiverCallback override*/
  virtual void NotifyByeReceived() override;
  virtual void NotifyNackReceived(
      const std::vector<uint16_t>& packet_seqs) override;

  /* RtcpSenderCallback override*/
  virtual bool HasSentRtp() override;
  virtual bool HasReceivedRtp() override;
  virtual bool HasReceivedBye() override;
  virtual std::unique_ptr<RemoteSenderInfo> GetRemoteSenderInfo() override;
  virtual std::unique_ptr<LocalSenderInfo> GetLocalSenderInfo() override;

  /* RtpReceiverCallback override*/
  virtual void NotifyLossPacketSeqsForNack(
      const std::vector<uint16_t>& loss_packet_seqs) override;
  // will run on signal thread
  virtual void OnRtpPacket(
      std::vector<std::unique_ptr<RtpPacket>> packets) override;

 private:
  const MediaSessionConfig* config_;
  RtpRtcpTranceiver* rtp_rtcp_tranceiver_;
  RtpRtcpRouter* rtp_rtcp_router_;
  Thread* worker_thread_;
  Thread* signal_thread_;
  const std::string cname_;
  std::unique_ptr<RtpSender> rtp_sender_;
  std::unique_ptr<RtcpSender> rtcp_sender_;
  std::unique_ptr<RtcpReceiver> rtcp_receiver_;
  std::unique_ptr<RtpReceiver> rtp_receiver_;
  std::atomic<bool> initialized_;
  std::atomic<bool> has_received_bye_;
  std::atomic<bool> has_received_rtp_;
  std::atomic<bool> has_sent_rtp_;
  std::mutex mutex_;
};
}  // namespace qosrtp