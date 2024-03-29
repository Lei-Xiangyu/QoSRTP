#include "./rtcp_sender.h"

#include <sstream>

#include "../utils/time_utils.h"
#include "../include/log.h"
#include "./sender_report.h"
#include "./receiver_report.h"
#include "./report_block.h"
#include "./sdes.h"
#include "./bye.h"
#include "./nack.h"

namespace qosrtp {
RtcpSenderConfig::RtcpSenderConfig() {
  local_ssrc = 0;
  remote_ssrc = 1;
  local_cname = "";
  rtcp_report_interval_ms = 1000;
  direction = MediaTransmissionDirection::kSendRecv;
}

RtcpSenderConfig::~RtcpSenderConfig() = default;

RemoteSenderInfo::RemoteSenderInfo() {
  remote_ssrc = 0;
  cumulative_loss = 0;
  extended_seq_num = 0;
  first_extended_seq_num = 0;
  interarrival_jitter = 0;
  lsr = 0;
  dlsr = 0;
}

RemoteSenderInfo::~RemoteSenderInfo() = default;

LocalSenderInfo::LocalSenderInfo() : ntp_now() {
  rtp_timestamp_now = 0;
  sender_packet_count = 0;
  sender_octet_count = 0;
}

LocalSenderInfo::~LocalSenderInfo() = default;

RtcpSenderCallback::RtcpSenderCallback() = default;

RtcpSenderCallback::~RtcpSenderCallback() = default;

RtcpSender::RtcpSender()
    : schedule_thread_(nullptr),
      sender_callback_(nullptr),
      tranceiver_(nullptr),
      config_(nullptr),
      last_report_remote_sender_info(),
      utc_ms_next_send_(0),
      has_sent_rtp_(false),
      has_sent_bye_(false),
      has_sent_rtcp_(false),
      has_received_rtp_(false) {}


RtcpSender::~RtcpSender() = default;

std::unique_ptr<Result> RtcpSender::Initialize(
    Thread* schedule_thread, RtcpSenderCallback* sender_callback,
    RtpRtcpTranceiver* tranceiver, std::unique_ptr<RtcpSenderConfig> config) {
  if ((nullptr == schedule_thread) || (nullptr == sender_callback) ||
      (nullptr == tranceiver) || (nullptr == config)) {
    return Result::Create(-1, "Parameter cannot be a nullptr");
  }
  schedule_thread_ = schedule_thread;
  sender_callback_ = sender_callback;
  tranceiver_ = tranceiver;
  config_ = std::move(config);
  utc_ms_next_send_ = UTCTimeMillis() + (config_->rtcp_report_interval_ms >> 1);
  schedule_thread_->PushTask(
      CallableWrapper::Wrap(&RtcpSender::ScheduleSendRtcp, this),
      (config_->rtcp_report_interval_ms >> 1));
  return Result::Create();
}

void RtcpSender::SendBye() {
  SendRtcpInfo send_rtcp_info;
  send_rtcp_info.nack = nullptr;
  send_rtcp_info.bye = true;
  std::lock_guard<std::mutex> lock(mutex_);
  SendRtcp(&send_rtcp_info);
  has_sent_bye_ = true;
}

void RtcpSender::SendNack(const std::vector<uint16_t>& nack_packet_seqs) {
  if (nack_packet_seqs.empty()) {
    return;
  }
  SendRtcpInfo send_rtcp_info;
  send_rtcp_info.nack =
      std::make_unique<SendRtcpInfo::NackInfo>(nack_packet_seqs);
  send_rtcp_info.bye = false;
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_sent_bye_ || sender_callback_->HasReceivedBye()) {
    return;
  }
  std::stringstream string_nack_packet_seqs;
  for (auto& seq : nack_packet_seqs) {
    string_nack_packet_seqs << " " << seq;
  }
  QOSRTP_LOG(Trace, "Send nack seqs:%s", string_nack_packet_seqs.str().c_str());
  SendRtcp(&send_rtcp_info);
  utc_ms_next_send_ = UTCTimeMillis() + config_->rtcp_report_interval_ms;
}

void RtcpSender::ScheduleSendRtcp() {
  uint64_t utc_ms_now = UTCTimeMillis();
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_sent_bye_ || sender_callback_->HasReceivedBye()) {
    return;
  }
  if (utc_ms_now < utc_ms_next_send_) {
    schedule_thread_->PushTask(
        CallableWrapper::Wrap(&RtcpSender::ScheduleSendRtcp, this),
        utc_ms_next_send_ - utc_ms_now);
    return;
  }
  SendRtcp(nullptr);
  utc_ms_next_send_ = utc_ms_now + config_->rtcp_report_interval_ms;
  schedule_thread_->PushTask(
      CallableWrapper::Wrap(&RtcpSender::ScheduleSendRtcp, this),
      config_->rtcp_report_interval_ms);
  return;
}

RtcpSender::SendRtcpInfo::SendRtcpInfo() : bye(false), nack(nullptr) {}

RtcpSender::SendRtcpInfo::~SendRtcpInfo() = default;

RtcpSender::SendRtcpInfo::NackInfo::NackInfo(
    const std::vector<uint16_t>& seqs_para)
    : seqs(seqs_para){};

RtcpSender::SendRtcpInfo::NackInfo::~NackInfo() = default;

void RtcpSender::SendRtcp(SendRtcpInfo* send_rtcp_info) { 
  if (MediaTransmissionDirection::kRecvOnly != config_->direction &&
      !has_sent_rtp_) {
    has_sent_rtp_ = sender_callback_->HasSentRtp();
  }
  if (MediaTransmissionDirection::kSendOnly != config_->direction &&
      !has_received_rtp_) {
    has_received_rtp_ = sender_callback_->HasReceivedRtp();
  }
  bool b_sr = has_sent_rtp_;
  std::vector<std::unique_ptr<rtcp::RtcpPacket>> rtcp_packets;
  std::vector<std::unique_ptr<rtcp::ReportBlock>> report_blocks;
  if (has_received_rtp_) {
    std::unique_ptr<RemoteSenderInfo> remote_sender_info =
        sender_callback_->GetRemoteSenderInfo();
    std::unique_ptr<rtcp::ReportBlock> report_block =
        std::make_unique<rtcp::ReportBlock>();
    report_block->SetMediaSsrc(remote_sender_info->remote_ssrc);
    report_block->SetCumulativeLost(remote_sender_info->cumulative_loss);
    int32_t loss_since_last_report = remote_sender_info->cumulative_loss;
    uint32_t expected_since_last_report =
        remote_sender_info->extended_seq_num -
        remote_sender_info->first_extended_seq_num;
    if (has_sent_rtcp_) {
      loss_since_last_report -= last_report_remote_sender_info.cumulative_loss;
      expected_since_last_report =
          remote_sender_info->extended_seq_num -
          last_report_remote_sender_info.extended_seq_num;
    }
    if ((expected_since_last_report == 0) || (loss_since_last_report <= 0)) {
      report_block->SetFractionLost(0);
    } else {
      report_block->SetFractionLost(static_cast<uint8_t>(
          (loss_since_last_report << 8) / expected_since_last_report));
    }
    last_report_remote_sender_info = *remote_sender_info;
    report_block->SetExtHighestSeqNum(remote_sender_info->extended_seq_num);
    report_block->SetJitter(remote_sender_info->interarrival_jitter);
    report_block->SetLastSr(remote_sender_info->lsr);
    report_block->SetDelayLastSr(remote_sender_info->dlsr);
    report_blocks.push_back(std::move(report_block));
  }
  if (b_sr) {
    std::unique_ptr<rtcp::SenderReport> sr_packet =
        std::make_unique<rtcp::SenderReport>();
    sr_packet->SetSenderSsrc(config_->local_ssrc);
    sr_packet->SetReportBlocks(std::move(report_blocks));
    std::unique_ptr<LocalSenderInfo> local_sender_info =
        sender_callback_->GetLocalSenderInfo(); 
    sr_packet->SetPacketCount(local_sender_info->sender_packet_count);
    sr_packet->SetOctetCount(local_sender_info->sender_octet_count);
    sr_packet->SetRtpTimestamp(local_sender_info->rtp_timestamp_now);
    sr_packet->SetNtp(local_sender_info->ntp_now);
    rtcp_packets.push_back(std::move(sr_packet));
  } else {
    std::unique_ptr<rtcp::ReceiverReport> rr_packet =
        std::make_unique<rtcp::ReceiverReport>();
    rr_packet->SetSenderSsrc(config_->local_ssrc);
    rr_packet->SetReportBlocks(std::move(report_blocks));
    rtcp_packets.push_back(std::move(rr_packet));
  }
  std::unique_ptr<rtcp::Sdes> sdes_packet = std::make_unique<rtcp::Sdes>();
  sdes_packet->AddCName(config_->local_ssrc, config_->local_cname);
  rtcp_packets.push_back(std::move(sdes_packet));
  bool is_bye = false;
  if (send_rtcp_info) {
    if (send_rtcp_info->bye) {
      is_bye = true;
      std::unique_ptr<rtcp::Bye> bye_packet = std::make_unique<rtcp::Bye>();
      bye_packet->SetSenderSsrc(config_->local_ssrc);
      rtcp_packets.push_back(std::move(bye_packet));
    } else if (send_rtcp_info->nack != nullptr) {
      std::unique_ptr<rtcp::Nack> nack_packet = std::make_unique<rtcp::Nack>();
      nack_packet->SetSenderSsrc(config_->local_ssrc);
      nack_packet->SetMediaSsrc(config_->remote_ssrc);
      nack_packet->SetPacketIds(send_rtcp_info->nack->seqs);
      rtcp_packets.push_back(std::move(nack_packet));
    }
  }
  tranceiver_->SendRtcp(std::move(rtcp_packets), is_bye);
  has_sent_rtcp_ = true;
}
}  // namespace qosrtp