#pragma once
#if defined(_MSC_VER)
#include <Winsock2.h>
#include <Ws2tcpip.h>
#else
#error "Unsupported compiler"
#endif

#include "../include/qosrtp_session.h"
#include "../utils/thread.h"
#include "../rtp_rtcp/rtp_rtcp_tranceiver.h"
#include "../rtp_rtcp/rtp_rtcp_router.h"
#include "../session/media_session.h"

namespace qosrtp {
class TransportAddressImpl : public TransportAddress {
 public:
  TransportAddressImpl();
  virtual ~TransportAddressImpl() override;
  virtual void AssignPort(uint16_t port) override;
  virtual void SetTransportProtocolType(TransportProtocolType type) override;
  virtual std::unique_ptr<Result> AssignIp(std::string ip) override;
  virtual std::string ip() const override;
  virtual uint16_t port() const override;
  virtual TransportProtocolType type() const override;
  virtual uint32_t ip_version() const override;
  virtual const in_addr& ipv4() const override;
  virtual const in6_addr& ipv6() const override;
  virtual void SaveToSockAddr(sockaddr* sock_addr) const override;

 private:
  bool ParseIpAddress(const std::string ip) {
    int result = 0;
#if defined(_MSC_VER)
    result = inet_pton(AF_INET, ip.c_str(), &ipv4_);
    if (result == 1) {
      ip_version_ = AF_INET;
    } else {
      result = inet_pton(AF_INET6, ip.c_str(), &ipv6_);
      if (result == 1) {
        ip_version_ = AF_INET6;
      }
    }
#else
#error "Unsupported compiler"
#endif
    return (result == 1);
  }
  std::string ip_;
  uint16_t port_;
  TransportProtocolType type_;
  uint32_t ip_version_;
  struct in_addr ipv4_;
  struct in6_addr ipv6_;
};

class RtxConfigImpl : public RtxConfig {
 public:
  RtxConfigImpl();
  virtual ~RtxConfigImpl() override;
  // must call
  virtual std::unique_ptr<Result> Configure(uint16_t max_cache_seq_difference,
                                            uint32_t ssrc) override;
  /**
   * Add the rtx pt and the corresponding rtp pt
   * If rtx_pt already exists, the previous setting will be overwritten.
   */
  virtual std::unique_ptr<Result> AddRtxAndAssociatedPayloadType(
      uint8_t rtx_pt, uint8_t associated_pt) override;
  virtual void DeleteRtxAndAssociatedPayloadType(uint8_t rtx_pt) override;
  virtual void ClearRtxPayload() override;
  /**
   * call
   * SetRtxAndAssociatedPayloadType\DeleteRtxAndAssociatedPayloadType\ClearRtxPayload
   * may change this return map
   */
  virtual const std::map<uint8_t, uint8_t>& map_rtx_payload_type() const override;
  virtual uint16_t max_cache_seq_difference() const override;
  virtual uint32_t ssrc() const override;

 private:
  uint16_t max_cache_seq_difference_;
  uint32_t ssrc_;
  std::map<uint8_t, uint8_t> map_rtx_payload_type_;
};

class MediaSessionConfigImpl : public MediaSessionConfig {
 public:
  MediaSessionConfigImpl();
  virtual ~MediaSessionConfigImpl() override;
  virtual std::unique_ptr<Result> Configure(
      uint32_t ssrc_media_local, const RtxConfig* rtx_config_local,
      const uint32_t* rtp_clock_rate_hz_local,
      const std::vector<uint8_t>* rtp_payload_types_local,
      uint32_t ssrc_media_remote, const RtxConfig* rtx_config_remote,
      const uint32_t* rtp_clock_rate_hz_remote,
      const std::vector<uint8_t>* rtp_payload_types_remote,
      const uint16_t* max_cache_duration_ms,
      MediaTransmissionDirection direction, int rtcp_report_interval_ms,
      MediaSessionCallback* callback) override;

  virtual uint32_t ssrc_media_local() const override;
  virtual const RtxConfig* rtx_config_local() const override;
  virtual uint32_t rtp_clock_rate_hz_local() const override;
  virtual const std::vector<uint8_t>& rtp_payload_types_local() const override;
  virtual uint32_t ssrc_media_remote() const override;
  virtual const RtxConfig* rtx_config_remote() const override;
  virtual uint32_t rtp_clock_rate_hz_remote() const override;
  virtual const std::vector<uint8_t>& rtp_payload_types_remote() const override;
  virtual uint16_t max_cache_duration_ms() const override;
  virtual MediaTransmissionDirection direction() const override;
  virtual int rtcp_report_interval_ms() const override;
  virtual MediaSessionCallback* callback() const override;

 private:
  bool AreAnySsrcsEqual(const std::vector<uint32_t>& ssrcs) {
    if (ssrcs.empty()) {
      return false;
    }
    for (size_t i = 1; i < ssrcs.size(); ++i) {
      uint32_t comparison_ssrc = ssrcs[i];
      for (size_t j = i + 1; j < ssrcs.size(); ++j) {
        if (ssrcs[j] == comparison_ssrc)
          return true;
      }
    }
    return false;
  }
  int rtcp_report_interval_ms_;
  uint32_t rtp_clock_rate_hz_local_;
  uint32_t rtp_clock_rate_hz_remote_;
  uint32_t ssrc_media_local_;
  uint32_t ssrc_media_remote_;
  MediaTransmissionDirection direction_;
  std::unique_ptr<RtxConfig> rtx_config_local_;
  std::unique_ptr<RtxConfig> rtx_config_remote_;
  std::vector<uint8_t> rtp_payload_types_local_;
  std::vector<uint8_t> rtp_payload_types_remote_;
  MediaSessionCallback* callback_;
  uint16_t max_cache_duration_ms_;
};

class QosrtpSessionConfigImpl : public QosrtpSessionConfig {
 public:
  QosrtpSessionConfigImpl();
  virtual ~QosrtpSessionConfigImpl() override;
  // must call
  virtual std::unique_ptr<Result> Configure(
      std::unique_ptr<TransportAddress> local_address,
      std::unique_ptr<TransportAddress> remote_address, std::string cname) override;
  /**
   * The caller must ensure that there is no ssrc conflict between
   * MediaSessionConfig, otherwise an unknown error will occur.
   */
  virtual void AddMediaSessionConfig(
      std::string name /*Identity of config*/,
      std::unique_ptr<MediaSessionConfig> config) override;
  virtual void DeleteMediaSessionConfig(std::string name) override;
  virtual void ClearMediaSessionConfig() override;

  virtual TransportAddress* address_local() const override;
  virtual TransportAddress* address_remote() const override;
  virtual const std::string& cname() const override;
  /* call AddMediaSessionConfig and DeleteMediaSessionConfig
   * may change this return map*/
  virtual const std::map<std::string, std::unique_ptr<MediaSessionConfig>>&
  map_media_session_config() const override;
  
 private:
  std::unique_ptr<TransportAddress> address_local_;
  std::unique_ptr<TransportAddress> address_remote_;
  std::map<std::string, std::unique_ptr<MediaSessionConfig>>
      map_media_session_config_;
  std::string cname_;
};

class QosrtpSessionImpl : public QosrtpSession {
 public:
  QosrtpSessionImpl();
  virtual ~QosrtpSessionImpl() override;
  virtual std::unique_ptr<Result> StartSession(
      std::unique_ptr<QosrtpSessionConfig> config) override;
  virtual void SendRtpPacket(std::unique_ptr<RtpPacket> pkt) override;

 private:
  std::unique_ptr<QosrtpSessionConfig> config_;
  std::unique_ptr<NetworkIoScheduler> scheduler_;
  std::unique_ptr<Thread> signaling_thread_;
  std::unique_ptr<Thread> worker_thread_;
  std::unique_ptr<Thread> network_thread_;
  std::unique_ptr<RtpRtcpRouter> router_;
  std::vector<std::unique_ptr<MediaSession>> media_sessions_;
  std::unique_ptr<RtpRtcpTranceiver> rtp_rtcp_tranceiver_;
  std::atomic<bool> has_started_;
};
}  // namespace qosrtp