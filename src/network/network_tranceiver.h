#pragma once
#include <memory>

#include "../include/data_buffer.h"
#include "../include/qosrtp_session.h"
#include "../include/result.h"
#include "./network_io_scheduler.h"

namespace qosrtp {
class RtpRtcpPacketDemuxer;

class NetworkTranceiver {
 public:
  static std::unique_ptr<NetworkTranceiver> Create(TransportProtocolType type);
  NetworkTranceiver();
  virtual ~NetworkTranceiver();
  virtual std::unique_ptr<Result> BuildSocketAndConnect(
      TransportAddress* local_address, TransportAddress* remote_address,
      NetworkIoScheduler* scheduler, RtpRtcpPacketDemuxer* demuxer) = 0;
  virtual void Send(std::unique_ptr<DataBuffer> data_buffer, bool is_bye = false) = 0;
};

class UdpNetworkTranceiver : public NetworkTranceiver, NetworkIOHandler {
 public:
  static constexpr int kDefaultBufferSize = 64 * 1024;
  UdpNetworkTranceiver();
  virtual ~UdpNetworkTranceiver() override;

  /* NetworkTranceiver override */
  virtual std::unique_ptr<Result> BuildSocketAndConnect(
      TransportAddress* local_address, TransportAddress* remote_address,
      NetworkIoScheduler* scheduler, RtpRtcpPacketDemuxer* demuxer) override;
  virtual void Send(std::unique_ptr<DataBuffer> data_buffer,
                    bool is_bye) override;

  /* NetworkIOHandler override */
  virtual uint32_t GetRequestedEvents() override;
  virtual void OnEvent(uint32_t ff, int err) override;
#if defined(_MSC_VER)
  virtual WSAEVENT GetWSAEvent() const override;
  virtual SOCKET GetSocket() const override;
#endif

 private:
  NetworkIoScheduler* scheduler_;
  RtpRtcpPacketDemuxer* demuxer_;
  uint32_t events_;
  uint8_t recv_buffer_[kDefaultBufferSize];
#if defined(_MSC_VER)
  sockaddr local_address_;
  sockaddr remote_address_;
  SOCKET sockfd_;
#endif
};
}  // namespace qosrtp