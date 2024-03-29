#include "../include/qosrtp_session.h"
#include "qosrtp_session_impl.h"

using namespace qosrtp;

std::unique_ptr<TransportAddress> TransportAddress::Create(
    std::string ip, uint16_t port, TransportProtocolType type) {
  std::unique_ptr<TransportAddress> address =
      std::make_unique<TransportAddressImpl>();
  std::unique_ptr<Result> result = address->AssignIp(ip);
  if (!result->ok()) {
    return nullptr;
  }
  address->SetTransportProtocolType(type);
  address->AssignPort(port);
  return std::move(address);
}

TransportAddress::TransportAddress() = default;

TransportAddress::~TransportAddress() = default;

std::unique_ptr<RtxConfig> RtxConfig::Create() {
  return std::make_unique<RtxConfigImpl>();
}

RtxConfig::RtxConfig() = default;

RtxConfig::~RtxConfig() = default;

std::unique_ptr<MediaSessionConfig> MediaSessionConfig::Create() {
  return std::make_unique<MediaSessionConfigImpl>();
}

MediaSessionConfig::MediaSessionConfig() = default;

MediaSessionConfig::~MediaSessionConfig() = default;

std::unique_ptr<QosrtpSessionConfig> QosrtpSessionConfig::Create() {
  return std::make_unique<QosrtpSessionConfigImpl>();
}

QosrtpSessionConfig::QosrtpSessionConfig() = default;

QosrtpSessionConfig::~QosrtpSessionConfig() = default;

std::unique_ptr<QosrtpSession> QosrtpSession::Create() {
  return std::make_unique<QosrtpSessionImpl>();
}

MediaSessionCallback::MediaSessionCallback() = default;

MediaSessionCallback::~MediaSessionCallback() = default;

QosrtpSession ::QosrtpSession() = default;

QosrtpSession ::~QosrtpSession() = default;