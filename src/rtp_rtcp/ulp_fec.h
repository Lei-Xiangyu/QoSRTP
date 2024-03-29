#pragma once
#include "../include/forward_error_correction.h"

#include <map>

namespace qosrtp {
// FEC Level 0 Header, 10 bytes.
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |E|L|P|X|  CC   |M| PT recovery |            SN base            |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                          TS recovery                          |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |        length recovery        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// FEC Level 1 Header, 4 bytes (L = 0) or 8 bytes (L = 1).
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |       Protection Length       |             mask              |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |              mask cont. (present only when L = 1)             |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// Maximum number of media packets that can be protected
// by these packet masks.
static constexpr size_t kUlpfecMaxMediaPackets = 48;
// Packet mask size in bytes (given L bit).
static constexpr size_t kUlpfecPacketMaskSizeLBitClear = 2;
static constexpr size_t kUlpfecPacketMaskSizeLBitSet = 6;
// Packet code mask maximum length. kFECPacketMaskMaxSize = MaxNumFECPackets *
// (kUlpfecMaxMediaPackets / 8), and MaxNumFECPackets is equal to maximum
// number of media packets (kUlpfecMaxMediaPackets)
static constexpr size_t kFECPacketMaskMaxSize = 288;
static constexpr size_t kUlpfecHeaderLength = 10;

class UlpFecEncoder : public FecEncoder {
 public:
  UlpFecEncoder();
  virtual ~UlpFecEncoder();
  virtual std::unique_ptr<Result> Configure(
      std::unique_ptr<FecEncoderConfig> config) override;
  virtual std::unique_ptr<Result> Encode(
      const std::vector<const RtpPacket*>& protected_packets,
      uint32_t num_important_packets,
      ImportantPacketsProtectionMethod method_protect_important_packets,
      uint8_t protection_factor, FecMaskType fec_mask_type,
      std::vector<std::unique_ptr<RtpPacket>>& fec_packets) override;

 private:
  struct RtpPacketData {
   public:
    RtpPacketData(const RtpPacket* param_packet_struct,
                  std::unique_ptr<DataBuffer> param_packet_buffer);
    ~RtpPacketData();
    const RtpPacket* packet_struct;
    std::unique_ptr<DataBuffer> packet_buffer;
  };
  uint32_t NumFecPackets(uint32_t num_media_packets, uint8_t protection_factor);
  void GeneratePacketMasks(
      int num_media_packets, int num_fec_packets, int num_imp_packets,
      ImportantPacketsProtectionMethod method_protect_important_packets,
      FecMaskType fec_mask_type, uint8_t packet_mask_size,
      uint8_t* packet_mask);
  std::unique_ptr<DataBuffer> Mask(int num_media_packets, int num_fec_packets,
                                   FecMaskType fec_mask_type,
                                   uint8_t packet_mask_size,
                                   const uint8_t* mask_table);
  const uint8_t* PickTable(FecMaskType fec_mask_type, int num_media_packets);
  std::unique_ptr<DataBuffer> LookUpInFecTable(const uint8_t* table,
                                               int media_packet_index,
                                               int fec_index);
  void UnequalProtectionMask(
      int num_media_packets, int num_fec_packets, int num_imp_packets,
      ImportantPacketsProtectionMethod method_protect_important_packets,
      const uint8_t* mask_table, FecMaskType fec_mask_type,
      uint8_t packet_mask_size, uint8_t* packet_mask);
  uint8_t PacketMaskSize(size_t num_sequence_numbers) {
    return (num_sequence_numbers > (kUlpfecPacketMaskSizeLBitClear * 8))
               ? kUlpfecPacketMaskSizeLBitSet
               : kUlpfecPacketMaskSizeLBitClear;
  }
  uint8_t SetProtectionAllocation(uint8_t num_media_packets,
                                  uint8_t num_fec_packets,
                                  uint8_t num_imp_packets);
  void ImportantPacketProtection(int num_fec_for_imp_packets,
                                 int num_imp_packets, int num_mask_bytes,
                                 FecMaskType fec_mask_type,
                                 uint8_t* packet_mask,
                                 const uint8_t* mask_table);
  void FitSubMask(int num_mask_bytes, int num_sub_mask_bytes, int num_rows,
                  const uint8_t* sub_mask, uint8_t* packet_mask);
  void ShiftFitSubMask(int num_mask_bytes, int res_mask_bytes,
                       int num_column_shift, int end_row,
                       const uint8_t* sub_mask, uint8_t* packet_mask);
  void RemainingPacketProtection(int num_media_packets, int num_fec_remaining,
                                 int num_fec_for_imp_packets,
                                 int num_mask_bytes,
                                 ImportantPacketsProtectionMethod mode,
                                 FecMaskType fec_mask_type,
                                 uint8_t* packet_mask,
                                 const uint8_t* mask_table);
  void GenerateFecPackets(
      const std::vector<std::unique_ptr<RtpPacketData>>& media_packets,
      uint8_t num_fec_packets, uint8_t packet_mask_size,
      std::vector<std::unique_ptr<RtpPacket>>& fec_packets);
  std::unique_ptr<RtpPacket> BuildFecPacket(
      std::vector<const RtpPacketData*>& group_media_packets,
      uint16_t max_length);
  std::unique_ptr<FecEncoderConfig> config_;
  uint8_t fec_packet_mask_[kFECPacketMaskMaxSize];
};

class UlpFecDecoder : public FecDecoder {
 public:
  UlpFecDecoder();
  virtual ~UlpFecDecoder();
  virtual std::unique_ptr<Result> Configure(
      std::unique_ptr<FecDecoderConfig> config) override;
  // Input:  received_packets   List of new received packets.
  //                            At output the list will be empty,
  //                            with packets either stored internally,
  //                            or accessible through the recovered list.
  // Output: recovered_packets  List of recovered media packets.
  //
  virtual void Decode(
      std::vector<std::unique_ptr<RtpPacket>> received_packets,
      std::vector<std::unique_ptr<RtpPacket>>& recovered_packets) override;
  virtual void Flush(std::vector<std::unique_ptr<RtpPacket>>& output_packets) override;

 private:
  struct CachedPacket {
   public:
    CachedPacket();
    ~CachedPacket();
    void DecodeFecInfo(uint8_t fec_pt);
    std::unique_ptr<RtpPacket> rtp_struct;
    std::unique_ptr<DataBuffer> rtp_buffer;
    bool is_fec;
    std::map<uint16_t, bool> map_protected_seq_received;
    std::vector<const CachedPacket*> media_packets;
    bool invalid_fec;
  };
  void CachePackets(std::vector<std::unique_ptr<RtpPacket>>& received_packets);
  void RcoverPackets();
  // The rationality of the parameters has been ensured when passing in, and no
  // checks other than the rationality of the fec length will be performed in
  // the function.
  std::unique_ptr<CachedPacket> FecRecoverPacket(
      const CachedPacket* fec_packet, uint16_t recovered_seq,
      const std::vector<const CachedPacket*>& media_packets);
  std::unique_ptr<FecDecoderConfig> config_;
  std::vector<std::unique_ptr<CachedPacket>> cached_packets_;
  bool has_output_;
  uint16_t seq_last_output_;
};
}  // namespace qosrtp
