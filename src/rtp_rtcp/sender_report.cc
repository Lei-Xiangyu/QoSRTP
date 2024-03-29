#include "sender_report.h"
#include "../utils/byte_io.h"
#include <format>
#include <sstream>

using namespace qosrtp;
using namespace qosrtp::rtcp;

SenderReport::SenderReport()
    : RtcpPacket(),
      rtp_timestamp_(0),
      sender_packet_count_(0),
      sender_octet_count_(0) {}

SenderReport::~SenderReport() = default;

std::unique_ptr<Result> SenderReport::AddReportBlock(
    std::unique_ptr<ReportBlock> block) {
  if (report_blocks_.size() >= kMaxNumberOfReportBlocks) {
    return Result::Create(-1, "Max report blocks reached.");
  }
  report_blocks_.push_back(std::move(block));
  return Result::Create();
}

std::unique_ptr<Result> SenderReport::SetReportBlocks(
    std::vector<std::unique_ptr<ReportBlock>> blocks) {
  if (blocks.size() > kMaxNumberOfReportBlocks) {
    return Result::Create(-1, "Max report blocks reached.");
  }
  report_blocks_ = std::move(blocks);
  return Result::Create();
}

uint32_t SenderReport::BlockLength() const {
  return kHeaderLength + kSenderBaseLength +
         report_blocks_.size() * ReportBlock::kLength;
}

std::unique_ptr<Result> SenderReport::LoadPacket(uint8_t* packet,
                                                  uint32_t* pos,
                                                  uint32_t max_length) const {
  if (max_length < (*pos) + BlockLength()) {
    return Result::Create(-1,
                          "The remaining buffer space is less than the length "
                          "of the loaded packet");
  }
  RtcpPacket::CreateHeader(report_blocks_.size(), SenderReport::kPacketType,
                           (BlockLength() - kHeaderLength) / sizeof(uint32_t),
                           packet, pos);
  // Write SenderReport header.
  ByteWriter<uint32_t>::WriteBigEndian(&packet[*pos + 0], sender_ssrc());
  ByteWriter<uint64_t>::WriteBigEndian(&packet[*pos + 4], (uint64_t)ntp_);
  //ByteWriter<uint32_t>::WriteBigEndian(&packet[*pos + 4], ntp_.seconds());
  //ByteWriter<uint32_t>::WriteBigEndian(&packet[*pos + 8], ntp_.fractions());
  ByteWriter<uint32_t>::WriteBigEndian(&packet[*pos + 12], rtp_timestamp_);
  ByteWriter<uint32_t>::WriteBigEndian(&packet[*pos + 16],
                                       sender_packet_count_);
  ByteWriter<uint32_t>::WriteBigEndian(&packet[*pos + 20], sender_octet_count_);
  *pos += kSenderBaseLength;
  // Write report blocks.
  for (const auto& block : report_blocks_) {
    block->LoadPacket(packet + *pos);
    *pos += ReportBlock::kLength;
  }
  return Result::Create();
}

std::unique_ptr<Result> SenderReport::StorePacket(const CommonHeader& packet) {
  if (packet.type() != kPacketType) {
    return Result::Create(-1, "The type of rtcp is not sender report.");
  }
  if (packet.payload() == nullptr) {
    return Result::Create(
        -1, "It is meaningless to parse a package with a payload of nullptr");
  }
  const uint8_t report_block_count = packet.count();
  if (packet.payload_size_bytes() <
      kSenderBaseLength + report_block_count * ReportBlock::kLength) {
    return Result::Create(-1, "Packet is too small to contain all the data.");
  }
  if (packet.payload_size_bytes() >
      kSenderBaseLength + report_block_count * ReportBlock::kLength) {
    uint32_t extensions_length = packet.payload_size_bytes() -
                                 kSenderBaseLength +
                                 report_block_count * ReportBlock::kLength;
    if ((extensions_length % 4) != 0) {
      return Result::Create(
          -1, "The length of the rtcp extension must be divisible by 4.");
    }
    // It does not intend to process the extension, nor does it intend to inform
    // the caller of the extension. The processing of the report is isolated
    // from the caller.
  }
  // Read SenderReport header.
  const uint8_t* const payload = packet.payload();
  SetSenderSsrc(ByteReader<uint32_t>::ReadBigEndian(&payload[0]));
  uint32_t secs = ByteReader<uint32_t>::ReadBigEndian(&payload[4]);
  uint32_t frac = ByteReader<uint32_t>::ReadBigEndian(&payload[8]);
  ntp_.Set(secs, frac);
  rtp_timestamp_ = ByteReader<uint32_t>::ReadBigEndian(&payload[12]);
  sender_packet_count_ = ByteReader<uint32_t>::ReadBigEndian(&payload[16]);
  sender_octet_count_ = ByteReader<uint32_t>::ReadBigEndian(&payload[20]);
  report_blocks_.resize(report_block_count);
  const uint8_t* next_block = payload + kSenderBaseLength;
  for (auto& block : report_blocks_) {
    auto block_parsed = block->StorePacket(next_block);
    if (!block_parsed->ok()) {
      std::stringstream result_description;
      result_description << "Failed to parse report block, because:"
                         << "\"" << block_parsed->description() << "\"."; 
      return Result::Create(-1, result_description.str());
    }
    next_block += ReportBlock::kLength;
  }
  return Result::Create();
}