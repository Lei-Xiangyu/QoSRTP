#pragma once
#if defined(_MSC_VER)
#include <Winsock2.h>
#include <Ws2tcpip.h>
#else
#error "Unsupported compiler"
#endif
#include <map>
#include <memory>
#include <string>

#include "define.h"
#include "result.h"
#include "rtp_packet.h"

namespace qosrtp {
enum class QOSRTP_API TransportProtocolType { kUnknown, kUdp, kTcp };

enum class QOSRTP_API MediaTransmissionDirection {
  kSendRecv,
  kSendOnly,
  kRecvOnly
};

class QOSRTP_API TransportAddress {
 public:
  static std::unique_ptr<TransportAddress> Create(std::string ip, uint16_t port,
                                                  TransportProtocolType type);
  TransportAddress();
  virtual ~TransportAddress();
  virtual void SetTransportProtocolType(TransportProtocolType type) = 0;
  virtual void AssignPort(uint16_t port) = 0;
  virtual std::unique_ptr<Result> AssignIp(std::string ip) = 0;
  virtual std::string ip() const = 0;
  virtual uint16_t port() const = 0;
  virtual TransportProtocolType type() const = 0;
  virtual uint32_t ip_version() const = 0;
  virtual const in_addr& ipv4() const = 0;
  virtual const in6_addr& ipv6() const = 0;
  virtual void SaveToSockAddr(sockaddr* sock_addr) const = 0;
};

class QOSRTP_API RtxConfig {
 public:
  static std::unique_ptr<RtxConfig> Create();
  RtxConfig();
  virtual ~RtxConfig();
  // must call
  virtual std::unique_ptr<Result> Configure(uint16_t max_cache_seq_difference,
                                            uint32_t ssrc) = 0;
  /**
   * Add the rtx pt and the corresponding rtp pt
   * If rtx_pt already exists, the previous setting will be overwritten.
   */
  virtual std::unique_ptr<Result> AddRtxAndAssociatedPayloadType(
      uint8_t rtx_pt, uint8_t associated_pt) = 0;
  virtual void DeleteRtxAndAssociatedPayloadType(uint8_t rtx_pt) = 0;
  virtual void ClearRtxPayload() = 0;
  /**
   * call
   * SetRtxAndAssociatedPayloadType\DeleteRtxAndAssociatedPayloadType\ClearRtxPayload
   * may change this return map
   */
  virtual const std::map<uint8_t, uint8_t>& map_rtx_payload_type() const = 0;
  virtual uint16_t max_cache_seq_difference() const = 0;
  virtual uint32_t ssrc() const = 0;
};

class QOSRTP_API MediaSessionCallback {
 public:
  virtual void OnRtpPacket(std::vector<std::unique_ptr<RtpPacket>> packets) = 0;

 protected:
  MediaSessionCallback();
  ~MediaSessionCallback();
};

class QOSRTP_API MediaSessionConfig {
 public:
  static std::unique_ptr<MediaSessionConfig> Create();
  MediaSessionConfig();
  virtual ~MediaSessionConfig();

  virtual std::unique_ptr<Result> Configure(
      uint32_t ssrc_media_local, const RtxConfig* rtx_config_local,
      const uint32_t* rtp_clock_rate_hz_local,
      const std::vector<uint8_t>* rtp_payload_types_local,
      uint32_t ssrc_media_remote, const RtxConfig* rtx_config_remote,
      const uint32_t* rtp_clock_rate_hz_remote,
      const std::vector<uint8_t>* rtp_payload_types_remote,
      const uint16_t* max_cache_duration_ms,
      MediaTransmissionDirection direction, int rtcp_report_interval_ms,
      MediaSessionCallback* callback) = 0;

  virtual uint32_t ssrc_media_local() const = 0;
  virtual const RtxConfig* rtx_config_local() const = 0;
  virtual uint32_t rtp_clock_rate_hz_local() const = 0;
  virtual const std::vector<uint8_t>& rtp_payload_types_local() const = 0;

  virtual uint32_t ssrc_media_remote() const = 0;
  virtual const RtxConfig* rtx_config_remote() const = 0;
  virtual uint32_t rtp_clock_rate_hz_remote() const = 0;
  virtual const std::vector<uint8_t>& rtp_payload_types_remote() const = 0;
  virtual uint16_t max_cache_duration_ms() const = 0;

  virtual MediaTransmissionDirection direction() const = 0;

  virtual int rtcp_report_interval_ms() const = 0;
  virtual MediaSessionCallback* callback() const = 0;
};

class QOSRTP_API QosrtpSessionConfig {
 public:
  static constexpr char kDefaultCname[] = "QosRtpSession";
  static std::unique_ptr<QosrtpSessionConfig> Create();
  QosrtpSessionConfig();
  virtual ~QosrtpSessionConfig();
  // must call
  virtual std::unique_ptr<Result> Configure(
      std::unique_ptr<TransportAddress> local_address,
      std::unique_ptr<TransportAddress> remote_address, std::string cname) = 0;
  /**
   * The caller must ensure that there is no ssrc conflict between
   * MediaSessionConfig, otherwise an unknown error will occur.
   */
  virtual void AddMediaSessionConfig(
      std::string name /*Identity of config*/,
      std::unique_ptr<MediaSessionConfig> config) = 0;
  virtual void DeleteMediaSessionConfig(std::string name) = 0;
  virtual void ClearMediaSessionConfig() = 0;

  virtual TransportAddress* address_local() const = 0;
  virtual TransportAddress* address_remote() const = 0;
  virtual const std::string& cname() const = 0;
  /**
   * call AddMediaSessionConfig and DeleteMediaSessionConfig
   * may change this return map
   */
  virtual const std::map<std::string, std::unique_ptr<MediaSessionConfig>>&
  map_media_session_config() const = 0;
};

class QOSRTP_API QosrtpSession {
 public:
  static std::unique_ptr<QosrtpSession> Create();
  QosrtpSession();
  virtual ~QosrtpSession();
  virtual std::unique_ptr<Result> StartSession(
      std::unique_ptr<QosrtpSessionConfig> config) = 0;
  virtual void SendRtpPacket(std::unique_ptr<RtpPacket> pkt) = 0;
};
}  // namespace qosrtp