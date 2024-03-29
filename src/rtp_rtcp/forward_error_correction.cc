#include "../include/forward_error_correction.h"

#include "./ulp_fec.h"

namespace qosrtp {
FecEncoderConfig::FecEncoderConfig() = default;

FecEncoderConfig::~FecEncoderConfig() = default;

class FecEncoderConfigImpl : public FecEncoderConfig {
 public:
  FecEncoderConfigImpl() : ssrc_(0), payload_type_(0) {}
  virtual ~FecEncoderConfigImpl() = default;
  virtual std::unique_ptr<Result> Configure(uint32_t ssrc,
                                            uint8_t payload_type) override {
    if ((((uint8_t)1) << RtpPacket::kBitsizePayloadType) <= payload_type) {
      return Result::Create(-1, "payload_type invalid value");
    }
    ssrc_ = ssrc;
    payload_type_ = payload_type;
    return Result::Create();
  }
  virtual uint32_t ssrc() const override { return ssrc_; }
  virtual uint8_t payload_type() const override { return payload_type_; }

 private:
  uint32_t ssrc_;
  uint8_t payload_type_;
};

std::unique_ptr<FecEncoderConfig> FecEncoderConfig::Create() {
  return std::make_unique<FecEncoderConfigImpl>();
}

std::unique_ptr<FecEncoder> FecEncoder::Create(FecType fec_type) {
  std::unique_ptr<FecEncoder> ret = nullptr;
  if (FecType::kUlp == fec_type) {
    ret = std::make_unique<UlpFecEncoder>();
  }
  return ret;
}

FecEncoder::FecEncoder() = default;

FecEncoder::~FecEncoder() = default;

class FecDecoderConfigImpl : public FecDecoderConfig {
 public:
  FecDecoderConfigImpl()
      : max_cache_seq_difference_(0), ssrc_(0), payload_type_(0) {}
  virtual ~FecDecoderConfigImpl() = default;
  // Cache range [seq_latest - max_cache_seq_difference, seq_latest]
  virtual std::unique_ptr<Result> Configure(uint16_t max_cache_seq_difference,
                                            uint32_t ssrc,
                                            uint8_t payload_type) override {
    if ((((uint8_t)1) << RtpPacket::kBitsizePayloadType) <= payload_type) {
      return Result::Create(-1, "payload_type invalid value");
    }
    max_cache_seq_difference_ = max_cache_seq_difference;
    ssrc_ = ssrc;
    payload_type_ = payload_type;
    return Result::Create();
  }
  virtual uint16_t max_cache_seq_difference() const override {
    return max_cache_seq_difference_;
  }
  virtual uint32_t ssrc() const override { return ssrc_; }
  virtual uint8_t payload_type() const override { return payload_type_; }

 private:
  uint16_t max_cache_seq_difference_;
  uint32_t ssrc_;
  uint8_t payload_type_;
};

std::unique_ptr<FecDecoderConfig> FecDecoderConfig::Create() {
  return std::make_unique<FecDecoderConfigImpl>();
}

FecDecoderConfig::FecDecoderConfig() = default;

FecDecoderConfig::~FecDecoderConfig() = default;

std::unique_ptr<FecDecoder> FecDecoder::Create(FecType fec_type) {
  std::unique_ptr<FecDecoder> ret = nullptr;
  if (FecType::kUlp == fec_type) {
    ret = std::make_unique<UlpFecDecoder>();
  }
  return ret;
}

FecDecoder::FecDecoder() = default;

FecDecoder::~FecDecoder() = default;
}  // namespace qosrtp