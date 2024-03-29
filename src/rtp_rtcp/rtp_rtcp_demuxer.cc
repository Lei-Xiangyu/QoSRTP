#include "./rtp_rtcp_demuxer.h"

#include "./rtp_rtcp_tranceiver.h"
#include "../include/log.h"

namespace qosrtp {
RtpRtcpPacketDemuxer::RtpRtcpPacketDemuxer(RtpRtcpTranceiverCallback* callback)
    : callback_(callback) {}

RtpRtcpPacketDemuxer::~RtpRtcpPacketDemuxer() = default;

void RtpRtcpPacketDemuxer::OnData(
    std::vector<std::unique_ptr<DataBuffer>> data_buffers) {
  std::vector<std::unique_ptr<RtpPacket>> rtps;
  std::vector<std::unique_ptr<DataBuffer>> rtcps;
  for (auto iter = data_buffers.begin(); iter != data_buffers.end(); ++iter) {
    std::unique_ptr<DataBuffer> data_buffer = std::move(*iter);
    if ((nullptr == data_buffer) || (nullptr == callback_)) continue;
    if (IsRtp(data_buffer.get())) {
      std::unique_ptr<RtpPacket> packet = RtpPacket::Create();
      std::unique_ptr<Result> result =
          packet->StorePacket(data_buffer->Get(), data_buffer->size());
      if (result->ok()) {
        //QOSRTP_LOG(Trace, "Parse rtp packet, pt: %hu, seq: %hu, ts: %u",
        //           (uint16_t)packet->payload_type(), packet->sequence_number(),
        //           packet->timestamp());
        rtps.push_back(std::move(packet));
        //callback_->OnRtp(std::move(packet));
      } else {
        QOSRTP_LOG(Error, "Failed to parse rtp packet, because: %s",
                   result->description().c_str());
      }
    } else if (IsRtcp(data_buffer.get())) {
      rtcps.push_back(std::move(data_buffer));
      //callback_->OnRtcp(std::move(data_buffer));
    } else {
      QOSRTP_LOG(Warning, "The received data is neither rtp nor rtcp");
    }
  }
  callback_->OnRtp(std::move(rtps));
  callback_->OnRtcp(std::move(rtcps));
}

bool RtpRtcpPacketDemuxer::IsRtp(const DataBuffer* data_buffer) {
  if (data_buffer->size() < RtpPacket::kFixedBufferLength) return false;
  return HasCorrectRtpVersion(data_buffer->Get()) &&
         !PayloadTypeIsReservedForRtcp(data_buffer->Get()[1] & 0x7F);
}

bool RtpRtcpPacketDemuxer::IsRtcp(const DataBuffer* data_buffer) {
  if (data_buffer->size() < rtcp::RtcpPacket::kHeaderLength) return false;
  return HasCorrectRtpVersion(data_buffer->Get()) &&
         PayloadTypeIsReservedForRtcp(data_buffer->Get()[1] & 0x7F);
}

bool RtpRtcpPacketDemuxer::HasCorrectRtpVersion(const uint8_t* data) {
  return (data[0] >> 6) == RtpPacket::kVersion;
}

bool RtpRtcpPacketDemuxer::PayloadTypeIsReservedForRtcp(uint8_t payload_type) {
  return 64 <= payload_type && payload_type < 96;
}
}  // namespace qosrtp