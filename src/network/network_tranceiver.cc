#include "network_tranceiver.h"

#include <sstream>

#include "../include/log.h"
#include "../rtp_rtcp/rtp_rtcp_demuxer.h"
#include "../session/session_states.h"
#include "../utils//time_utils.h"

namespace qosrtp {
NetworkTranceiver::NetworkTranceiver() = default;

NetworkTranceiver::~NetworkTranceiver() = default;

std::unique_ptr<NetworkTranceiver> NetworkTranceiver::Create(
    TransportProtocolType type) {
  if (TransportProtocolType::kUdp == type) {
    return std::make_unique<UdpNetworkTranceiver>();
  } else if (TransportProtocolType::kTcp == type) {
    QOSRTP_LOG(
        Warning,
        "Warning: The tcp type NetworkTranceiver has not been implemented yet");
    return nullptr;
  } else {
    QOSRTP_LOG(
        Warning,
        "Warning: When creating a NetworkTranceiver, type cannot be unknown");
    return nullptr;
  }
}

UdpNetworkTranceiver::UdpNetworkTranceiver()
    : scheduler_(nullptr),
      demuxer_(nullptr),
      events_(0),
#if defined(_MSC_VER)
      sockfd_(INVALID_SOCKET),
      local_address_(),
      remote_address_()
#endif
{
}

UdpNetworkTranceiver::~UdpNetworkTranceiver() = default;

std::unique_ptr<Result> UdpNetworkTranceiver::BuildSocketAndConnect(
    TransportAddress* local_address, TransportAddress* remote_address,
    NetworkIoScheduler* scheduler, RtpRtcpPacketDemuxer* demuxer) {
  if (nullptr == scheduler || nullptr == demuxer) {
    return Result::Create(-1, "scheduler and demuxer cannot be nullptr");
  }
  if ((TransportProtocolType::kUdp != local_address->type()) ||
      (TransportProtocolType::kUdp != remote_address->type())) {
    return Result::Create(-1, "The transport layer protocol must be udp");
  }
  if (local_address->ip_version() != remote_address->ip_version()) {
    return Result::Create(
        -1, "The version of source ip and target ip must be the same.");
  }
  if ((AF_INET != local_address->ip_version()) &&
      (AF_INET6 != local_address->ip_version())) {
    return Result::Create(
        -1, "The version of source ip and target ip must be ipv4 or ipv6");
  }
#if defined(_MSC_VER)
  sockfd_ = socket(local_address->ip_version(), SOCK_DGRAM, 0);
  if (sockfd_ == INVALID_SOCKET) {
    return Result::Create(-1, "Failed to create socket.");
  }
  u_long mode = 1;
  if (ioctlsocket(sockfd_, FIONBIO, &mode) == SOCKET_ERROR) {
    closesocket(sockfd_);
    std::stringstream result_description;
    result_description << "Error setting socket to non-blocking mode: %d"
                       << WSAGetLastError();
    return Result::Create(-1, result_description.str());
  }
#endif
  local_address->SaveToSockAddr(&local_address_);
  remote_address->SaveToSockAddr(&remote_address_);
  if (bind(sockfd_, &local_address_, sizeof(local_address_)) == SOCKET_ERROR) {
    closesocket(sockfd_);
    return Result::Create(-1, "Error binding socket");
  }
  events_ = static_cast<uint32_t>(NetworkIOEvent::kRead);
  // events_ = static_cast<uint32_t>(NetworkIOEvent::kRead) |
  //           static_cast<uint32_t>(NetworkIOEvent::kWrite) |
  //           static_cast<uint32_t>(NetworkIOEvent::kClose);
  scheduler_ = scheduler;
  demuxer_ = demuxer;
  scheduler_->AddHandler(this);
  return Result::Create();
}

uint32_t UdpNetworkTranceiver::GetRequestedEvents() { return events_; }

WSAEVENT UdpNetworkTranceiver::GetWSAEvent() const { return WSA_INVALID_EVENT; }

SOCKET UdpNetworkTranceiver::GetSocket() const { return sockfd_; }

void UdpNetworkTranceiver::Send(std::unique_ptr<DataBuffer> data_buffer,
                                bool is_bye) {
  int ret =
      sendto(sockfd_, reinterpret_cast<const char*>(data_buffer->Get()),
             data_buffer->size(), 0, &remote_address_, sizeof(remote_address_));
  if (ret == -1) {
    QOSRTP_LOG(Error, "Error sending data");
  }
  //QOSRTP_LOG(Trace, "Send data ok, utc ms now: %llu", UTCTimeMillis());
  if (is_bye) 
    SessionStates::GetInstance()->NotifyByeSent();
}

void UdpNetworkTranceiver::OnEvent(uint32_t ff, int err) {
  if ((0 == (ff & static_cast<uint32_t>(NetworkIOEvent::kRead)))) {
    QOSRTP_LOG(Warning, "Warning: Received an unexpected event with code");
    return;
  }
  //QOSRTP_LOG(Trace, "UdpNetworkTranceiver::OnEvent");
  std::vector<std::unique_ptr<DataBuffer>> received_buffers;
  for (;;) {
    int remote_address_size = sizeof(remote_address_);
    int ret =
        recvfrom(sockfd_, reinterpret_cast<char*>(recv_buffer_),
                 kDefaultBufferSize, 0, &remote_address_, &remote_address_size);
    if (ret == -1) {
      // QOSRTP_LOG(Error, "Error receiving data");
      break;
    }
    std::unique_ptr<DataBuffer> recv_buffer = DataBuffer::Create(ret);
    recv_buffer->SetSize(ret);
    recv_buffer->ModifyAt(0, recv_buffer_, ret);
    received_buffers.push_back(std::move(recv_buffer));
  }
  if (demuxer_) {
    demuxer_->OnData(std::move(received_buffers));
  }
  return;
}
}  // namespace qosrtp
