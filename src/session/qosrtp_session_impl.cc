#include "./qosrtp_session_impl.h"

#include <sstream>

#include "../include/log.h"

namespace qosrtp {
TransportAddressImpl::TransportAddressImpl()
    : ip_(""),
      port_(5555),
      type_(TransportProtocolType::kUnknown),
      ip_version_(AF_UNSPEC),
      ipv4_(),
      ipv6_() {}

TransportAddressImpl::~TransportAddressImpl() = default;

void TransportAddressImpl::AssignPort(uint16_t port) { port_ = port; }

void TransportAddressImpl::SetTransportProtocolType(
    TransportProtocolType type) {
  type_ = type;
}

std::unique_ptr<Result> TransportAddressImpl::AssignIp(std::string ip) {
  if (!ParseIpAddress(ip)) {
    return Result::Create(-1, "Invalid ip address.");
  }
  ip_ = ip;
  return Result::Create();
}

std::string TransportAddressImpl::ip() const { return ip_; }

uint16_t TransportAddressImpl::port() const { return port_; }

TransportProtocolType TransportAddressImpl::type() const { return type_; }

uint32_t TransportAddressImpl::ip_version() const { return ip_version_; }

const in_addr& TransportAddressImpl::ipv4() const { return ipv4_; }

const in6_addr& TransportAddressImpl::ipv6() const { return ipv6_; }

void TransportAddressImpl::SaveToSockAddr(sockaddr* sock_addr) const {
  if (AF_INET == ip_version_) {
    sockaddr_in* address = reinterpret_cast<sockaddr_in*>(sock_addr);
    address->sin_family = AF_INET;
    address->sin_port = htons(port_);
    address->sin_addr = ipv4_;

  } else if (AF_INET6 == ip_version_) {
    sockaddr_in6* address = reinterpret_cast<sockaddr_in6*>(sock_addr);
    address->sin6_family = AF_INET6;
    address->sin6_port = htons(port_);
    address->sin6_addr = ipv6_;
  }
}

RtxConfigImpl::RtxConfigImpl() : ssrc_(0), max_cache_seq_difference_(0) {}

RtxConfigImpl::~RtxConfigImpl() = default;

std::unique_ptr<Result> RtxConfigImpl::Configure(
    uint16_t max_cache_seq_difference, uint32_t ssrc) {
  ssrc_ = ssrc;
  max_cache_seq_difference_ = max_cache_seq_difference;
  return Result::Create();
}

std::unique_ptr<Result> RtxConfigImpl::AddRtxAndAssociatedPayloadType(
    uint8_t rtx_pt, uint8_t associated_pt) {
  uint8_t max_payload_type = (0xff >> 1);
  if (rtx_pt > max_payload_type || associated_pt > max_payload_type) {
    return Result::Create(-1, "rtx_pt or associated_pt is too large.");
  }
  map_rtx_payload_type_[rtx_pt] = associated_pt;
  return Result::Create();
}

void RtxConfigImpl::DeleteRtxAndAssociatedPayloadType(uint8_t rtx_pt) {
  auto iter = map_rtx_payload_type_.find(rtx_pt);
  if (iter != map_rtx_payload_type_.end()) map_rtx_payload_type_.erase(iter);
}

void RtxConfigImpl::ClearRtxPayload() { map_rtx_payload_type_.clear(); }

const std::map<uint8_t, uint8_t>& RtxConfigImpl::map_rtx_payload_type() const {
  return map_rtx_payload_type_;
}

uint16_t RtxConfigImpl::max_cache_seq_difference() const {
  return max_cache_seq_difference_;
}

uint32_t RtxConfigImpl::ssrc() const { return ssrc_; }

MediaSessionConfigImpl::MediaSessionConfigImpl()
    : rtcp_report_interval_ms_(0),
      rtp_clock_rate_hz_local_(0),
      rtp_clock_rate_hz_remote_(0),
      ssrc_media_local_(0),
      ssrc_media_remote_(0),
      max_cache_duration_ms_(0),
      direction_(MediaTransmissionDirection::kSendRecv),
      rtx_config_local_(nullptr),
      rtx_config_remote_(nullptr),
      callback_(nullptr) {}

MediaSessionConfigImpl::~MediaSessionConfigImpl() = default;

std::unique_ptr<Result> MediaSessionConfigImpl::Configure(
  uint32_t ssrc_media_local, const RtxConfig* rtx_config_local,
  const uint32_t* rtp_clock_rate_hz_local,
  const std::vector<uint8_t>* rtp_payload_types_local,
  uint32_t ssrc_media_remote, const RtxConfig* rtx_config_remote,
  const uint32_t* rtp_clock_rate_hz_remote,
  const std::vector<uint8_t>* rtp_payload_types_remote,
  const uint16_t* max_cache_duration_ms, MediaTransmissionDirection direction,
  int rtcp_report_interval_ms, MediaSessionCallback* callback) {
  if (!callback) return Result::Create(-1, "callback cannot be empty");
  if (MediaTransmissionDirection::kRecvOnly != direction) {
    if (!rtp_clock_rate_hz_local || !rtp_payload_types_local) {
      return Result::Create(-1, "Incomplete parameters required for sending");
    }
    if (0 == *rtp_clock_rate_hz_local)
      return Result::Create(-1, "local rtp timestamp unit can not be 0 hz");
    if (rtp_payload_types_local->empty()) {
      return Result::Create(-1, "local rtp payload types can not be empty");
    }
    for (auto iter = rtp_payload_types_local->begin();
         iter != rtp_payload_types_local->end(); ++iter) {
      if (*iter > 0x7F) {
        return Result::Create(-1, "The pt value must be less than or equal to 0x7F");
      }
    }
  }
  if (MediaTransmissionDirection::kSendOnly != direction) {
    if (!rtp_clock_rate_hz_remote || !rtp_payload_types_remote ||
        !max_cache_duration_ms) {
      return Result::Create(-1, "Incomplete parameters required for receiving");
    }
    if (0 == rtp_clock_rate_hz_remote)
      return Result::Create(-1, "remote rtp timestamp unit can not be 0 hz");
    if (0 == *rtp_clock_rate_hz_remote)
      return Result::Create(-1, "remote rtp timestamp unit can not be 0 hz");
    if (rtp_payload_types_remote->empty()) {
      return Result::Create(-1, "remote rtp payload types can not be empty");
    }
    for (auto iter = rtp_payload_types_remote->begin();
         iter != rtp_payload_types_remote->end(); ++iter) {
      if (*iter > 0x7F) {
        return Result::Create(
            -1, "The pt value must be less than or equal to 0x7F");
      }
    }
  }
  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(ssrc_media_local);
  ssrcs.push_back(ssrc_media_remote);
  if (nullptr != rtx_config_local) {
    ssrcs.push_back(rtx_config_local->ssrc());
  }
  if (nullptr != rtx_config_remote) {
    ssrcs.push_back(rtx_config_remote->ssrc());
  }
  if (AreAnySsrcsEqual(ssrcs)) {
    return Result::Create(-1, "Duplicate ssrc exists.");
  }
  switch (direction) {
    case qosrtp::MediaTransmissionDirection::kSendRecv:
    case qosrtp::MediaTransmissionDirection::kSendOnly:
    case qosrtp::MediaTransmissionDirection::kRecvOnly:
      direction_ = direction;
      break;
    default:
      return Result::Create(-1, "direction, unknown value.");
      break;
  }
  ssrc_media_local_ = ssrc_media_local;
  ssrc_media_remote_ = ssrc_media_remote;
  if (rtx_config_local) {
    rtx_config_local_ = RtxConfig::Create();
    rtx_config_local_->Configure(rtx_config_local->max_cache_seq_difference(),
                                 rtx_config_local->ssrc());
    const std::map<uint8_t, uint8_t>& map_rtx_payload_type =
        rtx_config_local->map_rtx_payload_type();
    for (auto iter = map_rtx_payload_type.begin();
         iter != map_rtx_payload_type.end(); ++iter) {
      rtx_config_local_->AddRtxAndAssociatedPayloadType(iter->first,
                                                        iter->second);
    }
  }
  if (rtx_config_remote) {
    rtx_config_remote_ = RtxConfig::Create();
    rtx_config_remote_->Configure(rtx_config_remote->max_cache_seq_difference(),
                                  rtx_config_remote->ssrc());
    const std::map<uint8_t, uint8_t>& map_rtx_payload_type =
        rtx_config_remote->map_rtx_payload_type();
    for (auto iter = map_rtx_payload_type.begin();
         iter != map_rtx_payload_type.end(); ++iter) {
      rtx_config_remote_->AddRtxAndAssociatedPayloadType(iter->first,
                                                         iter->second);
    }
  }
  rtcp_report_interval_ms_ = rtcp_report_interval_ms;
  callback_ = callback;
  if (max_cache_duration_ms) 
    max_cache_duration_ms_ = *max_cache_duration_ms;
  if (rtp_clock_rate_hz_local)
    rtp_clock_rate_hz_local_ = *rtp_clock_rate_hz_local;
  if (rtp_payload_types_local)
    rtp_payload_types_local_ = *rtp_payload_types_local;
  if (rtp_clock_rate_hz_remote)
    rtp_clock_rate_hz_remote_ = *rtp_clock_rate_hz_remote;
  if (rtp_payload_types_remote)
    rtp_payload_types_remote_ = *rtp_payload_types_remote;
  return Result::Create();
}

uint32_t MediaSessionConfigImpl::ssrc_media_local() const {
  return ssrc_media_local_;
}

const RtxConfig* MediaSessionConfigImpl::rtx_config_local() const {
  return rtx_config_local_.get();
}

uint32_t MediaSessionConfigImpl::rtp_clock_rate_hz_local() const {
  return rtp_clock_rate_hz_local_;
}

const std::vector<uint8_t>& MediaSessionConfigImpl::rtp_payload_types_local()
    const {
  return rtp_payload_types_local_;
}

uint32_t MediaSessionConfigImpl::ssrc_media_remote() const {
  return ssrc_media_remote_;
}

const RtxConfig* MediaSessionConfigImpl::rtx_config_remote() const {
  return rtx_config_remote_.get();
}

uint32_t MediaSessionConfigImpl::rtp_clock_rate_hz_remote() const {
  return rtp_clock_rate_hz_remote_;
}

const std::vector<uint8_t>& MediaSessionConfigImpl::rtp_payload_types_remote()
    const {
  return rtp_payload_types_remote_;
}

int MediaSessionConfigImpl::rtcp_report_interval_ms() const {
  return rtcp_report_interval_ms_;
}

uint16_t MediaSessionConfigImpl::max_cache_duration_ms() const {
  return max_cache_duration_ms_;
}

MediaTransmissionDirection MediaSessionConfigImpl::direction() const {
  return direction_;
}

MediaSessionCallback* MediaSessionConfigImpl::callback() const {
  return callback_;
}

QosrtpSessionConfigImpl::QosrtpSessionConfigImpl()
    : address_local_(nullptr), address_remote_(nullptr), cname_("") {}

QosrtpSessionConfigImpl::~QosrtpSessionConfigImpl() = default;

std::unique_ptr<Result> QosrtpSessionConfigImpl::Configure(
  std::unique_ptr<TransportAddress> local_address,
    std::unique_ptr<TransportAddress> remote_address, std::string cname) {
  address_local_ = std::move(local_address);
  address_remote_ = std::move(remote_address);
  cname_ = cname;
  return Result::Create();
}

void QosrtpSessionConfigImpl::AddMediaSessionConfig(
    std::string name, std::unique_ptr<MediaSessionConfig> config) {
  map_media_session_config_[name] = std::move(config);
}

void QosrtpSessionConfigImpl::DeleteMediaSessionConfig(std::string name) {
  auto iter = map_media_session_config_.find(name);
  if (iter != map_media_session_config_.end())
    map_media_session_config_.erase(iter);
}

void QosrtpSessionConfigImpl::ClearMediaSessionConfig() {
  map_media_session_config_.clear();
}

const std::map<std::string, std::unique_ptr<MediaSessionConfig>>&
QosrtpSessionConfigImpl::map_media_session_config() const {
  return map_media_session_config_;
}

TransportAddress* QosrtpSessionConfigImpl::address_local() const {
  return address_local_.get();
}

TransportAddress* QosrtpSessionConfigImpl::address_remote() const {
  return address_remote_.get();
}

const std::string& QosrtpSessionConfigImpl::cname() const { return cname_; }

QosrtpSessionImpl::QosrtpSessionImpl()
    : config_(nullptr),
      signaling_thread_(nullptr),
      worker_thread_(nullptr),
      network_thread_(nullptr),
      scheduler_(nullptr),
      router_(nullptr),
      media_sessions_(),
      rtp_rtcp_tranceiver_(nullptr),
      has_started_(false) {}

QosrtpSessionImpl::~QosrtpSessionImpl() {
  if (network_thread_) network_thread_->Stop();
  if (signaling_thread_) signaling_thread_->Stop();
  if (worker_thread_) worker_thread_->Stop();
  media_sessions_.clear();
  router_.reset(nullptr);
  scheduler_.reset(nullptr);
  network_thread_.reset(nullptr);
  worker_thread_.reset(nullptr);
  signaling_thread_.reset(nullptr);
}

std::unique_ptr<Result> QosrtpSessionImpl::StartSession(
    std::unique_ptr<QosrtpSessionConfig> config) {
  if (has_started_.load())
    return Result::Create(-1, "Repeat start QosrtpSession");
  if (nullptr == config) {
    return Result::Create(-1, "config can not be nullptr");
  }
  config_ = std::move(config);
  const std::map<std::string, std::unique_ptr<MediaSessionConfig>>&
      media_session_configs = config_->map_media_session_config();
  std::stringstream result_description;
  scheduler_ = std::make_unique<NetworkIoScheduler>();
  signaling_thread_ = std::make_unique<Thread>("signaling thread", nullptr);
  worker_thread_ = std::make_unique<Thread>("worker thread", nullptr);
  network_thread_ =
      std::make_unique<Thread>("network thread", scheduler_.get());
  signaling_thread_->Start();
  worker_thread_->Start();
  network_thread_->Start();
  router_ = std::make_unique<RtpRtcpRouter>(worker_thread_.get());
  rtp_rtcp_tranceiver_ = RtpRtcpTranceiver::Create(
      router_.get(), network_thread_.get(), scheduler_.get(),
      config_->address_local(), config_->address_remote());
  if (rtp_rtcp_tranceiver_ == nullptr) {
    result_description << "Failed to create rtcp rtp tranceiver";
    goto failed;
  }
  for (auto iter_media_session_config = media_session_configs.begin();
       iter_media_session_config != media_session_configs.end();
       ++iter_media_session_config) {
    std::unique_ptr<MediaSession> media_session =
        std::make_unique<MediaSession>(config_->cname());
    std::unique_ptr<Result> result = media_session->Initialize(
        iter_media_session_config->second.get(), signaling_thread_.get(),
        worker_thread_.get(), rtp_rtcp_tranceiver_.get(), router_.get());
    if (!result->ok()) {
      result_description << "Failed to Initialize media session("
                         << iter_media_session_config->first
                         << "), because: " << result->description();
      goto failed;
    }
    media_sessions_.push_back(std::move(media_session));
  }
  has_started_.store(true);
  return Result::Create();
failed:
  network_thread_->Stop();
  signaling_thread_->Stop();
  worker_thread_->Stop();
  media_sessions_.clear();
  router_.reset(nullptr);
  scheduler_.reset(nullptr);
  network_thread_.reset(nullptr);
  worker_thread_.reset(nullptr);
  signaling_thread_.reset(nullptr);
  config_.reset(nullptr);
  return Result::Create(-1, result_description.str());
}

void QosrtpSessionImpl::SendRtpPacket(std::unique_ptr<RtpPacket> pkt) {
  if (!has_started_.load())
    return;
  for (auto iter_media_session = media_sessions_.begin();
       iter_media_session != media_sessions_.end(); ++iter_media_session) {
    if ((*iter_media_session)->GetLocalSsrc() == pkt->ssrc()) {
      (*iter_media_session)->SendRtpPacket(std::move(pkt));
      break;
    }
  }
}
}  // namespace qosrtp