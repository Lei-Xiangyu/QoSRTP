#include "rtp_sender.h"
#include "../include/log.h"
#include "../utils/byte_io.h"
#include "../utils/seq_comparison.h"
#include "../utils/time_utils.h"

#include <random>
#include <algorithm>

#ifdef max
#undef max
#endif  // max

namespace qosrtp {
RtpSenderConfig::RtpSenderConfig() {
  max_cache_seq_difference = 0;
  local_ssrc = 0;
  rtx_ssrc = 0;
  rtx_enabled = false;
  rtp_clock_rate_hz = 1;
}

RtpSenderConfig::~RtpSenderConfig() = default;

RtpSenderCallback::RtpSenderCallback() = default;

RtpSenderCallback::~RtpSenderCallback() = default;

RtpSenderPacketCache::RtpSenderPacketCache(uint16_t max_cache_seq_difference)
    : max_cache_seq_difference_(max_cache_seq_difference) {}

RtpSenderPacketCache::~RtpSenderPacketCache() = default;

void RtpSenderPacketCache::PutPacket(std::unique_ptr<RtpPacket> packet) {
  for (auto iter_cached_packet = cached_rtp_packets_.begin();
       iter_cached_packet != cached_rtp_packets_.end();) {
    if (IsSeqBeforeInRange((*iter_cached_packet)->sequence_number(),
                           packet->sequence_number(),
                           max_cache_seq_difference_)) {
      break;
    }
    iter_cached_packet = cached_rtp_packets_.erase(iter_cached_packet);
  }
  cached_rtp_packets_.push_back(std::move(packet));
}

void RtpSenderPacketCache::GetPackets(
    const std::vector<uint16_t>& packet_seqs,
    std::vector<std::unique_ptr<RtpPacket>>& out_packets) {
  for (auto iter_seq = packet_seqs.begin(); iter_seq != packet_seqs.end();
       ++iter_seq) {
    for (auto iter_cached_packet = cached_rtp_packets_.begin();
         iter_cached_packet != cached_rtp_packets_.end();
         ++iter_cached_packet) {
      if ((*iter_cached_packet)->sequence_number() == (*iter_seq)) {
        out_packets.push_back(RtpPacket::Create((*iter_cached_packet).get()));
        break;
      }
    }
  }
}

RtpSender::RtpSender()
    : rtx_context(),
      sender_callback_(nullptr),
      tranceiver_(nullptr),
      config_(nullptr),
      cache_(nullptr),
      has_sent_(false),
      last_seq_(0),
      utc_ms_first_(0),
      rtp_timestamp_first_(0),
      rtp_clock_rate_(0),
      sender_packet_count_(0),
      sender_octet_count_(0) {}

RtpSender::~RtpSender() = default;

RtpSender::RtxContext::RtxContext() {
  last_seq = 0;
  has_sent = false;
}

RtpSender::RtxContext::~RtxContext() = default;

std::unique_ptr<Result> RtpSender::Initialize(
    RtpSenderCallback* sender_callback, RtpRtcpTranceiver* tranceiver,
    std::unique_ptr<RtpSenderConfig> config) {
  if ((nullptr == sender_callback) || (nullptr == tranceiver) ||
      (nullptr == config)) {
    return Result::Create(-1, "Parameter cannot be a nullptr");
  }
  sender_callback = sender_callback;
  tranceiver_ = tranceiver;
  config_ = std::move(config);
  if (config_->rtx_enabled) {
    if (config_->max_cache_seq_difference == 0) {
      return Result::Create(
          -1,
          "The number of caches cannot be 0 when the rtx function is enabled");
    }
    cache_ = std::make_unique<RtpSenderPacketCache>(
        config_->max_cache_seq_difference);
  }
  return Result::Create();
}

void RtpSender::SendRtp(std::unique_ptr<RtpPacket> packet) {
  if (nullptr == packet) return;
  if (config_->local_ssrc != packet->ssrc()) {
    QOSRTP_LOG(
        Error,
        "The ssrc (%u) of the rtp packet given to RtpSender does not match "
        "the set ssrc(%u)",
        packet->ssrc(), config_->local_ssrc);
    return;
  }
  uint8_t m_payload_type_octet = packet->payload_type();
  auto iter_rtp_clock_rate = std::find(config_->rtp_payload_types.begin(),
                config_->rtp_payload_types.end(), packet->payload_type());
  if (iter_rtp_clock_rate == config_->rtp_payload_types.end()) {
    QOSRTP_LOG(Error,
               "The payload_type (%u) of the rtp packet given to RtpSender "
               "does not match the set payload_type",
               packet->payload_type());
    return;
  }
  //if (config_->rtx_enabled) {
  //  auto iter_rtx_type =
  //      std::find_if(config_->map_rtx_payload_type.begin(),
  //                   config_->map_rtx_payload_type.end(),
  //                   [&m_payload_type_octet](const auto& element) {
  //                     return element.second == m_payload_type_octet;
  //                   });
  //  if (iter_rtx_type == config_->map_rtx_payload_type.end()) {
  //    QOSRTP_LOG(Warning,
  //               "The payload_type (%u) of the rtp packet given to RtpSender "
  //               "does not match the set payload_type",
  //               packet->payload_type());
  //    return;
  //  }
  //}
  std::lock_guard<std::mutex> lock(mutex_);
  uint16_t seq = packet->sequence_number();
  if (has_sent_ && (seq != ((last_seq_ == std::numeric_limits<uint16_t>::max())
                                ? 0
                                : (last_seq_ + 1)))) {
    QOSRTP_LOG(
        Error,
        "The seq of the sent rtp is not the next one sent last time, ssrc(%u)",
        config_->local_ssrc);
    return;
  }
  if (config_->rtx_enabled) {
    auto iter_rtx_type =
        std::find_if(config_->map_rtx_payload_type.begin(),
                     config_->map_rtx_payload_type.end(),
                     [&m_payload_type_octet](const auto& element) {
                       return element.second == m_payload_type_octet;
                     });
    if (iter_rtx_type != config_->map_rtx_payload_type.end()) {
      std::unique_ptr<RtpPacket> cached_packet =
          RtpPacket::Create(packet.get());
      cache_->PutPacket(std::move(cached_packet));
    }
  }
  uint64_t utc_ms_now = UTCTimeMillis();
  if (!has_sent_.load()) {
    utc_ms_first_ = utc_ms_now;
    rtp_timestamp_first_ = packet->timestamp();
    rtp_clock_rate_ = config_->rtp_clock_rate_hz;
  }
  sender_packet_count_++;
  sender_octet_count_ +=
      packet->GetPayloadBuffer() ? packet->GetPayloadBuffer()->size() : 0;
  tranceiver_->SendRtp(std::move(packet));
  last_seq_ = seq;
  has_sent_.store(true);
}

void RtpSender::SendRtx(const std::vector<uint16_t>& packet_seqs) {
  if (!(config_->rtx_enabled)) return;
  if (packet_seqs.empty() || (nullptr == cache_)) return;
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::unique_ptr<RtpPacket>> cached_packets;
  for (auto iter_seq = packet_seqs.begin(); iter_seq != packet_seqs.end();
       ++iter_seq) {
    QOSRTP_LOG(Info, "Receive loss seq: %hu to send rtx", (*iter_seq));
  }
  cache_->GetPackets(packet_seqs, cached_packets);
  for (auto iter_packet = cached_packets.begin();
       iter_packet != cached_packets.end();) {
    std::unique_ptr<RtpPacket>& packet = (*iter_packet);
    std::unique_ptr<RtpPacket> packet_rtx = ConstructRtx(std::move(packet));
    //QOSRTP_LOG(Info,
    //           "Send rtx, rtx_ssrc: %u, rtx_payload_type: %hhu, rtx_seq: %hu",
    //           packet_rtx->ssrc(), packet_rtx->payload_type(),
    //           packet_rtx->sequence_number());
    if (nullptr != packet_rtx) tranceiver_->SendRtp(std::move(packet_rtx));
    iter_packet = cached_packets.erase(iter_packet);
  }
}

std::unique_ptr<RtpPacket> RtpSender::ConstructRtx(
    std::unique_ptr<RtpPacket> packet) {
  uint8_t m_payload_type_octet = packet->payload_type();
  auto iter_rtx_type =
      std::find_if(config_->map_rtx_payload_type.begin(),
                   config_->map_rtx_payload_type.end(),
                   [&m_payload_type_octet](const auto& element) {
                     return element.second == m_payload_type_octet;
                   });
  if (iter_rtx_type == config_->map_rtx_payload_type.end()) {
    QOSRTP_LOG(Error,
               "The rtx payload type corresponding to the payload type cannot "
               "be found");
    return nullptr;
  }
  m_payload_type_octet = iter_rtx_type->first;
  std::unique_ptr<RtpPacket> packet_rtx = RtpPacket::Create();
  m_payload_type_octet |= packet->m() ? 0x80 : 0x00;
  std::vector<uint32_t> csrcs;
  uint8_t count_csrcs = packet->count_csrcs();
  uint8_t csrc = 0;
  for (uint8_t i = 0; i < count_csrcs; i++) {
    std::unique_ptr<Result> result = packet->csrc(csrc, i);
    if (!result->ok()) {
      QOSRTP_LOG(Error, "Failed to get csrc, because: %s",
                 result->description().c_str());
      return nullptr;
    }
    csrcs.push_back(csrc);
  }
  std::unique_ptr<RtpPacket::Extension> extension_copy = nullptr;
  if (packet->x()) {
    const RtpPacket::Extension* extension_src = packet->GetExtension();
    extension_copy =
        std::make_unique<RtpPacket::Extension>(extension_src->length);
    memcpy(extension_copy->name, extension_src->name, 2);
    extension_copy->content->ModifyAt(0, extension_src->content->Get(),
                                      extension_copy->content->size());
  }
  std::unique_ptr<DataBuffer> payload_buffer_rtx =
      DataBuffer::Create(packet->GetPayloadBuffer()->size() + sizeof(uint16_t));
  payload_buffer_rtx->SetSize(payload_buffer_rtx->capacity());
  ByteWriter<uint16_t>::WriteBigEndian(payload_buffer_rtx->GetW(),
                             packet->sequence_number());
  payload_buffer_rtx->ModifyAt(sizeof(uint16_t),
                               packet->GetPayloadBuffer()->Get(),
                               packet->GetPayloadBuffer()->size());
  if (!rtx_context.has_sent) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dis(
        0, std::numeric_limits<uint16_t>::max());
    rtx_context.last_seq = dis(gen);
  }
  uint16_t rtx_seq = rtx_context.last_seq;
  if (std::numeric_limits<uint16_t>::max() == rtx_seq)
    rtx_seq = 0;
  else
    rtx_seq++;
  auto result = packet_rtx->StorePacket(
      m_payload_type_octet, rtx_seq, packet->timestamp(), config_->rtx_ssrc, csrcs,
      std::move(extension_copy), std::move(payload_buffer_rtx),
      packet->pad_size());
  if (!result->ok()) {
    QOSRTP_LOG(Error, "Failed to construct rtx packet, because: %s",
               result->description().c_str());
    return nullptr;
  }
  rtx_context.has_sent = true;
  rtx_context.last_seq = rtx_seq;
  return packet_rtx;
}

bool RtpSender::GetStatisticInfo(NtpTime& ntp_now, uint32_t& rtp_timestamp_now,
                                 uint32_t& sender_packet_count,
                                 uint32_t& sender_octet_count) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!has_sent_.load()) return false;
  uint64_t utc_ms_now = UTCTimeMillis();
  ntp_now = NtpTime(NtpTimeNow());
  rtp_timestamp_now =
      rtp_timestamp_first_ + (((double)(utc_ms_now - utc_ms_first_)) *
                              ((double)rtp_clock_rate_ / 1000.0));
  sender_packet_count = sender_packet_count_;
  sender_octet_count = sender_octet_count_;
  return true;
}
}  // namespace qosrtp