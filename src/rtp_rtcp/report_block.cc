#include "report_block.h"

#include "../utils/byte_io.h"

using namespace qosrtp;
using namespace qosrtp::rtcp;

ReportBlock::ReportBlock()
    : source_ssrc_(0),
      fraction_lost_(0),
      cumulative_lost_(0),
      extended_high_seq_num_(0),
      jitter_(0),
      last_sr_(0),
      delay_since_last_sr_(0) {}

ReportBlock::~ReportBlock() = default;

std::unique_ptr<Result> ReportBlock::SetCumulativeLost(
    int32_t cumulative_lost) {
  // We have only 3 bytes to store it, and it's a signed value.
  if (cumulative_lost >= (1 << 23) || cumulative_lost < -(1 << 23)) {
    return Result::Create(
        -1, "Cumulative lost is too big to fit into Report Block");
  }
  cumulative_lost_ = cumulative_lost;
  return Result::Create();
}

void ReportBlock::LoadPacket(uint8_t* buffer) const {
  ByteWriter<uint32_t>::WriteBigEndian(&buffer[0], source_ssrc_);
  buffer[4] = fraction_lost_;
  ByteWriter<int32_t, 3>::WriteBigEndian(&buffer[5], cumulative_lost_);
  ByteWriter<uint32_t>::WriteBigEndian(&buffer[8], extended_high_seq_num_);
  ByteWriter<uint32_t>::WriteBigEndian(&buffer[12], jitter_);
  ByteWriter<uint32_t>::WriteBigEndian(&buffer[16], last_sr_);
  ByteWriter<uint32_t>::WriteBigEndian(&buffer[20], delay_since_last_sr_);
}

std::unique_ptr<Result> ReportBlock::StorePacket(const uint8_t* buffer) {
  source_ssrc_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[0]);
  fraction_lost_ = buffer[4];
  cumulative_lost_ = ByteReader<int32_t, 3>::ReadBigEndian(&buffer[5]);
  extended_high_seq_num_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[8]);
  jitter_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[12]);
  last_sr_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[16]);
  delay_since_last_sr_ = ByteReader<uint32_t>::ReadBigEndian(&buffer[20]);
  return Result::Create();
}