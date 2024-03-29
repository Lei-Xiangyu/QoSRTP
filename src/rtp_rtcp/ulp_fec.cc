#include "ulp_fec.h"

#include "../utils/byte_io.h"
#include "../utils/seq_comparison.h"
#include "./fec_private_tables_bursty.h"
#include "./fec_private_tables_random.h"
#include "../include/log.h"
#include "../utils/time_utils.h"

namespace qosrtp {
UlpFecEncoder::UlpFecEncoder() : config_(nullptr) {
  memset(fec_packet_mask_, 0, kFECPacketMaskMaxSize);
}

UlpFecEncoder::~UlpFecEncoder() = default;

UlpFecEncoder::RtpPacketData::RtpPacketData(
    const RtpPacket* param_packet_struct,
    std::unique_ptr<DataBuffer> param_packet_buffer)
    : packet_struct(param_packet_struct),
      packet_buffer(std::move(param_packet_buffer)) {}

UlpFecEncoder::RtpPacketData::~RtpPacketData() = default;

std::unique_ptr<Result> UlpFecEncoder::Configure(
    std::unique_ptr<FecEncoderConfig> config) {
  if (nullptr == config) {
    return Result::Create(-1, "Parameter cannot be a nullptr");
  }
  if ((((uint8_t)1) << RtpPacket::kBitsizePayloadType) <=
      config->payload_type()) {
    return Result::Create(-1, "payload_type invalid value");
  }
  config_ = std::move(config);
  return Result::Create();
}

std::unique_ptr<Result> UlpFecEncoder::Encode(
    const std::vector<const RtpPacket*>& protected_packets,
    uint32_t num_important_packets,
    ImportantPacketsProtectionMethod method_protect_important_packets,
    uint8_t protection_factor, FecMaskType fec_mask_type,
    std::vector<std::unique_ptr<RtpPacket>>& fec_packets) {
  size_t nb_protected_packets = protected_packets.size();
  if (0 == nb_protected_packets) {
    return Result::Create(-1, "protected_packets is empty");
  }
  if (kUlpfecMaxMediaPackets < nb_protected_packets) {
    return Result::Create(-1, "The size of protected_packets is too large");
  }
  if (num_important_packets > nb_protected_packets) {
    return Result::Create(-1,
                          "num_important_packets must be less than or equal to "
                          "the size of protected_packets");
  }
  std::vector<std::unique_ptr<RtpPacketData>> input_packets;
  uint16_t last_seq = 0;
  for (auto iter_protected_packet = protected_packets.begin();
       iter_protected_packet != protected_packets.end();
       ++iter_protected_packet) {
    const RtpPacket* protected_packet = (*iter_protected_packet);
    if (config_->ssrc() != protected_packet->ssrc()) {
      return Result::Create(-1, "ssrc is different from the set value");
    }
    if (iter_protected_packet != protected_packets.begin()) {
      if (!IsNextSeq(last_seq, protected_packet->sequence_number())) {
        return Result::Create(
            -1, "The sequence number must be continuously increasing");
      }
    }
    last_seq = protected_packet->sequence_number();
    input_packets.push_back(std::make_unique<RtpPacketData>(
        protected_packet, protected_packet->LoadPacket()));
  }
  uint8_t packet_mask_size = PacketMaskSize(nb_protected_packets);
  uint32_t num_fec_packets =
      NumFecPackets(nb_protected_packets, protection_factor);
  memset(fec_packet_mask_, 0, num_fec_packets * packet_mask_size);
  GeneratePacketMasks(nb_protected_packets, num_fec_packets,
                      num_important_packets, method_protect_important_packets,
                      fec_mask_type, packet_mask_size, fec_packet_mask_);
  GenerateFecPackets(input_packets, num_fec_packets, packet_mask_size,
                     fec_packets);
  return Result::Create();
}

void UlpFecEncoder::GenerateFecPackets(
    const std::vector<std::unique_ptr<RtpPacketData>>& media_packets,
    uint8_t num_fec_packets, uint8_t packet_mask_size,
    std::vector<std::unique_ptr<RtpPacket>>& fec_packets) {
  for (uint8_t i_fec = 0; i_fec < num_fec_packets; ++i_fec) {
    std::vector<const RtpPacketData*> group_media_packets;
    uint16_t max_length = 0;
    const uint8_t* mask_this = fec_packet_mask_ + i_fec * packet_mask_size;
    for (uint8_t i_mask = 0; i_mask < packet_mask_size * 8; ++i_mask) {
      const uint8_t* mask_this_byte = mask_this + i_mask / 8;
      uint8_t i_bit = i_mask % 8;
      if (!((((uint8_t)0x80) >> i_bit) & *mask_this_byte)) {
        continue;
      }
      const std::unique_ptr<RtpPacketData>& packet_in_group =
          media_packets[i_mask];
      group_media_packets.push_back(packet_in_group.get());
      max_length = std::max(max_length,
                            (uint16_t)(packet_in_group->packet_buffer->size() -
                                       RtpPacket::kFixedBufferLength));
    }
    fec_packets.push_back(BuildFecPacket(group_media_packets, max_length));
  }
}

std::unique_ptr<RtpPacket> UlpFecEncoder::BuildFecPacket(
    std::vector<const RtpPacketData*>& group_media_packets,
    uint16_t max_length) {
  uint16_t seq_base =
      (group_media_packets.front())->packet_struct->sequence_number();
  uint16_t seq_end =
      (group_media_packets.back())->packet_struct->sequence_number();
  uint8_t seq_diff = DiffSeq(seq_base, seq_end);
  bool l = false;
  uint8_t packet_mask_size = kUlpfecPacketMaskSizeLBitClear;
  if (kUlpfecPacketMaskSizeLBitClear * 8 < (seq_diff + 1)) {
    l = true;
    packet_mask_size = kUlpfecPacketMaskSizeLBitSet;
  }
  std::unique_ptr<DataBuffer> fec_payload = DataBuffer::Create(
      max_length + kUlpfecHeaderLength + 2 + packet_mask_size);
  fec_payload->SetSize(fec_payload->capacity());
  fec_payload->MemSet(0, 0, fec_payload->size());
  uint8_t* fec_level_0_header = fec_payload->GetW();
  uint8_t* fec_level_1_header = fec_level_0_header + kUlpfecHeaderLength;
  uint8_t* fec_level_1_payload = fec_level_1_header + 2 + packet_mask_size;
  // FEC Level 1 Header
  uint8_t Protection_Length[2];
  ByteWriter<uint16_t>::WriteBigEndian(Protection_Length, max_length);
  fec_level_1_header[0] = Protection_Length[0];
  fec_level_1_header[1] = Protection_Length[1];
  uint8_t* fec_level_1_header_mask = fec_level_1_header + 2;
  for (auto& packet : group_media_packets) {
    const uint8_t* src_header = packet->packet_buffer->Get();
    const uint8_t* src_payload = src_header + RtpPacket::kFixedBufferLength;
    uint16_t src_length =
        packet->packet_buffer->size() - RtpPacket::kFixedBufferLength;
    // XOR the first 2 bytes of the header: V, P, X, CC, M, PT fields.
    fec_level_0_header[0] ^= src_header[0];
    fec_level_0_header[1] ^= src_header[1];
    // XOR the 5th to 8th bytes of the header: the timestamp field.
    fec_level_0_header[4] ^= src_header[4];
    fec_level_0_header[5] ^= src_header[5];
    fec_level_0_header[6] ^= src_header[6];
    fec_level_0_header[7] ^= src_header[7];
    // XOR the length recovery field.
    uint8_t src_payload_length_network_order[2];
    ByteWriter<uint16_t>::WriteBigEndian(src_payload_length_network_order,
                                         src_length);
    fec_level_0_header[8] ^= src_payload_length_network_order[0];
    fec_level_0_header[9] ^= src_payload_length_network_order[1];
    for (int16_t pos_payload = 0; pos_payload < src_length; ++pos_payload) {
      fec_level_1_payload[pos_payload] ^= src_payload[pos_payload];
    }
    uint8_t seq_diff_this_packet =
        DiffSeq(seq_base, packet->packet_struct->sequence_number());
    uint8_t index_mask_byte = seq_diff_this_packet / 8;
    uint8_t index_mask_bit = seq_diff_this_packet % 8;
    fec_level_1_header_mask[index_mask_byte] |=
        (((uint8_t)0x80) >> index_mask_bit);
  }
  //if ((fec_level_0_header[8] == 5) && (fec_level_0_header[9] == 120) &&
  //    (group_media_packets.size() % 2 == 0)) {
  //  QOSRTP_LOG(Trace, "debug");
  //}
  uint8_t seq_base_network_order[2];
  ByteWriter<uint16_t>::WriteBigEndian(seq_base_network_order, seq_base);
  fec_level_0_header[2] ^= seq_base_network_order[0];
  fec_level_0_header[3] ^= seq_base_network_order[1];
  fec_level_0_header[0] &= 0x3f;
  if (l) fec_level_0_header[0] |= 0x40;
  std::unique_ptr<RtpPacket> fec_packet = RtpPacket::Create();
  std::vector<uint32_t> csrcs;
  fec_packet->StorePacket(config_->payload_type(), 0, 0, config_->ssrc(), csrcs,
                          nullptr, std::move(fec_payload), 0);
  return fec_packet;
}

uint32_t UlpFecEncoder::NumFecPackets(uint32_t num_media_packets,
                                      uint8_t protection_factor) {
  uint32_t num_fec_packets =
      (num_media_packets * protection_factor + (1 << 7)) >> 8;
  if (protection_factor > 0 && num_fec_packets == 0) {
    num_fec_packets = 1;
  }
  return num_fec_packets;
}

const uint8_t* UlpFecEncoder::PickTable(FecMaskType fec_mask_type,
                                        int num_media_packets) {
  if (fec_mask_type != FecMaskType::kFecMaskRandom &&
      num_media_packets <=
          static_cast<int>(fec_private_tables::kPacketMaskBurstyTbl[0])) {
    return &fec_private_tables::kPacketMaskBurstyTbl[0];
  }
  return &fec_private_tables::kPacketMaskRandomTbl[0];
}

std::unique_ptr<DataBuffer> UlpFecEncoder::LookUpInFecTable(
    const uint8_t* table, int media_packet_index, int fec_index) {
  const uint8_t* entry = &table[1];
  uint8_t entry_size_increment = 2;  // 0-16 are 2 byte wide, then changes to 6.
  // Hop over un-interesting array entries.
  for (int i = 0; i < media_packet_index; ++i) {
    if (i == 16) entry_size_increment = 6;
    uint8_t count = entry[0];
    ++entry;  // skip over the count.
    for (int j = 0; j < count; ++j) {
      entry += entry_size_increment * (j + 1);  // skip over the data.
    }
  }
  if (media_packet_index == 16) entry_size_increment = 6;
  ++entry;  // Skip over the size.
  // Find the appropriate data in the second dimension.
  // Find the specific data we're looking for.
  for (int i = 0; i < fec_index; ++i)
    entry += entry_size_increment * (i + 1);  // skip over the data.

  size_t size = entry_size_increment * (fec_index + 1);
  std::unique_ptr<DataBuffer> ret_buffer = DataBuffer::Create(size);
  ret_buffer->SetSize(size);
  ret_buffer->ModifyAt(0, &entry[0], size);
  return ret_buffer;
}

std::unique_ptr<DataBuffer> UlpFecEncoder::Mask(int num_media_packets,
                                                int num_fec_packets,
                                                FecMaskType fec_mask_type,
                                                uint8_t packet_mask_size,
                                                const uint8_t* mask_table) {
  if (num_media_packets <= 12) {
    return LookUpInFecTable(mask_table, num_media_packets - 1,
                            num_fec_packets - 1);
  }

  // Generate FEC code mask for {num_media_packets(M), num_fec_packets(N)}
  // (use N FEC packets to protect M media packets) In the mask, each FEC
  // packet occupies one row, each bit / coloumn represent one media packet.
  // E.g. Row A, Col/Bit B is set to 1, means FEC packet A will have
  // protection for media packet B.

  // Loop through each fec packet.
  for (uint8_t row = 0; row < num_fec_packets; row++) {
    // Loop through each fec code in a row, one code has 8 bits.
    // Bit X will be set to 1 if media packet X shall be protected by current
    // FEC packet. In this implementation, the protection is interleaved, thus
    // media packet X will be protected by FEC packet (X % N)
    for (uint8_t col = 0; col < packet_mask_size; col++) {
      fec_packet_mask_[row * packet_mask_size + col] =
          ((col * 8) % num_fec_packets == row && (col * 8) < num_media_packets
               ? 0x80
               : 0x00) |
          ((col * 8 + 1) % num_fec_packets == row &&
                   (col * 8 + 1) < num_media_packets
               ? 0x40
               : 0x00) |
          ((col * 8 + 2) % num_fec_packets == row &&
                   (col * 8 + 2) < num_media_packets
               ? 0x20
               : 0x00) |
          ((col * 8 + 3) % num_fec_packets == row &&
                   (col * 8 + 3) < num_media_packets
               ? 0x10
               : 0x00) |
          ((col * 8 + 4) % num_fec_packets == row &&
                   (col * 8 + 4) < num_media_packets
               ? 0x08
               : 0x00) |
          ((col * 8 + 5) % num_fec_packets == row &&
                   (col * 8 + 5) < num_media_packets
               ? 0x04
               : 0x00) |
          ((col * 8 + 6) % num_fec_packets == row &&
                   (col * 8 + 6) < num_media_packets
               ? 0x02
               : 0x00) |
          ((col * 8 + 7) % num_fec_packets == row &&
                   (col * 8 + 7) < num_media_packets
               ? 0x01
               : 0x00);
    }
  }

  std::unique_ptr<DataBuffer> ret_buffer =
      DataBuffer::Create(num_fec_packets * packet_mask_size);
  ret_buffer->SetSize(ret_buffer->capacity());
  ret_buffer->ModifyAt(0, &fec_packet_mask_[0], ret_buffer->size());
  return ret_buffer;
}

void UlpFecEncoder::GeneratePacketMasks(
    int num_media_packets, int num_fec_packets, int num_imp_packets,
    ImportantPacketsProtectionMethod method_protect_important_packets,
    FecMaskType fec_mask_type, uint8_t packet_mask_size, uint8_t* packet_mask) {
  const uint8_t* mask_table = PickTable(fec_mask_type, num_media_packets);
  if (0 == num_imp_packets) {
    std::unique_ptr<DataBuffer> mask =
        Mask(num_media_packets, num_fec_packets, fec_mask_type,
             packet_mask_size, mask_table);
    memcpy(packet_mask, mask->Get(), mask->size());
  } else {
    if (ImportantPacketsProtectionMethod::kNone ==
        method_protect_important_packets) {
      method_protect_important_packets =
          ImportantPacketsProtectionMethod::kModeOverlap;
    }
    UnequalProtectionMask(num_media_packets, num_fec_packets, num_imp_packets,
                          method_protect_important_packets, mask_table,
                          fec_mask_type, packet_mask_size, packet_mask);
  }
}

uint8_t UlpFecEncoder::SetProtectionAllocation(uint8_t num_media_packets,
                                               uint8_t num_fec_packets,
                                               uint8_t num_imp_packets) {
  float alloc_par = 0.5;
  uint8_t max_num_fec_for_imp = alloc_par * num_fec_packets;
  uint8_t num_fec_for_imp_packets = (num_imp_packets < max_num_fec_for_imp)
                                        ? num_imp_packets
                                        : max_num_fec_for_imp;
  // Fall back to equal protection in this case
  if (num_fec_packets == 1 && (num_media_packets > 2 * num_imp_packets)) {
    num_fec_for_imp_packets = 0;
  }
  return num_fec_for_imp_packets;
}

void UlpFecEncoder::UnequalProtectionMask(
    int num_media_packets, int num_fec_packets, int num_imp_packets,
    ImportantPacketsProtectionMethod method_protect_important_packets,
    const uint8_t* mask_table, FecMaskType fec_mask_type,
    uint8_t packet_mask_size, uint8_t* packet_mask) {
  int num_fec_for_imp_packets = 0;
  if (ImportantPacketsProtectionMethod::kModeBiasFirstPacket !=
      method_protect_important_packets) {
    num_fec_for_imp_packets = SetProtectionAllocation(
        num_media_packets, num_fec_packets, num_imp_packets);
  }
  int num_fec_remaining = num_fec_packets - num_fec_for_imp_packets;
  // Done with setting protection type and allocation
  //
  // Generate sub_mask1
  //
  if (num_fec_for_imp_packets > 0) {
    ImportantPacketProtection(num_fec_for_imp_packets, num_imp_packets,
                              packet_mask_size, fec_mask_type, packet_mask,
                              mask_table);
  }
  //
  // Generate sub_mask2
  //
  if (num_fec_remaining > 0) {
    RemainingPacketProtection(num_media_packets, num_fec_remaining,
                              num_fec_for_imp_packets, packet_mask_size,
                              method_protect_important_packets, fec_mask_type,
                              packet_mask, mask_table);
  }
}

void UlpFecEncoder::ImportantPacketProtection(int num_fec_for_imp_packets,
                                              int num_imp_packets,
                                              int num_mask_bytes,
                                              FecMaskType fec_mask_type,
                                              uint8_t* packet_mask,
                                              const uint8_t* mask_table) {
  const int num_imp_mask_bytes = PacketMaskSize(num_imp_packets);
  //std::unique_ptr<DataBuffer> packet_mask_sub_1 =
  //    LookUpInFecTable(mask_table, num_imp_packets, num_fec_for_imp_packets);
  std::unique_ptr<DataBuffer> packet_mask_sub_1 =
      Mask(num_imp_packets, num_fec_for_imp_packets, fec_mask_type,
           num_mask_bytes, mask_table);
  FitSubMask(num_mask_bytes, num_imp_mask_bytes, num_fec_for_imp_packets,
             packet_mask_sub_1->Get(), packet_mask);
}

void UlpFecEncoder::FitSubMask(int num_mask_bytes, int num_sub_mask_bytes,
                               int num_rows, const uint8_t* sub_mask,
                               uint8_t* packet_mask) {
  if (num_mask_bytes == num_sub_mask_bytes) {
    memcpy(packet_mask, sub_mask, num_rows * num_sub_mask_bytes);
  } else {
    for (int i = 0; i < num_rows; ++i) {
      int pkt_mask_idx = i * num_mask_bytes;
      int pkt_mask_idx2 = i * num_sub_mask_bytes;
      for (int j = 0; j < num_sub_mask_bytes; ++j) {
        packet_mask[pkt_mask_idx] = sub_mask[pkt_mask_idx2];
        pkt_mask_idx++;
        pkt_mask_idx2++;
      }
    }
  }
}

void UlpFecEncoder::ShiftFitSubMask(int num_mask_bytes, int res_mask_bytes,
                                    int num_column_shift, int end_row,
                                    const uint8_t* sub_mask,
                                    uint8_t* packet_mask) {
  // Number of bit shifts within a byte
  const int num_bit_shifts = (num_column_shift % 8);
  const int num_byte_shifts = num_column_shift >> 3;

  // Modify new mask with sub-mask21.

  // Loop over the remaining FEC packets.
  for (int i = num_column_shift; i < end_row; ++i) {
    // Byte index of new mask, for row i and column res_mask_bytes,
    // offset by the number of bytes shifts
    int pkt_mask_idx =
        i * num_mask_bytes + res_mask_bytes - 1 + num_byte_shifts;
    // Byte index of sub_mask, for row i and column res_mask_bytes
    int pkt_mask_idx2 =
        (i - num_column_shift) * res_mask_bytes + res_mask_bytes - 1;

    uint8_t shift_right_curr_byte = 0;
    uint8_t shift_left_prev_byte = 0;
    uint8_t comb_new_byte = 0;

    // Handle case of num_mask_bytes > res_mask_bytes:
    // For a given row, copy the rightmost "numBitShifts" bits
    // of the last byte of sub_mask into output mask.
    if (num_mask_bytes > res_mask_bytes) {
      shift_left_prev_byte = (sub_mask[pkt_mask_idx2] << (8 - num_bit_shifts));
      packet_mask[pkt_mask_idx + 1] = shift_left_prev_byte;
    }

    // For each row i (FEC packet), shift the bit-mask of the sub_mask.
    // Each row of the mask contains "resMaskBytes" of bytes.
    // We start from the last byte of the sub_mask and move to first one.
    for (int j = res_mask_bytes - 1; j > 0; j--) {
      // Shift current byte of sub21 to the right by "numBitShifts".
      shift_right_curr_byte = sub_mask[pkt_mask_idx2] >> num_bit_shifts;

      // Fill in shifted bits with bits from the previous (left) byte:
      // First shift the previous byte to the left by "8-numBitShifts".
      shift_left_prev_byte =
          (sub_mask[pkt_mask_idx2 - 1] << (8 - num_bit_shifts));

      // Then combine both shifted bytes into new mask byte.
      comb_new_byte = shift_right_curr_byte | shift_left_prev_byte;

      // Assign to new mask.
      packet_mask[pkt_mask_idx] = comb_new_byte;
      pkt_mask_idx--;
      pkt_mask_idx2--;
    }
    // For the first byte in the row (j=0 case).
    shift_right_curr_byte = sub_mask[pkt_mask_idx2] >> num_bit_shifts;
    packet_mask[pkt_mask_idx] = shift_right_curr_byte;
  }
}

void UlpFecEncoder::RemainingPacketProtection(
    int num_media_packets, int num_fec_remaining, int num_fec_for_imp_packets,
    int num_mask_bytes, ImportantPacketsProtectionMethod mode,
    FecMaskType fec_mask_type, uint8_t* packet_mask,
    const uint8_t* mask_table) {
  if (mode == ImportantPacketsProtectionMethod::kModeNoOverlap) {
    // sub_mask21
    const int res_mask_bytes =
        PacketMaskSize(num_media_packets - num_fec_for_imp_packets);
    auto end_row = (num_fec_for_imp_packets + num_fec_remaining);
    std::unique_ptr<DataBuffer> packet_mask_sub_21 =
        Mask(num_media_packets - num_fec_for_imp_packets, num_fec_remaining,
             fec_mask_type, res_mask_bytes, mask_table);
    ShiftFitSubMask(num_mask_bytes, res_mask_bytes, num_fec_for_imp_packets,
                    end_row, packet_mask_sub_21->Get(), packet_mask);

  } else if (mode == ImportantPacketsProtectionMethod::kModeOverlap ||
             mode == ImportantPacketsProtectionMethod::kModeBiasFirstPacket) {
    // sub_mask22
    std::unique_ptr<DataBuffer> packet_mask_sub_22 =
        Mask(num_media_packets, num_fec_remaining, fec_mask_type,
             num_mask_bytes, mask_table);
    FitSubMask(num_mask_bytes, num_mask_bytes, num_fec_remaining,
               packet_mask_sub_22->Get(),
               &packet_mask[num_fec_for_imp_packets * num_mask_bytes]);
    if (mode == ImportantPacketsProtectionMethod::kModeBiasFirstPacket) {
      for (int i = 0; i < num_fec_remaining; ++i) {
        int pkt_mask_idx = i * num_mask_bytes;
        packet_mask[pkt_mask_idx] = packet_mask[pkt_mask_idx] | (1 << 7);
      }
    }
  }
}

UlpFecDecoder::UlpFecDecoder()
    : config_(nullptr), has_output_(false), seq_last_output_(0) {}

UlpFecDecoder::~UlpFecDecoder() = default;

std::unique_ptr<Result> UlpFecDecoder::Configure(
    std::unique_ptr<FecDecoderConfig> config) {
  if (nullptr == config) {
    return Result::Create(-1, "Parameter cannot be a nullptr");
  }
  if ((((uint8_t)1) << RtpPacket::kBitsizePayloadType) <=
      config->payload_type()) {
    return Result::Create(-1, "payload_type invalid value");
  }
  config_ = std::move(config);
  return Result::Create();
}

void UlpFecDecoder::Decode(
    std::vector<std::unique_ptr<RtpPacket>> received_packets,
    std::vector<std::unique_ptr<RtpPacket>>& recovered_packets) {
  uint64_t trace_1 = UTCTimeMillis();
  CachePackets(received_packets);
  uint64_t trace_2 = UTCTimeMillis();
  RcoverPackets();
  uint64_t trace_3 = UTCTimeMillis();
  uint16_t latest_cached_seq =
      cached_packets_.back()->rtp_struct->sequence_number();
  for (auto iter_cached_packet = cached_packets_.begin();
       iter_cached_packet != cached_packets_.end();) {
    if (IsSeqBeforeInRange(
            (*iter_cached_packet)->rtp_struct->sequence_number(),
            latest_cached_seq, config_->max_cache_seq_difference())) {
      break;
    }
    bool output = false;
    if (!has_output_ ||
        IsSeqAfter(seq_last_output_,
                   (*iter_cached_packet)->rtp_struct->sequence_number())) {
      output = true;
    }
    if (output) {
      recovered_packets.push_back(std::move((*iter_cached_packet)->rtp_struct));
    }
    iter_cached_packet = cached_packets_.erase(iter_cached_packet);
  }
  for (auto iter_cached_packet = cached_packets_.begin();
       iter_cached_packet != cached_packets_.end(); ++iter_cached_packet) {
    if (recovered_packets.empty()) {
      if (has_output_ &&
          !IsSeqAfter(seq_last_output_,
                      (*iter_cached_packet)->rtp_struct->sequence_number())) {
        break;
      }
    } else {
      if (!IsNextSeq(recovered_packets.back()->sequence_number(),
                     (*iter_cached_packet)->rtp_struct->sequence_number())) {
        break;
      }
    }
    recovered_packets.push_back(RtpPacket::Create((*iter_cached_packet)->rtp_struct.get()));
  }
  if (!recovered_packets.empty()) {
    has_output_ = true;
    seq_last_output_ = recovered_packets.back()->sequence_number();
  }
  uint64_t trace_4 = UTCTimeMillis();
  QOSRTP_LOG(Trace, "UlpFecDecoder::Decode inner cost: (%lld %lld %lld) ms",
             trace_2 - trace_1, trace_3 - trace_2, trace_4 - trace_3);
}

void UlpFecDecoder::Flush(
  std::vector<std::unique_ptr<RtpPacket>>& output_packets) {
  for (auto iter_cached_packet = cached_packets_.begin();
       iter_cached_packet != cached_packets_.end();) {
    output_packets.push_back(std::move((*iter_cached_packet)->rtp_struct));
    iter_cached_packet = cached_packets_.erase(iter_cached_packet);
  }
  if (!output_packets.empty()) {
    has_output_ = true;
    seq_last_output_ = output_packets.back()->sequence_number();
  }
}

void UlpFecDecoder::RcoverPackets() {
  uint16_t last_seq = cached_packets_.back()->rtp_struct->sequence_number();
  std::vector<CachedPacket*> fec_packets;
  for (auto iter_fec_packet = cached_packets_.begin();
       iter_fec_packet != cached_packets_.end(); ++iter_fec_packet) {
    if (!(*iter_fec_packet)->is_fec) {
      continue;
    }
    fec_packets.push_back((*iter_fec_packet).get());
  }
  for (auto iter_fec_packet = fec_packets.begin();
       iter_fec_packet != fec_packets.end(); ++iter_fec_packet) {
    if ((*iter_fec_packet)->invalid_fec) {
      continue;
    }
    bool valid = true;
    std::vector<uint16_t> no_received_seqs;
    for (auto iter_protected_seq =
             (*iter_fec_packet)->map_protected_seq_received.begin();
         iter_protected_seq !=
         (*iter_fec_packet)->map_protected_seq_received.end();
         ++iter_protected_seq) {
      if (has_output_ &&
          !IsSeqAfter(seq_last_output_, iter_protected_seq->first)) {
        valid = false;
        (*iter_fec_packet)->invalid_fec = true;
        break;
      }
      if (IsSeqAfter(last_seq, iter_protected_seq->first)) {
        valid = false;
        break;
      }
      if (iter_protected_seq->second) {
        continue;
      }
      for (auto iter_cached_packet = cached_packets_.begin();
           iter_cached_packet != cached_packets_.end(); ++iter_cached_packet) {
        if ((*iter_cached_packet)->rtp_struct->sequence_number() ==
            iter_protected_seq->first) {
          iter_protected_seq->second = true;
          (*iter_fec_packet)
              ->media_packets.push_back((*iter_cached_packet).get());
          break;
        }
      }
      if (!iter_protected_seq->second) {
        no_received_seqs.push_back(iter_protected_seq->first);
      }
    }
    if (!valid) {
      continue;
    }
    if (1 != no_received_seqs.size()) {
      continue;
    }
    uint16_t seq_will_recover = no_received_seqs.front();
    if (IsSeqBefore(seq_will_recover, seq_last_output_) ||
        IsSeqAfter(cached_packets_.back()->rtp_struct->sequence_number(),
                   seq_will_recover)) {
      continue;
    }
    std::unique_ptr<CachedPacket> recovered_packet = FecRecoverPacket(
        *iter_fec_packet, seq_will_recover, (*iter_fec_packet)->media_packets);
    if (!recovered_packet) {
      continue;
    }
    if (recovered_packet->rtp_struct->sequence_number() != seq_will_recover) {
      continue;
    }
    QOSRTP_LOG(Trace, "Recover seq: %hu, from fec seq: %hu",
               recovered_packet->rtp_struct->sequence_number(),
               (*iter_fec_packet)->rtp_struct->sequence_number());
    bool need_insert = true;
    auto pos_insert = cached_packets_.begin();
    for (; pos_insert != cached_packets_.end(); ++pos_insert) {
      if ((*pos_insert)->rtp_struct->sequence_number() ==
          recovered_packet->rtp_struct->sequence_number()) {
        need_insert = false;
        break;
      }
      if (IsSeqBefore(recovered_packet->rtp_struct->sequence_number(),
                      (*pos_insert)->rtp_struct->sequence_number())) {
        break;
      }
    }
    if (need_insert) {
      cached_packets_.insert(pos_insert, std::move(recovered_packet));
      (*iter_fec_packet)->invalid_fec = true;
    }
  }
}

std::unique_ptr<UlpFecDecoder::CachedPacket> UlpFecDecoder::FecRecoverPacket(
    const CachedPacket* fec_packet, uint16_t recovered_seq,
    const std::vector<const CachedPacket*>& media_packets) {
  const uint8_t* fec_header =
      fec_packet->rtp_buffer->Get() + RtpPacket::kFixedBufferLength;
  const uint8_t* fec_level_header = fec_header + kUlpfecHeaderLength;
  const uint8_t* fec_level_payload = fec_level_header + 2;
  uint16_t fec_packet_payload_length =
      fec_packet->rtp_buffer->size() - RtpPacket::kFixedBufferLength;
  fec_level_payload +=
      (((*fec_header) & 0x40) ? kUlpfecPacketMaskSizeLBitSet
                              : kUlpfecPacketMaskSizeLBitClear);
  if (fec_packet_payload_length <= (fec_level_payload - fec_header)) {
    return nullptr;
  }
  uint16_t protection_length =
      ByteReader<uint16_t>::ReadBigEndian(fec_level_header);
  if (protection_length <
      (fec_packet_payload_length - (fec_level_payload - fec_header))) {
    return nullptr;
  }
  std::unique_ptr<DataBuffer> recovered_rtp_buffer =
      DataBuffer::Create(protection_length + RtpPacket::kFixedBufferLength);
  recovered_rtp_buffer->SetSize(recovered_rtp_buffer->capacity());
  recovered_rtp_buffer->MemSet(0, 0, RtpPacket::kFixedBufferLength);
  recovered_rtp_buffer->ModifyAt(0, fec_header, 8);
  uint8_t* recovered_rtp_header = recovered_rtp_buffer->GetW();
  recovered_rtp_buffer->ModifyAt(RtpPacket::kFixedBufferLength,
                                 fec_level_payload, protection_length);
  uint8_t* recovered_rtp_payload =
      recovered_rtp_header + RtpPacket::kFixedBufferLength;
  uint8_t recovery_length[2];
  memcpy(recovery_length, fec_header + 8, 2);
  for (auto iter_media_packet = media_packets.begin();
       iter_media_packet != media_packets.end(); ++iter_media_packet) {
    const uint8_t* media_packet_header =
        (*iter_media_packet)->rtp_buffer->Get();
    recovered_rtp_header[0] ^= media_packet_header[0];
    recovered_rtp_header[1] ^= media_packet_header[1];
    recovered_rtp_header[4] ^= media_packet_header[4];
    recovered_rtp_header[5] ^= media_packet_header[5];
    recovered_rtp_header[6] ^= media_packet_header[6];
    recovered_rtp_header[7] ^= media_packet_header[7];
    uint16_t payload_size = (*iter_media_packet)->rtp_buffer->size() -
                            RtpPacket::kFixedBufferLength;
    if (payload_size == 0) {
      continue;
    }
    const uint8_t* media_packet_payload =
        media_packet_header + RtpPacket::kFixedBufferLength;
    recovery_length[0] ^= ((uint8_t*)&payload_size)[1];
    recovery_length[1] ^= ((uint8_t*)&payload_size)[0];
    uint16_t xor_length = std::min(payload_size, protection_length);
    for (uint16_t index = 0; index < xor_length; ++index) {
      recovered_rtp_payload[index] ^= media_packet_payload[index];
    }
  }
  ByteWriter<uint32_t>::WriteBigEndian(recovered_rtp_header + 8,
                                       config_->ssrc());
  ByteWriter<uint16_t>::WriteBigEndian(recovered_rtp_header + 2,
                                       recovered_seq);
  (*recovered_rtp_header) &= 0x3f;
  (*recovered_rtp_header) |= 0x80;
  recovered_rtp_buffer->SetSize(
      ByteReader<uint16_t>::ReadBigEndian(recovery_length) +
      RtpPacket::kFixedBufferLength);
  std::unique_ptr<UlpFecDecoder::CachedPacket> ret_packet =
      std::make_unique<UlpFecDecoder::CachedPacket>();
  ret_packet->rtp_struct = RtpPacket::Create();
  std::unique_ptr<Result> result = ret_packet->rtp_struct->StorePacket(
      recovered_rtp_buffer->Get(), recovered_rtp_buffer->size());
  if (!result->ok()) {
    QOSRTP_LOG(
        Error,
        "Failed to build rtp when recovering packet from fec, because: %s",
        result->description().c_str());
    return nullptr;
  }
  if (!ret_packet->rtp_struct->GetPayloadBuffer()) {
    QOSRTP_LOG(Error, "Get a empty rtp when recovering packet from fec");
    return nullptr;
  }
  ret_packet->rtp_buffer = std::move(recovered_rtp_buffer);
  ret_packet->DecodeFecInfo(config_->payload_type());
  return ret_packet;
}

void UlpFecDecoder::CachePackets(
    std::vector<std::unique_ptr<RtpPacket>>& received_packets) {
  for (auto iter_received_packet = received_packets.begin();
       iter_received_packet != received_packets.end(); ++iter_received_packet) {
    if (config_->ssrc() != (*iter_received_packet)->ssrc()) {
      continue;
    }
    if (has_output_) {
      if (IsSeqBefore((*iter_received_packet)->sequence_number(),
                      seq_last_output_)) {
        continue;
      }
    }
    bool need_insert = true;
    auto pos_insert = cached_packets_.begin();
    for (; pos_insert != cached_packets_.end(); ++pos_insert) {
      if ((*pos_insert)->rtp_struct->sequence_number() ==
          (*iter_received_packet)->sequence_number()) {
        need_insert = false;
        break;
      }
      if (IsSeqBefore((*iter_received_packet)->sequence_number(),
                      (*pos_insert)->rtp_struct->sequence_number())) {
        break;
      }
    }
    if (!need_insert) {
      continue;
    }
    std::unique_ptr<CachedPacket> new_cached_packet =
        std::make_unique<CachedPacket>();
    new_cached_packet->rtp_buffer = (*iter_received_packet)->LoadPacket();
    new_cached_packet->rtp_struct = std::move((*iter_received_packet));
    new_cached_packet->DecodeFecInfo(config_->payload_type());
    cached_packets_.insert(pos_insert, std::move(new_cached_packet));
  }
  received_packets.clear();
}

UlpFecDecoder::CachedPacket::CachedPacket()
    : rtp_struct(nullptr),
      rtp_buffer(nullptr),
      is_fec(false),
      invalid_fec(false) {}

UlpFecDecoder::CachedPacket::~CachedPacket() = default;

void UlpFecDecoder::CachedPacket::DecodeFecInfo(uint8_t fec_pt) {
  if (rtp_struct->payload_type() != fec_pt) {
    is_fec = false;
    return;
  }
  is_fec = true;
  const uint8_t* fec_level_0_header =
      rtp_buffer->Get() + RtpPacket::kFixedBufferLength;
  const uint8_t* fec_level_1_header = fec_level_0_header + kUlpfecHeaderLength;
  bool l = (fec_level_0_header[0] & 0x40);
  uint8_t mask_length =
      (l ? kUlpfecPacketMaskSizeLBitSet : kUlpfecPacketMaskSizeLBitClear);
  const uint8_t* mask = fec_level_1_header + 2;
  uint16_t seq_base =
      ByteReader<uint16_t>::ReadBigEndian(fec_level_0_header + 2);
  for (uint8_t index_mask_byte = 0; index_mask_byte < mask_length;
       ++index_mask_byte) {
    const uint8_t mask_byte = mask[index_mask_byte];
    for (uint8_t index_mask_bit = 0; index_mask_bit < 8; ++index_mask_bit) {
      if ((((uint8_t)0x80) >> index_mask_bit) & mask_byte) {
        map_protected_seq_received.emplace(
            seq_base + 8 * index_mask_byte + index_mask_bit, false);
      }
    }
  }
  return;
}
}  // namespace qosrtp