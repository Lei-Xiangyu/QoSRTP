#include "./rtpfb.h"

#include "../utils/byte_io.h"

using namespace qosrtp;
using namespace rtcp;

Rtpfb::Rtpfb() = default;

Rtpfb::~Rtpfb() = default;

void Rtpfb::StoreCommonFeedback(const uint8_t* payload) {
  SetSenderSsrc(ByteReader<uint32_t>::ReadBigEndian(&payload[0]));
  SetMediaSsrc(ByteReader<uint32_t>::ReadBigEndian(&payload[4]));
}

void Rtpfb::LoadCommonFeedback(uint8_t* payload) const {
  ByteWriter<uint32_t>::WriteBigEndian(&payload[0], sender_ssrc());
  ByteWriter<uint32_t>::WriteBigEndian(&payload[4], media_ssrc());
}
