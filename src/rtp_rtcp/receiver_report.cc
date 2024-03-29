#include "receiver_report.h"

#include <sstream>

#include "../utils/byte_io.h"

using namespace qosrtp;
using namespace qosrtp::rtcp;

ReceiverReport::ReceiverReport() = default;

ReceiverReport::~ReceiverReport() = default;

uint32_t ReceiverReport::BlockLength() const {
  return kHeaderLength + kRrBaseLength +
         report_blocks_.size() * ReportBlock::kLength;
}

std::unique_ptr<Result> ReceiverReport::LoadPacket(uint8_t* packet,
                                                   uint32_t* pos,
                                                   uint32_t max_length) const {
  if (*pos + BlockLength() > max_length) {
    return Result::Create(-1,
                          "The remaining buffer space is less than the length "
                          "of the loaded packet");
  }
  RtcpPacket::CreateHeader(report_blocks_.size(), ReceiverReport::kPacketType,
                           (BlockLength() - kHeaderLength) / sizeof(uint32_t),
                           packet, pos);
  ByteWriter<uint32_t>::WriteBigEndian(packet + *pos, sender_ssrc());
  *pos += kRrBaseLength;
  for (const auto& block : report_blocks_) {
    block->LoadPacket(packet + *pos);
    *pos += ReportBlock::kLength;
  }
  return Result::Create();
}

std::unique_ptr<Result> ReceiverReport::StorePacket(
    const CommonHeader& packet) {
  if (kPacketType != packet.type()) {
    return Result::Create(-1, "The type of rtcp is not receiver report.");
  }
  if (packet.payload() == nullptr) {
    return Result::Create(
        -1, "It is meaningless to parse a package with a payload of nullptr");
  }
  const uint8_t report_blocks_count = packet.count();
  if (packet.payload_size_bytes() <
      kRrBaseLength + report_blocks_count * ReportBlock::kLength) {
    return Result::Create(-1, "Packet is too small to contain all the data.");
  }
  if (packet.payload_size_bytes() >
      kRrBaseLength + report_blocks_count * ReportBlock::kLength) {
    uint32_t extensions_length = packet.payload_size_bytes() - kRrBaseLength +
                                 report_blocks_count * ReportBlock::kLength;
    if ((extensions_length % 4) != 0) {
      return Result::Create(
          -1, "The length of the rtcp extension must be divisible by 4.");
    }
    // It does not intend to process the extension, nor does it intend to inform
    // the caller of the extension. The processing of the report is isolated
    // from the caller.
  }
  SetSenderSsrc(ByteReader<uint32_t>::ReadBigEndian(packet.payload()));
  const uint8_t* next_report_block = packet.payload() + kRrBaseLength;
  report_blocks_.resize(report_blocks_count);
  for (auto& block : report_blocks_) {
    block = std::make_unique<ReportBlock>();
    block->StorePacket(next_report_block);
    next_report_block += ReportBlock::kLength;
  }
  return Result::Create();
}

std::unique_ptr<Result> ReceiverReport::AddReportBlock(
    std::unique_ptr<ReportBlock> block) {
  if (report_blocks_.size() >= kMaxNumberOfReportBlocks) {
    return Result::Create(-1, "Max report blocks reached.");
  }
  report_blocks_.push_back(std::move(block));
  return Result::Create();
}

std::unique_ptr<Result> ReceiverReport::SetReportBlocks(
    std::vector<std::unique_ptr<ReportBlock>> blocks) {
  if (blocks.size() > kMaxNumberOfReportBlocks) {
    return Result::Create(-1, "Max report blocks reached.");
  }
  report_blocks_ = std::move(blocks);
  return Result::Create();
}