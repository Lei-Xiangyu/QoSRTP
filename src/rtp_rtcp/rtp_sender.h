#pragma once
#include "./rtp_rtcp_tranceiver.h"
#include "../utils/ntp_time.h"
#include "../include/result.h"

#include <mutex>
#include <vector>

namespace qosrtp {
struct RtpSenderConfig {
  RtpSenderConfig();
  ~RtpSenderConfig();
  uint16_t max_cache_seq_difference;
  uint32_t local_ssrc;
  uint32_t rtp_clock_rate_hz;
  std::vector<uint8_t> rtp_payload_types;
  bool rtx_enabled;
  uint32_t rtx_ssrc;
  std::map<uint8_t, uint8_t> map_rtx_payload_type;
};

class RtpSenderCallback {
 protected:
  RtpSenderCallback();
  ~RtpSenderCallback();
};

/* This class is not thread-safe */
class RtpSenderPacketCache {
 public:
  RtpSenderPacketCache(uint16_t max_cache_seq_difference);
  ~RtpSenderPacketCache();
  /* It is required that the seq of the incoming packet is continuous */
  void PutPacket(std::unique_ptr<RtpPacket> packet);
  void GetPackets(const std::vector<uint16_t>& packet_seqs,
                  std::vector<std::unique_ptr<RtpPacket>>& out_packets);

 private:
  std::vector<std::unique_ptr<RtpPacket>> cached_rtp_packets_;
  uint16_t max_cache_seq_difference_;
};

class RtpSender {
 public:
  RtpSender();
  ~RtpSender();
  /* Call it first, otherwise it will lead to unexpected results */
  std::unique_ptr<Result> Initialize(RtpSenderCallback* sender_callback,
                                     RtpRtcpTranceiver* tranceiver,
                                     std::unique_ptr<RtpSenderConfig> config);
  void SendRtx(const std::vector<uint16_t>& packet_seqs);
  void SendRtp(std::unique_ptr<RtpPacket> packet);
  bool HasSentRtp() { return has_sent_.load(); }
  bool GetStatisticInfo(NtpTime& ntp_now, uint32_t& rtp_timestamp_now,
                        uint32_t& sender_packet_count,
                        uint32_t& sender_octet_count);

 private:
  struct RtxContext {
    RtxContext();
    ~RtxContext();
    uint16_t last_seq;
    bool has_sent;
  };
  std::unique_ptr<RtpPacket> ConstructRtx(std::unique_ptr<RtpPacket> packet);
  RtxContext rtx_context;
  RtpSenderCallback* sender_callback_;
  RtpRtcpTranceiver* tranceiver_;
  std::unique_ptr<RtpSenderConfig> config_;
  std::unique_ptr<RtpSenderPacketCache> cache_;
  std::atomic<bool> has_sent_;
  uint16_t last_seq_;
  std::mutex mutex_;
  uint32_t utc_ms_first_;
  uint32_t rtp_timestamp_first_;
  uint32_t rtp_clock_rate_;
  uint32_t sender_packet_count_;
  uint32_t sender_octet_count_;
};
}  // namespace qosrtp