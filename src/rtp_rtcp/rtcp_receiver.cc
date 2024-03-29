#include "rtcp_receiver.h"

#include "../include/log.h"
#include "../utils/time_utils.h"

namespace qosrtp {
RtcpReceiverCallback::RtcpReceiverCallback() = default;

RtcpReceiverCallback::~RtcpReceiverCallback() = default;

RtcpReceiverConfig::RtcpReceiverConfig() : remote_ssrc(0), local_ssrc(0) {}

RtcpReceiverConfig::~RtcpReceiverConfig() = default;

RtcpReceiver::RtcpReceiver()
    : config_(),
      receiver_callback_(nullptr),
      has_received_bye_(false),
      has_received_sender_report_(false),
      ntp_last_sender_report_(0),
      ms_receive_last_sr_(0) {}

RtcpReceiver::~RtcpReceiver() = default;

RtcpReceiver::PacketInformation::PacketInformation() : type_flags(0) {}

RtcpReceiver::PacketInformation::~PacketInformation() = default;

std::unique_ptr<Result> RtcpReceiver::Initialize(
    RtcpReceiverCallback* receiver_callback,
    std::unique_ptr<RtcpReceiverConfig> config) {
  if ((nullptr == receiver_callback) || (nullptr == config)) {
    return Result::Create(-1, "Parameter cannot be a nullptr");
  }
  receiver_callback_ = receiver_callback;
  config_ = std::move(config);
  return Result::Create();
}

void RtcpReceiver::OnRtcpPacket(const DataBuffer* data_buffer) {
  if ((nullptr == data_buffer) || has_received_bye_) return;
  std::unique_ptr<PacketInformation> info = Parse(data_buffer);
  if (nullptr == info) {
    QOSRTP_LOG(Error, "Failed to parse rtcp information");
    return;
  }
  return;
}

std::unique_ptr<RtcpReceiver::PacketInformation> RtcpReceiver::Parse(
    const DataBuffer* data_buffer) {
  std::unique_ptr<rtcp::CommonHeader> compound_packet =
      std::make_unique<rtcp::CommonHeader>();
  const uint8_t* next_packet = data_buffer->Get();
  std::unique_ptr<PacketInformation> info =
      std::make_unique<PacketInformation>();
  for (;;) {
    uint32_t remain_length =
        data_buffer->size() - (next_packet - data_buffer->Get());
    if (0 == remain_length) {
      break;
    }
    std::unique_ptr<Result> result =
        compound_packet->StorePacket(next_packet, remain_length);
    if (!result->ok()) {
      QOSRTP_LOG(Error, "Failed to parse compound header, because: %s",
                 result->description().c_str());
      return nullptr;
    }
    next_packet = compound_packet->NextPacket();
    switch (compound_packet->type()) {
      case rtcp::SenderReport::kPacketType:
        ParseSenderReport(compound_packet.get(), info.get());
        break;
      case rtcp::ReceiverReport::kPacketType:
        ParseReceiverReport(compound_packet.get(), info.get());
        break;
      case rtcp::Sdes::kPacketType:
        ParseSdes(compound_packet.get(), info.get());
        break;
      case rtcp::Bye::kPacketType:
        ParseBye(compound_packet.get(), info.get());
        break;
      case rtcp::Rtpfb::kPacketType:
        switch (compound_packet->fmt()) { 
        case rtcp::Nack::kFeedbackMessageType:
          ParseNacks(compound_packet.get(), info.get());
          break;
        }
        break;
      default:
        break;
    }
  }
  return info;
}

void RtcpReceiver::ParseSenderReport(rtcp::CommonHeader* header,
                                     PacketInformation* info) {
  std::unique_ptr<rtcp::SenderReport> sr_packet =
      std::make_unique<rtcp::SenderReport>();
  std::unique_ptr<Result> result = sr_packet->StorePacket(*header);
  if (!(result->ok())) {
    QOSRTP_LOG(Error, "Failed to parse sender report, because: %s",
               result->description().c_str());
    return;
  }
  if (config_->remote_ssrc != sr_packet->sender_ssrc()) {
    QOSRTP_LOG(Error,
               "Failed to parse sender report, because: the ssrc in the "
               "package is not equal to the remote ssrc");
    return;
  }
  info->type_flags |= static_cast<uint32_t>(RTCPPacketType::kRtcpSr);
  const std::vector<const rtcp::ReportBlock*> report_blocks =
      sr_packet->report_blocks();
  if (!(report_blocks.empty())) {
    info->type_flags |= static_cast<uint32_t>(RTCPPacketType::kRtcpReport);
  }
  std::lock_guard<std::mutex> lock(mutex_);
  has_received_sender_report_ = true;
  ntp_last_sender_report_ = sr_packet->ntp();
  ms_receive_last_sr_ = UTCTimeMillis();
  return;
}

void RtcpReceiver::ParseReceiverReport(rtcp::CommonHeader* header,
                                       PacketInformation* info) {
  std::unique_ptr<rtcp::ReceiverReport> rr_packet =
      std::make_unique<rtcp::ReceiverReport>();
  std::unique_ptr<Result> result = rr_packet->StorePacket(*header);
  if (!(result->ok())) {
    QOSRTP_LOG(Error, "Failed to parse receiver report, because: %s",
               result->description().c_str());
    return;
  }
  if (config_->remote_ssrc != rr_packet->sender_ssrc()) {
    QOSRTP_LOG(Error,
               "Failed to parse receiver report, because: the ssrc in the "
               "package is not equal to the remote ssrc");
    return;
  }
  info->type_flags |= static_cast<uint32_t>(RTCPPacketType::kRtcpRr);
  const std::vector<const rtcp::ReportBlock*> report_blocks =
      rr_packet->report_blocks();
  if (!(report_blocks.empty())) {
    info->type_flags |= static_cast<uint32_t>(RTCPPacketType::kRtcpReport);
  }
  return;
}

void RtcpReceiver::ParseSdes(rtcp::CommonHeader* header,
                             PacketInformation* info) {
  std::unique_ptr<rtcp::Sdes> sdes_packet = std::make_unique<rtcp::Sdes>();
  std::unique_ptr<Result> result = sdes_packet->StorePacket(*header);
  if (!(result->ok())) {
    QOSRTP_LOG(Error, "Failed to parse sdes, because: %s",
               result->description().c_str());
    return;
  }
  bool need = false;
  const std::vector<rtcp::Sdes::Chunk>& chunks = sdes_packet->chunks();
  for (auto& chunk : chunks) {
    if (config_->remote_ssrc == chunk.ssrc) {
      need = true;
      sdes_packet->SetSenderSsrc(config_->remote_ssrc);
      break;
    }
  }
  if (!need) {
    QOSRTP_LOG(Error,
               "Failed to parse sdes, because: the ssrc in the "
               "package is not equal to the remote ssrc");
    return;
  }
  info->type_flags |= static_cast<uint32_t>(RTCPPacketType::kRtcpSdes);
  return;
}

void RtcpReceiver::ParseBye(rtcp::CommonHeader* header,
                            PacketInformation* info) {
  std::unique_ptr<rtcp::Bye> bye_packet = std::make_unique<rtcp::Bye>();
  std::unique_ptr<Result> result = bye_packet->StorePacket(*header);
  if (!(result->ok())) {
    QOSRTP_LOG(Error, "Failed to parse bye, because: %s",
               result->description().c_str());
    return;
  }
  if (config_->remote_ssrc != bye_packet->sender_ssrc()) {
    QOSRTP_LOG(Error,
               "Failed to parse bye, because: the ssrc in the "
               "package is not equal to the remote ssrc");
    return;
  }
  info->type_flags |= static_cast<uint32_t>(RTCPPacketType::kRtcpBye);
  has_received_bye_ = true;
  receiver_callback_->NotifyByeReceived();
  return;
}

void RtcpReceiver::ParseNacks(rtcp::CommonHeader* header,
                              PacketInformation* info) {
  std::unique_ptr<rtcp::Nack> nack_packet = std::make_unique<rtcp::Nack>();
  std::unique_ptr<Result> result = nack_packet->StorePacket(*header);
  if (!(result->ok())) {
    QOSRTP_LOG(Error, "Failed to parse nack, because: %s",
               result->description().c_str());
    return;
  }
  if (config_->remote_ssrc != nack_packet->sender_ssrc()) {
    QOSRTP_LOG(Error,
               "Failed to parse nack, because: the ssrc in the "
               "package is not equal to the remote ssrc");
    return;
  }
  info->type_flags |= static_cast<uint32_t>(RTCPPacketType::kRtcpNack);
  receiver_callback_->NotifyNackReceived(nack_packet->packet_ids());
  return;
}

void RtcpReceiver::GetSrInfo(uint32_t& lsr, uint32_t& dlsr) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!has_received_sender_report_) {
    lsr = 0;
    dlsr = 0;
    return;
  }
  lsr = (((uint64_t)ntp_last_sender_report_) << 16) >> 32;
  dlsr = ((UTCTimeMillis() - ms_receive_last_sr_) << 16) / 1000;
}
}  // namespace qosrtp