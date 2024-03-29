#include "rtp_receiver.h"

#include <limits>
#include <algorithm>

#include "../include/log.h"
#include "../utils/byte_io.h"
#include "../utils/seq_comparison.h"
#include "../utils/time_utils.h"

#ifdef max
#undef max
#endif  // max

namespace qosrtp {
RtpReceiverCallback::RtpReceiverCallback() = default;

RtpReceiverCallback::~RtpReceiverCallback() = default;

RtpReceiverConfig::RtpReceiverConfig() {
  rtx_max_cache_seq_difference = 0;
  remote_ssrc = 0;
  rtx_ssrc = 0;
  rtx_enabled = false;
  max_cache_duration_ms = 0;
  rtp_clock_rate_hz = 1;
  max_cache_duration_ms = 0;
}

RtpReceiverConfig::~RtpReceiverConfig() = default;

RtpReceiverPacketCache::CachedRTPPacket::CachedRTPPacket(
    std::unique_ptr<RtpPacket> param_packet,
    uint64_t param_packet_timeout_time_utc_ms)
    : packet(std::move(param_packet)),
      packet_timeout_time_utc_ms(param_packet_timeout_time_utc_ms) {}

RtpReceiverPacketCache::CachedRTPPacket::~CachedRTPPacket() = default;

RtpReceiverPacketCache::LossPacketSequenceNumber::LossPacketSequenceNumber(
    uint16_t param_seq)
    : seq(param_seq), notified(false), last_notify_time_utc_ms(0) {}

RtpReceiverPacketCache::LossPacketSequenceNumber::~LossPacketSequenceNumber() =
    default;

RtpReceiverPacketCache::RtpReceiverPacketCache(uint16_t max_cache_duration_ms)
    : max_cache_duration_ms_(max_cache_duration_ms),
      latest_callback_seq_(0),
      has_callback_packet_(false),
      cumulative_packets_Loss_(0),
      extended_first_seq_(0),
      extended_highest_seq_(0),
      has_cached_packet_(false) {}

RtpReceiverPacketCache::~RtpReceiverPacketCache() = default;

void RtpReceiverPacketCache::PutPacket(std::unique_ptr<RtpPacket> packet) {
  uint16_t packet_seq = packet->sequence_number();
  if (has_callback_packet_) {
    // It must be the rtp packet after the rtp packet that has been called
    // back
    if (!IsSeqAfter(latest_callback_seq_, packet_seq)) {
      return;
    }
  }
  if (has_cached_packet_) {
    uint16_t num_extended = (extended_highest_seq_ >> 16);
    uint16_t latest_cached_seq = (extended_highest_seq_ << 16 >> 16);
    if (IsSeqAfter(latest_cached_seq, packet_seq)) {
      if (packet_seq < latest_cached_seq) {
        ++num_extended;
      }
      extended_highest_seq_ =
          ((((uint32_t)num_extended) << 16) | ((uint32_t)packet_seq));
    }
    if ((0 == num_extended) && (packet_seq < extended_first_seq_)) {
      extended_first_seq_ = packet_seq;
    }
  } else {
    extended_highest_seq_ = packet_seq;
    extended_first_seq_ = extended_highest_seq_;
  }
  uint64_t utc_ms_now = UTCTimeMillis();
  uint64_t packet_timeout_time_utc_ms = max_cache_duration_ms_ + utc_ms_now;
  if (cached_packets_.empty()) {
    cached_packets_.push_front(std::make_unique<CachedRTPPacket>(
        std::move(packet), packet_timeout_time_utc_ms));
  } else {
    if (IsSeqAfter(packet_seq,
                   cached_packets_.back()->packet->sequence_number())) {
      cached_packets_.push_back(std::make_unique<CachedRTPPacket>(
          std::move(packet), packet_timeout_time_utc_ms));
    } else {
      for (auto iter = cached_packets_.begin(); iter != cached_packets_.end();
           iter++) {
        if ((*iter)->packet->sequence_number() == packet_seq) break;
        if (IsSeqAfter((*iter)->packet->sequence_number(), packet_seq)) {
          cached_packets_.insert(
              iter, std::make_unique<CachedRTPPacket>(
                        std::move(packet), packet_timeout_time_utc_ms));
          break;
        }
      }
    }
  }
  for (auto iter_loss_seq = loss_seqs_.begin();
       iter_loss_seq != loss_seqs_.end(); ++iter_loss_seq) {
    if ((*iter_loss_seq)->seq == packet_seq) {
      loss_seqs_.erase(iter_loss_seq);
      QOSRTP_LOG(Trace, "Receive loss packet seq: %hu", packet_seq);
      break;
    }
  }
  SupplementLossSeqs();
  has_cached_packet_ = true;
}

void RtpReceiverPacketCache::SupplementLossSeqs() {
  auto iter_cached_packet = cached_packets_.begin();
  uint16_t last_seq = (*iter_cached_packet)->packet->sequence_number();
  uint16_t this_seq = (*iter_cached_packet)->packet->sequence_number();
  ++iter_cached_packet;
  auto iter_insert_pos = loss_seqs_.begin();
  auto func_get_loss_seqs = [this, &iter_insert_pos](const uint16_t& last_seq,
                                       const uint16_t& this_seq) {
    if (this_seq < last_seq) {
      for (uint16_t loss_seq = last_seq - 1; loss_seq > this_seq; --loss_seq) {
        bool need_insert = true;
        for (; iter_insert_pos != loss_seqs_.end(); ++iter_insert_pos) {
          if (loss_seq == (*iter_insert_pos)->seq) {
            need_insert = false;
            break;
          }
          if (!IsSeqBefore(loss_seq, (*iter_insert_pos)->seq)) {
            break;
          }
        }
        if (need_insert) {
          loss_seqs_.insert(
              iter_insert_pos,
              std::make_unique<LossPacketSequenceNumber>(loss_seq));
          ++cumulative_packets_Loss_;
        }
      }
    } else /*if (this_seq > last_seq)*/ {
      for (uint16_t loss_seq = last_seq - 1; loss_seq > 0; --loss_seq) {
        bool need_insert = true;
        for (; iter_insert_pos != loss_seqs_.end(); ++iter_insert_pos) {
          if (loss_seq == (*iter_insert_pos)->seq) {
            need_insert = false;
            break;
          }
          if (!IsSeqBefore(loss_seq, (*iter_insert_pos)->seq)) {
            break;
          }
        }
        if (need_insert) {
          loss_seqs_.insert(
              iter_insert_pos,
              std::make_unique<LossPacketSequenceNumber>(loss_seq));
          ++cumulative_packets_Loss_;
        }
      }
      for (uint16_t loss_seq = std::numeric_limits<uint16_t>::max();
           loss_seq > this_seq; --loss_seq) {
        bool need_insert = true;
        for (; iter_insert_pos != loss_seqs_.end(); ++iter_insert_pos) {
          if (loss_seq == (*iter_insert_pos)->seq) {
            need_insert = false;
            break;
          }
          if (!IsSeqBefore(loss_seq, (*iter_insert_pos)->seq)) {
            break;
          }
        }
        if (need_insert) {
          loss_seqs_.insert(
              iter_insert_pos,
              std::make_unique<LossPacketSequenceNumber>(loss_seq));
          ++cumulative_packets_Loss_;
        }
      }
    }
  };
  for (; iter_cached_packet != cached_packets_.end(); ++iter_cached_packet) {
    this_seq = (*iter_cached_packet)->packet->sequence_number();
    // this_seq < last_seq
    if (!IsNextSeq(this_seq, last_seq)) {
      func_get_loss_seqs(last_seq, this_seq);
    }
    last_seq = this_seq;
  }
  this_seq = latest_callback_seq_;
  if (has_callback_packet_ && !IsNextSeq(this_seq, last_seq)) {
    func_get_loss_seqs(last_seq, this_seq);
  }
  if (has_callback_packet_) {
    for (auto iter_loss_seq = loss_seqs_.begin();
         iter_loss_seq != loss_seqs_.end();) {
      if (!IsSeqBefore(latest_callback_seq_, (*iter_loss_seq)->seq)) {
        iter_loss_seq = loss_seqs_.erase(iter_loss_seq);
        continue;
      }
      ++iter_loss_seq;
    }
  }
}

void RtpReceiverPacketCache::GetPackets(
  std::vector<std::unique_ptr<RtpPacket>>& packets) {
   uint64_t utc_ms_now = UTCTimeMillis();
  auto iter_latest_ready_packet = cached_packets_.end();
  if (has_callback_packet_) {
    int32_t nb_ready_packet = 0;
    auto reversed_iter_latest_ready_packet = cached_packets_.rbegin();
    uint16_t latest_ready_seq = latest_callback_seq_;
    for (; reversed_iter_latest_ready_packet != cached_packets_.rend();
         ++reversed_iter_latest_ready_packet) {
      if (!IsNextSeq(latest_ready_seq, (*reversed_iter_latest_ready_packet)
                                           ->packet->sequence_number())) {
        break;
      }
      latest_ready_seq =
          (*reversed_iter_latest_ready_packet)->packet->sequence_number();
      ++nb_ready_packet;
    }
    std::advance(iter_latest_ready_packet, -nb_ready_packet);
  }
   bool should_get = false;
   for (auto iter_cached_packet = cached_packets_.begin();
        iter_cached_packet != cached_packets_.end();) {
     if (!should_get) {
       if ((iter_latest_ready_packet == iter_cached_packet) ||
           ((*iter_cached_packet)->packet_timeout_time_utc_ms <= utc_ms_now)) {
         should_get = true;
         latest_callback_seq_ =
             (*iter_cached_packet)->packet->sequence_number();
         has_callback_packet_ = true;
       }
     }
     if (should_get) {
       packets.insert(packets.begin(),
                      std::move((*iter_cached_packet)->packet));
       iter_cached_packet = cached_packets_.erase(iter_cached_packet);
       continue;
     }
     ++iter_cached_packet;
   }
}

void RtpReceiverPacketCache::GetLossPacketSeqsForNack(
    std::vector<uint16_t>& loss_packet_seqs) {
   uint64_t utc_ms_now = UTCTimeMillis();
   for (auto riter_loss_seq = loss_seqs_.rbegin();
        riter_loss_seq != loss_seqs_.rend(); ++riter_loss_seq) {
     if (!(*riter_loss_seq)->notified) {
       (*riter_loss_seq)->notified = true;
       (*riter_loss_seq)->last_notify_time_utc_ms = utc_ms_now;
       loss_packet_seqs.push_back((*riter_loss_seq)->seq);
     } else if (((*riter_loss_seq)->last_notify_time_utc_ms +
                 kNackIntervalMs) <= utc_ms_now) {
       (*riter_loss_seq)->last_notify_time_utc_ms = utc_ms_now;
       loss_packet_seqs.push_back((*riter_loss_seq)->seq);
     }
   }
}

RtpReceiver::RtpReceiver()
    : receiver_callback_(nullptr),
      config_(nullptr),
      packet_cache_(nullptr),
      has_received_(false),
      rtp_receiver_statistics_(nullptr),
      nb_received_expected_(0),
      nb_received_real_(0) {}

RtpReceiver::~RtpReceiver() = default;

std::unique_ptr<Result> RtpReceiver::Initialize(
    RtpReceiverCallback* receiver_callback,
    std::unique_ptr<RtpReceiverConfig> config) {
  if ((nullptr == receiver_callback) || (nullptr == config)) {
    return Result::Create(-1, "Parameter cannot be a nullptr");
  }
  receiver_callback_ = receiver_callback;
  config_ = std::move(config);
  if (config_->rtx_enabled) {
    if (config_->rtx_max_cache_seq_difference == 0) {
      return Result::Create(
          -1,
          "The number of caches cannot be 0 when the rtx function is enabled");
    }
  }
  packet_cache_ = std::make_unique<RtpReceiverPacketCache>(
      config_->max_cache_duration_ms);
  rtp_receiver_statistics_ = std::make_unique<RtpReceiverStatistics>();
  rtp_receiver_statistics_->remote_ssrc = config_->remote_ssrc;
  return Result::Create();
}

bool RtpReceiver::IsExpectedRemoteSsrc(uint32_t ssrc) const {
  if ((ssrc == config_->remote_ssrc) ||
      ((config_->rtx_enabled) && (ssrc == config_->rtx_ssrc))) {
    return true;
  }
  return false;
}

void RtpReceiver::OnRtpPacket(std::unique_ptr<RtpPacket> packet) {
  uint64_t trace_1 = UTCTimeMillis();
  if (config_->rtx_enabled) {
    if (packet->ssrc() == config_->rtx_ssrc) {
      packet = ReconstructRtpFromRtx(std::move(packet));
      if (nullptr == packet) {
        QOSRTP_LOG(Error, "Failed to reconstruct rtp from rtx.");
        return;
      }
    }
  }
  if (packet->ssrc() != config_->remote_ssrc) {
    QOSRTP_LOG(Warning,
               "Rtp receiver received an rtp packet whose seq is neither "
               "remote_ssrc nor rtx_ssrc.");
    return;
  }
  uint8_t m_payload_type_octet = packet->payload_type();
  //if (config_->rtx_enabled) {
  //  auto iter_rtx_type =
  //      std::find_if(config_->map_rtx_payload_type.begin(),
  //                   config_->map_rtx_payload_type.end(),
  //                   [&m_payload_type_octet](const auto& element) {
  //                     return element.second == m_payload_type_octet;
  //                   });
  //  if (iter_rtx_type == config_->map_rtx_payload_type.end()) {
  //    QOSRTP_LOG(Error, "Received rtp packet with unknown payload type.");
  //    return;
  //  }
  //}
  auto iter_rtp_payload_type =
      std::find(config_->rtp_payload_types.begin(),
                config_->rtp_payload_types.end(), packet->payload_type());
  if (iter_rtp_payload_type == config_->rtp_payload_types.end()) {
    QOSRTP_LOG(Error, "Received rtp packet with unknown payload type.");
    return;
  }
  uint64_t utc_now_ms = UTCTimeMillis();
  if (has_received_.load()) {
    int32_t duration_utc_ms =
        utc_now_ms - interarrival_jitter_info.last_rtp_utc_ms;
    int64_t duration_rtp_ts =
        (int64_t)packet->timestamp() -
        (int64_t)interarrival_jitter_info.last_rtp_timestamp;
    int32_t d = std::abs(duration_rtp_ts -
                         duration_utc_ms *
                             (((double)config_->rtp_clock_rate_hz) / 1000.0));
    interarrival_jitter_info.interarrival_jitter +=
        ((d - ((int32_t)interarrival_jitter_info.interarrival_jitter)) / 16);
  } else {
    interarrival_jitter_info.interarrival_jitter = 0;
  }
  interarrival_jitter_info.last_rtp_utc_ms = utc_now_ms;
  interarrival_jitter_info.last_rtp_timestamp = packet->timestamp();
  std::vector<std::unique_ptr<RtpPacket>> packets;
  std::vector<uint16_t> loss_packet_seqs;
  uint64_t trace_2 = UTCTimeMillis();
  packet_cache_->PutPacket(std::move(packet));
  {
    std::lock_guard<std::mutex> lock(mutex_);
    rtp_receiver_statistics_->first_extended_seq_num =
        packet_cache_->extended_first_seq();
    rtp_receiver_statistics_->extended_seq_num =
        packet_cache_->extended_highest_seq();
    nb_received_expected_ = rtp_receiver_statistics_->extended_seq_num -
                            rtp_receiver_statistics_->first_extended_seq_num;
    ++nb_received_real_;
    int32_t cumulative_loss = nb_received_expected_ - nb_received_real_;
    rtp_receiver_statistics_->cumulative_loss =
        (cumulative_loss > 0) ? cumulative_loss : 0;
    rtp_receiver_statistics_->interarrival_jitter =
        interarrival_jitter_info.interarrival_jitter;
  }
  has_received_.store(true);
  uint64_t trace_3 = UTCTimeMillis();
  packet_cache_->GetLossPacketSeqsForNack(loss_packet_seqs);
  uint64_t trace_4 = UTCTimeMillis();
  receiver_callback_->NotifyLossPacketSeqsForNack(loss_packet_seqs);
  uint64_t trace_5 = UTCTimeMillis();
  packet_cache_->GetPackets(packets);
  uint64_t trace_6 = UTCTimeMillis();
  if (packets.empty()) {
    //QOSRTP_LOG(Trace,
    //           "RtpReceiver::OnRtpPacket inner cost: (%lld %lld %lld %lld %lld"
    //           ") ms",
    //           trace_2 - trace_1, trace_3 - trace_2, trace_4 - trace_3,
    //           trace_5 - trace_4, trace_6 - trace_5);
    return;
  }
  receiver_callback_->OnRtpPacket(std::move(packets));
  uint64_t trace_7 = UTCTimeMillis();
  //QOSRTP_LOG(
  //    Trace,
  //    "RtpReceiver::OnRtpPacket inner cost: (%lld %lld %lld %lld %lld %lld) ms",
  //    trace_2 - trace_1, trace_3 - trace_2, trace_4 - trace_3,
  //    trace_5 - trace_4, trace_6 - trace_5, trace_7 - trace_6);
}

std::unique_ptr<RtpPacket> RtpReceiver::ReconstructRtpFromRtx(
    std::unique_ptr<RtpPacket> packet) {
  auto iter = config_->map_rtx_payload_type.find(packet->payload_type());
  if (iter == config_->map_rtx_payload_type.end()) {
    QOSRTP_LOG(Error, "Received rtx packet with unknown payload type.");
    return nullptr;
  }
  uint8_t payload_type_reconstruct = iter->second;
  const DataBuffer* rtx_payload_buffer = packet->GetPayloadBuffer();
  if (nullptr == rtx_payload_buffer) {
    QOSRTP_LOG(Error, "Received rtx packet with null payload.");
    return nullptr;
  }
  if (rtx_payload_buffer->size() <= sizeof(uint16_t)) {
    QOSRTP_LOG(Error,
               "The payload size of the rtx package must be larger than "
               "sizeof(uint16_t).");
    return nullptr;
  }
  uint16_t seq_reconstruct =
      ByteReader<uint16_t>::ReadBigEndian(rtx_payload_buffer->Get());
  std::unique_ptr<DataBuffer> payload_buffer_reconstruct =
      DataBuffer::Create(rtx_payload_buffer->size() - sizeof(uint16_t));
  payload_buffer_reconstruct->SetSize(payload_buffer_reconstruct->capacity());
  payload_buffer_reconstruct->ModifyAt(
      0, rtx_payload_buffer->Get() + sizeof(uint16_t),
      payload_buffer_reconstruct->size());
  payload_type_reconstruct |= packet->m() ? 0x80 : 0x00;
  std::vector<uint32_t> csrcs;
  uint8_t count_csrcs = packet->count_csrcs();
  uint8_t csrc = 0;
  for (uint8_t i = 0; i < count_csrcs; i++) {
    std::unique_ptr<Result> result = packet->csrc(csrc, i);
    if (!result->ok()) {
      QOSRTP_LOG(Error, "Failed to get csrc, because: %s.",
                 result->description().c_str());
      return nullptr;
    }
    csrcs.push_back(csrc);
  }
  std::unique_ptr<RtpPacket::Extension> extension_reconstruct = nullptr;
  if (packet->x()) {
    const RtpPacket::Extension* extension_rtx = packet->GetExtension();
    extension_reconstruct =
        std::make_unique<RtpPacket::Extension>(extension_rtx->length);
    memcpy(extension_reconstruct->name, extension_rtx->name, 2);
    extension_reconstruct->content->ModifyAt(
        0, extension_rtx->content->Get(),
        extension_reconstruct->content->size());
  }
  std::unique_ptr<RtpPacket> packet_reconstruct = RtpPacket::Create();
  packet_reconstruct->StorePacket(payload_type_reconstruct, seq_reconstruct,
                                  packet->timestamp(), config_->remote_ssrc,
                                  csrcs, std::move(extension_reconstruct),
                                  std::move(payload_buffer_reconstruct), 0);

  QOSRTP_LOG(Trace,
             "Reconstruct packet (ssrc: %u, pt: %hhu, seq: %hu), from packet "
             "(ssrc: %u, pt: %hhu, seq: %hu)",
             packet_reconstruct->ssrc(), packet_reconstruct->payload_type(),
             packet_reconstruct->sequence_number(), packet->ssrc(),
             packet->payload_type(), packet->sequence_number());
  return packet_reconstruct;
}

std::unique_ptr<RtpReceiverStatistics> RtpReceiver::GetRtpReceiverStatistics() {
  if (!has_received_.load()) return nullptr;
  return std::make_unique<RtpReceiverStatistics>(*rtp_receiver_statistics_);
}
}  // namespace qosrtp