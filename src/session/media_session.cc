#include "media_session.h"

#include "../include/log.h"

namespace qosrtp {
std::unique_ptr<Result> MediaSession::Initialize(
    const MediaSessionConfig* config, Thread* signal_thread,
    Thread* worker_thread, RtpRtcpTranceiver* rtp_rtcp_tranceiver,
    RtpRtcpRouter* rtp_rtcp_router) {
  if ((nullptr == config) || (nullptr == worker_thread) ||
      (nullptr == signal_thread) || (nullptr == rtp_rtcp_tranceiver)) {
    return Result::Create(-1, "Parameter cannot be a nullptr");
  }
  config_ = config;
  worker_thread_ = worker_thread;
  signal_thread_ = signal_thread;
  rtp_rtcp_tranceiver_ = rtp_rtcp_tranceiver;
  std::unique_ptr<RtpSenderConfig> rtp_sender_config = nullptr;
  if (MediaTransmissionDirection::kRecvOnly != config_->direction()) {
    rtp_sender_config = std::make_unique<RtpSenderConfig>();
  }
  std::unique_ptr<RtpReceiverConfig> rtp_receiver_config = nullptr;
  if (MediaTransmissionDirection::kSendOnly != config_->direction()) {
    rtp_receiver_config = std::make_unique<RtpReceiverConfig>();
  }
  std::unique_ptr<RtcpReceiverConfig> rtcp_receiver_config =
      std::make_unique<RtcpReceiverConfig>();
  std::unique_ptr<RtcpSenderConfig> rtcp_sender_config =
      std::make_unique<RtcpSenderConfig>();
  std::unique_ptr<Result> error_result = nullptr;
  std::unique_ptr<Result> result = nullptr;
  if (MediaTransmissionDirection::kRecvOnly != config_->direction()) {
    rtp_sender_config->local_ssrc = config_->ssrc_media_local();
    rtp_sender_config->rtp_clock_rate_hz = config_->rtp_clock_rate_hz_local();
    rtp_sender_config->rtp_payload_types = config_->rtp_payload_types_local();
    if (config_->rtx_config_local()) {
      rtp_sender_config->rtx_enabled = true;
      rtp_sender_config->rtx_ssrc = config_->rtx_config_local()->ssrc();
      rtp_sender_config->max_cache_seq_difference =
          config_->rtx_config_local()->max_cache_seq_difference();
      rtp_sender_config->map_rtx_payload_type =
          config_->rtx_config_local()->map_rtx_payload_type();
    }
    rtp_sender_ = std::make_unique<RtpSender>();
    result = rtp_sender_->Initialize(this, rtp_rtcp_tranceiver_,
                                     std::move(rtp_sender_config));
    if (!result->ok()) {
      QOSRTP_LOG(Error, "Failed to initialize rtp sender, because: %s",
                 result->description().c_str());
      error_result = Result::Create(-1, "Failed to initialize rtp sender");
      goto failed;
    }
  }
  rtcp_sender_config->local_ssrc = config_->ssrc_media_local();
  rtcp_sender_config->local_cname = cname_;
  rtcp_sender_config->remote_ssrc = config_->ssrc_media_remote();
  rtcp_sender_config->direction = config_->direction();
  rtcp_sender_config->rtcp_report_interval_ms =
      config_->rtcp_report_interval_ms();
  rtcp_sender_ = std::make_unique<RtcpSender>();
  result = rtcp_sender_->Initialize(worker_thread_, this, rtp_rtcp_tranceiver_,
                                    std::move(rtcp_sender_config));
  if (!result->ok()) {
    QOSRTP_LOG(Error, "Failed to initialize rtcp sender, because: %s",
               result->description().c_str());
    error_result = Result::Create(-1, "Failed to initialize rtcp sender");
    goto failed;
  }
  rtcp_receiver_config->local_ssrc = config_->ssrc_media_local();
  rtcp_receiver_config->remote_ssrc = config_->ssrc_media_remote();
  rtcp_receiver_ = std::make_unique<RtcpReceiver>();
  result = rtcp_receiver_->Initialize(this, std::move(rtcp_receiver_config));
  if (!result->ok()) {
    QOSRTP_LOG(Error, "Failed to initialize rtcp receiver, because: %s",
               result->description().c_str());
    error_result = Result::Create(-1, "Failed to initialize rtcp receiver");
    goto failed;
  }
  rtp_rtcp_router_ = rtp_rtcp_router;
  if (MediaTransmissionDirection::kSendOnly != config_->direction()) {
    rtp_receiver_config->remote_ssrc = config_->ssrc_media_remote();
    rtp_receiver_config->rtp_clock_rate_hz =
        config_->rtp_clock_rate_hz_remote();
    rtp_receiver_config->rtp_payload_types =
        config_->rtp_payload_types_remote();
    rtp_receiver_config->max_cache_duration_ms =
        config_->max_cache_duration_ms();
    if (config_->rtx_config_remote()) {
      rtp_receiver_config->rtx_enabled = true;
      rtp_receiver_config->rtx_ssrc = config_->rtx_config_remote()->ssrc();
      rtp_receiver_config->rtx_max_cache_seq_difference =
          config_->rtx_config_remote()->max_cache_seq_difference();
      rtp_receiver_config->map_rtx_payload_type =
          config_->rtx_config_remote()->map_rtx_payload_type();
    }
    rtp_receiver_ = std::make_unique<RtpReceiver>();
    result = rtp_receiver_->Initialize(this, std::move(rtp_receiver_config));
    if (!result->ok()) {
      QOSRTP_LOG(Error, "Failed to initialize rtp receiver, because: %s",
                 result->description().c_str());
      error_result = Result::Create(-1, "Failed to initialize rtp receiver");
      goto failed;
    }
    rtp_rtcp_router_->AddRtpDst(rtp_receiver_.get());
  }
  rtp_rtcp_router_->AddRtcpDst(rtcp_receiver_.get());
  initialized_.store(true);
  return Result::Create();
failed:
  rtp_sender_.reset(nullptr);
  rtcp_sender_.reset(nullptr);
  rtcp_receiver_.reset(nullptr);
  rtp_receiver_.reset(nullptr);
  return error_result;
}

void MediaSession::SendRtpPacket(std::unique_ptr<RtpPacket> pkt) {
  if (!initialized_.load()) return;
  if (MediaTransmissionDirection::kRecvOnly == config_->direction()) return;
  if (!signal_thread_->IsCurrent()) {
    signal_thread_->PushTask(CallableWrapper::Wrap(&MediaSession::SendRtpPacket,
                                                   this, std::move(pkt)));
    return;
  }
  rtp_sender_->SendRtp(std::move(pkt));
}

void MediaSession::SendBye() {
  if (!initialized_.load()) return;
  rtcp_sender_->SendBye();
}

void MediaSession::NotifyByeReceived() {
  has_received_bye_.store(true);
}

void MediaSession::NotifyNackReceived(
    const std::vector<uint16_t>& packet_seqs) {
  if (!initialized_.load()) return;
  if (MediaTransmissionDirection::kRecvOnly == config_->direction()) return;
  rtp_sender_->SendRtx(packet_seqs);
}

bool MediaSession::HasSentRtp() {
  if (!initialized_.load()) return false;
  if (!has_sent_rtp_.load() &&
      (MediaTransmissionDirection::kRecvOnly != config_->direction())) {
    has_sent_rtp_.store(rtp_sender_->HasSentRtp());
  }
  return has_sent_rtp_.load();
}

bool MediaSession::HasReceivedRtp() {
  if (!initialized_.load()) return false;
  if (!has_received_rtp_.load() &&
      (MediaTransmissionDirection::kSendOnly != config_->direction())) {
    has_received_rtp_.store(rtp_receiver_->HasReceivedRtp());
  }
  return has_received_rtp_.load();
}

bool MediaSession::HasReceivedBye() {
  if (!initialized_.load()) return false;
  return has_received_bye_.load();
}

std::unique_ptr<RemoteSenderInfo> MediaSession::GetRemoteSenderInfo() {
  if (!initialized_.load() || !has_received_rtp_.load()) return nullptr;
  std::unique_ptr<RemoteSenderInfo> remote_sender_info =
      std::make_unique<RemoteSenderInfo>();
  std::unique_ptr<RtpReceiverStatistics> statistics =
      rtp_receiver_->GetRtpReceiverStatistics();
  remote_sender_info->remote_ssrc = statistics->remote_ssrc;
  remote_sender_info->cumulative_loss = statistics->cumulative_loss;
  remote_sender_info->extended_seq_num = statistics->extended_seq_num;
  remote_sender_info->first_extended_seq_num =
      statistics->first_extended_seq_num;
  remote_sender_info->interarrival_jitter = statistics->interarrival_jitter;
  rtcp_receiver_->GetSrInfo(remote_sender_info->lsr, remote_sender_info->dlsr);
  return remote_sender_info;
}

std::unique_ptr<LocalSenderInfo> MediaSession::GetLocalSenderInfo() {
  if (!initialized_.load() || !has_sent_rtp_.load()) return nullptr;
  std::unique_ptr<LocalSenderInfo> local_sender_info =
      std::make_unique<LocalSenderInfo>();
  rtp_sender_->GetStatisticInfo(local_sender_info->ntp_now,
                                local_sender_info->rtp_timestamp_now,
                                local_sender_info->sender_packet_count,
                                local_sender_info->sender_octet_count);
  return local_sender_info;
}

void MediaSession::NotifyLossPacketSeqsForNack(
    const std::vector<uint16_t>& loss_packet_seqs) {
  if (!initialized_.load()) return;
  rtcp_sender_->SendNack(loss_packet_seqs);
}

void MediaSession::OnRtpPacket(
    std::vector<std::unique_ptr<RtpPacket>> packets) {
  if (!initialized_.load()) return;
  if (!signal_thread_->IsCurrent()) {
    signal_thread_->PushTask(CallableWrapper::Wrap(&MediaSession::OnRtpPacket,
                                                   this, std::move(packets)));
    return;
  }
  config_->callback()->OnRtpPacket(std::move(packets));
}


MediaSession::MediaSession(const std::string& cname)
    : config_(nullptr),
      rtp_rtcp_tranceiver_(nullptr),
      rtp_rtcp_router_(nullptr),
      worker_thread_(nullptr),
      signal_thread_(nullptr),
      cname_(cname),
      rtp_sender_(nullptr),
      rtcp_sender_(nullptr),
      rtcp_receiver_(nullptr),
      rtp_receiver_(nullptr),
      initialized_(false),
      has_received_bye_(false),
      has_received_rtp_(false),
      has_sent_rtp_(false) {}

MediaSession::~MediaSession() {}
}  // namespace qosrtp