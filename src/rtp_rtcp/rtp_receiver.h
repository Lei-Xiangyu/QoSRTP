#pragma once
#include <list>
#include <vector>

#include "rtp_rtcp_router.h"

namespace qosrtp {
class RtpReceiverCallback {
 public:
  // The number in loss_packet_seqs must "increase" with index
  virtual void NotifyLossPacketSeqsForNack(
      const std::vector<uint16_t>& loss_packet_seqs) = 0;
  /* use std::move */
  virtual void OnRtpPacket(std::vector<std::unique_ptr<RtpPacket>> packets) = 0;

 protected:
  RtpReceiverCallback();
  ~RtpReceiverCallback();
};

struct RtpReceiverConfig {
  RtpReceiverConfig();
  ~RtpReceiverConfig();
  uint32_t remote_ssrc;
  uint32_t rtp_clock_rate_hz;
  std::vector<uint8_t> rtp_payload_types;
  uint16_t max_cache_duration_ms;
  bool rtx_enabled;
  uint16_t rtx_max_cache_seq_difference;
  uint32_t rtx_ssrc;
  std::map<uint8_t, uint8_t> map_rtx_payload_type;
};

class RtpReceiverPacketCache {
 public:
  RtpReceiverPacketCache(uint16_t max_cache_duration_ms);
  ~RtpReceiverPacketCache();
  void PutPacket(std::unique_ptr<RtpPacket> packet);
  //void PutFecPacket(std::unique_ptr<RtpPacket> packet);
  //void ReconstructPacketsFromFec();
  void GetPackets(std::vector<std::unique_ptr<RtpPacket>>& packets);
  // seq in loss_packet_seqs "increases" with index
  void GetLossPacketSeqsForNack(std::vector<uint16_t>& loss_packet_seqs);
  //void GetLossPacketSeqs(std::vector<uint16_t>& loss_packet_seqs);
  uint32_t extended_highest_seq() { return extended_highest_seq_; }
  uint32_t cumulative_packets_Loss() { return cumulative_packets_Loss_; }
  uint32_t extended_first_seq() { return extended_first_seq_; }

 private:
  static const uint16_t kNackIntervalMs = 50;
  void SupplementLossSeqs();
  struct CachedRTPPacket {
    CachedRTPPacket(std::unique_ptr<RtpPacket> param_packet,
                    uint64_t param_packet_timeout_time_utc_ms);
    ~CachedRTPPacket();
    std::unique_ptr<RtpPacket> packet;
    uint64_t packet_timeout_time_utc_ms;
  };
  // The smaller the index, the "bigger" the corresponding seq.
  std::list<std::unique_ptr<CachedRTPPacket>> cached_packets_;
  struct LossPacketSequenceNumber {
    LossPacketSequenceNumber(uint16_t param_seq);
    ~LossPacketSequenceNumber();
    uint16_t seq;
    bool notified;
    uint64_t last_notify_time_utc_ms;
  };
  // The smaller the index, the "bigger" the corresponding seq.
  std::list<std::unique_ptr<LossPacketSequenceNumber>> loss_seqs_;
  uint16_t max_cache_duration_ms_;
  uint16_t latest_callback_seq_;
  bool has_callback_packet_;
  uint32_t cumulative_packets_Loss_;
  uint32_t extended_highest_seq_;
  uint32_t extended_first_seq_;
  bool has_cached_packet_;
};

struct RtpReceiverStatistics {
  uint32_t remote_ssrc = 0;
  uint32_t cumulative_loss = 0;  // 24 bits
  uint32_t extended_seq_num = 0;
  uint32_t first_extended_seq_num = 0;
  uint32_t interarrival_jitter = 0;
};

class RtpReceiver : public RtpRouterDst {
 public:
  RtpReceiver();
  ~RtpReceiver();
  /* Call it first, otherwise it will lead to unexpected results */
  std::unique_ptr<Result> Initialize(RtpReceiverCallback* receiver_callback,
                                     std::unique_ptr<RtpReceiverConfig> config);
  std::unique_ptr<RtpReceiverStatistics> GetRtpReceiverStatistics();
  bool HasReceivedRtp() { return has_received_.load(); }

  /* RtpRouterDst override */
  virtual bool IsExpectedRemoteSsrc(uint32_t ssrc) const override;
  virtual void OnRtpPacket(std::unique_ptr<RtpPacket> packet) override;

 private:
  std::unique_ptr<RtpPacket> ReconstructRtpFromRtx(
      std::unique_ptr<RtpPacket> packet);
  RtpReceiverCallback* receiver_callback_;
  std::unique_ptr<RtpReceiverConfig> config_;
  std::unique_ptr<RtpReceiverPacketCache> packet_cache_;
  std::atomic<bool> has_received_;
  std::mutex mutex_;
  std::unique_ptr<RtpReceiverStatistics> rtp_receiver_statistics_;
  int32_t nb_received_expected_;
  int32_t nb_received_real_;
  struct {
    uint32_t last_rtp_timestamp = 0;
    uint64_t last_rtp_utc_ms = 0;
    uint32_t interarrival_jitter = 0;
  } interarrival_jitter_info;
};
}  // namespace qosrtp