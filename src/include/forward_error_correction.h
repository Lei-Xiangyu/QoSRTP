#pragma once
#include "./rtp_packet.h"

namespace qosrtp {
enum class QOSRTP_API FecType {
  kUlp,
};

enum class QOSRTP_API FecMaskType {
  kFecMaskRandom,
  kFecMaskBursty,
};

enum class QOSRTP_API ImportantPacketsProtectionMethod {
  kNone,
  kModeNoOverlap,
  kModeOverlap,
  kModeBiasFirstPacket,
};

class QOSRTP_API FecEncoderConfig {
 public:
  static std::unique_ptr<FecEncoderConfig> Create();
  FecEncoderConfig();
  virtual ~FecEncoderConfig();
  virtual std::unique_ptr<Result> Configure(uint32_t ssrc,
                                            uint8_t payload_type) = 0;
  virtual uint32_t ssrc() const = 0;
  virtual uint8_t payload_type() const = 0;
};

class QOSRTP_API FecEncoder {
 public:
  static std::unique_ptr<FecEncoder> Create(FecType fec_type);
  FecEncoder();
  virtual ~FecEncoder();
  virtual std::unique_ptr<Result> Configure(
      std::unique_ptr<FecEncoderConfig> config) = 0;

  // Generates a list of FEC packets from supplied packets.
  //
  // Input:  protected_packets                 List of media packets to protect.
  //                                           All packets must belong to the
  //                                           same frame, have the same SSRC as
  //                                           the config, and the list must not
  //                                           be empty. The sequence number
  //                                           must be continuously increasing.
  // Input:  num_important_packets             The number of "important" packets
  //                                           in the protected_packets. These
  //                                           packets may receive greater
  //                                           protection than the remaining
  //                                           packets. The important packets
  //                                           must be located at the start of
  //                                           the protected_packets. For codecs
  //                                           with data partitioning, the
  //                                           important packets may correspond
  //                                           to first partition packets.
  // Input:  method_protect_important_packets  Methods to protect important
  //                                           packets. If num_important_packets
  //                                           > 0, it needs to be set. If it is
  //                                           set to kNone, it will be set 
  //                                           automatically.
  // Input:  protection_factor                 FEC protection overhead in the
  //                                           [0, 255] domain. To obtain 100%
  //                                           overhead, or an equal number of
  //                                           FEC packets as media packets, use
  //                                           255.
  // Input:  fec_mask_type                     The type of packet mask used in
  //                                           the FEC. Random or bursty type
  //                                           may be selected. The bursty type
  //                                           is only defined up to 12 media
  //                                           packets. If the number of media
  //                                           packets is above 12, the packet
  //                                           masks from the random table will
  //                                           be selected.
  // Output: fec_packets                       The packets in fec_packets must
  //                                           be set timestamp and seq by the
  //                                           caller
  //
  virtual std::unique_ptr<Result> Encode(
      const std::vector<const RtpPacket*>& protected_packets,
      uint32_t num_important_packets,
      ImportantPacketsProtectionMethod method_protect_important_packets,
      uint8_t protection_factor, FecMaskType fec_mask_type,
      std::vector<std::unique_ptr<RtpPacket>>& fec_packets) = 0;
};

class QOSRTP_API FecDecoderConfig {
 public:
  static std::unique_ptr<FecDecoderConfig> Create();
  FecDecoderConfig();
  virtual ~FecDecoderConfig();
  // Cache range [seq_latest - max_cache_seq_difference, seq_latest]
  virtual std::unique_ptr<Result> Configure(uint16_t max_cache_seq_difference,
                                            uint32_t ssrc,
                                            uint8_t payload_type) = 0;
  virtual uint16_t max_cache_seq_difference() const = 0;
  virtual uint32_t ssrc() const = 0;
  virtual uint8_t payload_type() const = 0;
};

class QOSRTP_API FecDecoder {
 public:
  static std::unique_ptr<FecDecoder> Create(FecType fec_type);
  FecDecoder();
  virtual ~FecDecoder();
  virtual std::unique_ptr<Result> Configure(
      std::unique_ptr<FecDecoderConfig> config) = 0;
  // Input:  received_packets   List of new received packets.
  //                            At output the list will be empty,
  //                            with packets either stored internally,
  //                            or accessible through the recovered list.
  // Output: recovered_packets  List of recovered media packets.
  //
  virtual void Decode(
      std::vector<std::unique_ptr<RtpPacket>> received_packets,
      std::vector<std::unique_ptr<RtpPacket>>& recovered_packets) = 0;
  virtual void Flush(std::vector<std::unique_ptr<RtpPacket>>& packets) = 0;
};
}  // namespace qosrtp