#pragma once
#include <vector>

#include "../include/rtp_packet.h"

namespace qosrtp {
class RtpRtcpTranceiverCallback;

class RtpRtcpPacketDemuxer {
 public:
  RtpRtcpPacketDemuxer(RtpRtcpTranceiverCallback* callback);
  ~RtpRtcpPacketDemuxer();
  void OnData(std::vector<std::unique_ptr<DataBuffer>> data_buffers);

 private:
  bool IsRtp(const DataBuffer* data_buffer);
  bool IsRtcp(const DataBuffer* data_buffer);
  bool HasCorrectRtpVersion(const uint8_t* data);
  bool PayloadTypeIsReservedForRtcp(uint8_t payload_type);
  RtpRtcpTranceiverCallback* callback_;
};
}  // namespace qosrtp