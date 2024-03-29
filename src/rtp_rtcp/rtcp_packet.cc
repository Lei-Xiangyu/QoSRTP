#include "./rtcp_packet.h"

namespace qosrtp {
namespace rtcp {
RtcpPacket::RtcpPacket() : sender_ssrc_(0) {}

RtcpPacket::~RtcpPacket() = default;
}  // namespace rtcp
}  // namespace qosrtp