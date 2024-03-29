#include "./rtp_rtcp_tranceiver_impl.h"

#include "../include/log.h"
#include "../utils/time_utils.h"

namespace qosrtp {
RtpRtcpTranceiverImpl::RtpRtcpTranceiverImpl()
    : demuxer_(nullptr),
      network_tranceiver_(nullptr),
      network_thread_(nullptr) {}

RtpRtcpTranceiverImpl::~RtpRtcpTranceiverImpl() = default;

std::unique_ptr<Result> RtpRtcpTranceiverImpl::InitTranceiver(
    RtpRtcpTranceiverCallback* callback, Thread* network_thread,
    NetworkIoScheduler* scheduler, TransportAddress* local_address,
    TransportAddress* remote_address) {
  if (nullptr == network_thread) {
    return Result::Create(-1, "Network thread must not be nullptr");
  }
  network_thread_ = network_thread;
  demuxer_ = std::make_unique<RtpRtcpPacketDemuxer>(callback);
  network_tranceiver_ = NetworkTranceiver::Create(local_address->type());
  if (nullptr == network_tranceiver_) {
    return Result::Create(-1, "Failed to create network tranceiver");
  }
  std::unique_ptr<Result> result = network_tranceiver_->BuildSocketAndConnect(
      local_address, remote_address, scheduler, demuxer_.get());
  if (!result->ok()) {
    QOSRTP_LOG(Error, "Failed to initialize network tranceiver, because: %s",
               result->description().c_str());
    return Result::Create(-1, "Failed to initialize network tranceiver");
  }
  return Result::Create();
}

void RtpRtcpTranceiverImpl::SendRtp(std::unique_ptr<RtpPacket> packet) {
  if (!network_thread_->IsCurrent()) {
    network_thread_->PushTask(CallableWrapper::Wrap(
        &RtpRtcpTranceiverImpl::SendRtp, this, std::move(packet)));
    return;
  }
  if (nullptr == packet) return;
  std::unique_ptr<DataBuffer> data_buffer = packet->LoadPacket();
  network_tranceiver_->Send(std::move(data_buffer));
}

void RtpRtcpTranceiverImpl::SendRtcp(
    std::vector<std::unique_ptr<rtcp::RtcpPacket>> packets, bool is_bye) {
  if (!network_thread_->IsCurrent()) {
    network_thread_->PushTask(CallableWrapper::Wrap(
        &RtpRtcpTranceiverImpl::SendRtcp, this, std::move(packets), is_bye));
    return;
  }
  uint32_t compound_packet_size = 0;
  for (auto iter = packets.begin(); iter != packets.end(); iter++) {
    if (nullptr == (*iter)) {
      continue;
    }
    compound_packet_size += (*iter)->BlockLength();
  }
  if (0 == compound_packet_size) {
    return;
  }
  std::unique_ptr<DataBuffer> compound_packet_buffer =
      DataBuffer::Create(compound_packet_size);
  compound_packet_buffer->SetSize(compound_packet_size);
  uint32_t write_pos = 0;
  uint8_t* buffer_inner = compound_packet_buffer->GetW();
  std::unique_ptr<Result> load_result = nullptr;
  for (auto iter = packets.begin(); iter != packets.end(); iter++) {
    if (nullptr == (*iter)) {
      continue;
    }
    load_result =
        (*iter)->LoadPacket(buffer_inner, &write_pos, compound_packet_size);
    if (!load_result->ok()) {
      QOSRTP_LOG(Error, "Failed to load rtcp packet, because: %s",
                 load_result->description().c_str());
      return;
    }
  }
  if (write_pos != compound_packet_size) {
    QOSRTP_LOG(Error,
               "The actual size of the rtcp cache written (%u) is not equal to "
               "the calculated size(%u)",
               write_pos, compound_packet_size);
    return;
  }
  network_tranceiver_->Send(std::move(compound_packet_buffer));
  QOSRTP_LOG(Trace, "Send rtcp utc: %llu", UTCTimeMillis());
}
}  // namespace qosrtp