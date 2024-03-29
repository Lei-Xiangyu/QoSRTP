#pragma once
#include "./rtp_rtcp_tranceiver.h"
#include "../utils/thread.h"
#include "../utils/ntp_time.h"

#include <mutex>

namespace qosrtp {
struct RemoteSenderInfo {
  RemoteSenderInfo();
  ~RemoteSenderInfo();
  uint32_t remote_ssrc;
  uint32_t cumulative_loss;  // 24 bits
  uint32_t extended_seq_num;
  uint32_t first_extended_seq_num;
  uint32_t interarrival_jitter;
  uint32_t lsr;
  uint32_t dlsr;
};

struct LocalSenderInfo {
  LocalSenderInfo();
  ~LocalSenderInfo();
  NtpTime ntp_now;
  uint32_t rtp_timestamp_now;
  uint32_t sender_packet_count;
  uint32_t sender_octet_count;
};

class RtcpSenderCallback {
 public:
  virtual bool HasSentRtp() = 0;
  virtual bool HasReceivedRtp() = 0;
  virtual bool HasReceivedBye() = 0;
  virtual std::unique_ptr<RemoteSenderInfo> GetRemoteSenderInfo() = 0;
  virtual std::unique_ptr<LocalSenderInfo> GetLocalSenderInfo() = 0;
 protected:
  RtcpSenderCallback();
  ~RtcpSenderCallback();
};

struct RtcpSenderConfig {
  RtcpSenderConfig();
  ~RtcpSenderConfig();
  uint32_t local_ssrc;
  uint32_t remote_ssrc;
  std::string local_cname;
  uint32_t rtcp_report_interval_ms;
  MediaTransmissionDirection direction;
};

/* Schedule_thread must be stopped before destruction */
class RtcpSender {
 public:
  RtcpSender();
  ~RtcpSender();
  std::unique_ptr<Result> Initialize(Thread* schedule_thread,
                                     RtcpSenderCallback* sender_callback,
                                     RtpRtcpTranceiver* tranceiver,
                                     std::unique_ptr<RtcpSenderConfig> config);
  void SendBye();
  void SendNack(const std::vector<uint16_t>& nack_packet_seqs);

 private:
  struct SendRtcpInfo {
    SendRtcpInfo();
    ~SendRtcpInfo();
    bool bye;
    struct NackInfo {
      NackInfo() = delete;
      NackInfo(const std::vector<uint16_t>& seqs_para);
      ~NackInfo();
      const std::vector<uint16_t>& seqs;
    };
    std::unique_ptr<NackInfo> nack;
  };
  void ScheduleSendRtcp();
  /* TODO Further decoupling and enhancing scalability */
  void SendRtcp(SendRtcpInfo* send_rtcp_info);
  Thread* schedule_thread_;
  RtcpSenderCallback* sender_callback_;
  RtpRtcpTranceiver* tranceiver_;
  std::unique_ptr<RtcpSenderConfig> config_;
  /* These variables are guarded by the following mutex */
  std::mutex mutex_;
  bool has_sent_rtp_;
  bool has_received_rtp_;
  bool has_sent_bye_;
  bool has_sent_rtcp_;
  RemoteSenderInfo last_report_remote_sender_info;
  uint64_t utc_ms_next_send_;
};
}  // namespace qosrtp