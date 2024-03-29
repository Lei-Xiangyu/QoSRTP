#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include "../../src/utils/time_utils.h"
#include "qosrtp.h"

static const struct {
  uint32_t src_ssrc = 789;
  std::string src_ip = "127.0.0.1";
  uint16_t src_port = 6666;
  uint32_t dst_ssrc = 123;
  std::string dst_ip = "127.0.0.1";
  uint16_t dst_port = 7777;
  qosrtp::TransportProtocolType protocol_type =
      qosrtp::TransportProtocolType::kUdp;
  qosrtp::MediaTransmissionDirection direction =
      qosrtp::MediaTransmissionDirection::kSendOnly;
  uint32_t rtcp_report_interval_ms = 1000;
  uint16_t local_rtx_max_cache_seq_difference = 100;
  uint32_t local_rtx_ssrc = 7890;
  uint32_t rtp_clock_rate_hz_local = 1000;
  std::vector<uint8_t> rtp_payload_types_local = {0};
  std::string cname = "test_sender_with_rtx";
  std::string media_session_name = "test";
} global_config;

class SenderWithRtx : qosrtp::MediaSessionCallback {
 public:
  SenderWithRtx() : stop_signal_(false), qosrtp_session_(nullptr) {
    sender_thread_ = std::thread(&SenderWithRtx::SenderThreadMain, this);
  }
  ~SenderWithRtx() {
    stop_signal_.store(true);
    sender_thread_.join();
  }
  virtual void OnRtpPacket(
      std::vector<std::unique_ptr<qosrtp::RtpPacket>> packets) override {
    return;
  }

 private:
  static constexpr uint32_t kTimeDurationMs = 40;
  std::unique_ptr<qosrtp::QosrtpSession> ConstructQosrtpSession() {
    std::unique_ptr<qosrtp::QosrtpSessionConfig> session_config =
        qosrtp::QosrtpSessionConfig::Create();
    std::unique_ptr<qosrtp::TransportAddress> src_address =
        qosrtp::TransportAddress::Create(global_config.src_ip,
                                         global_config.src_port,
                                         global_config.protocol_type);
    std::unique_ptr<qosrtp::TransportAddress> dst_address =
        qosrtp::TransportAddress::Create(global_config.dst_ip,
                                         global_config.dst_port,
                                         global_config.protocol_type);
    session_config->Configure(std::move(src_address), std::move(dst_address),
                              global_config.cname);
    std::unique_ptr<qosrtp::RtxConfig> local_rtx_config =
        qosrtp::RtxConfig::Create();
    local_rtx_config->Configure(
        global_config.local_rtx_max_cache_seq_difference,
        global_config.local_rtx_ssrc);
    local_rtx_config->AddRtxAndAssociatedPayloadType(1, 0);
    std::unique_ptr<qosrtp::MediaSessionConfig> media_session_config =
        qosrtp::MediaSessionConfig::Create();
    media_session_config->Configure(
        global_config.src_ssrc, local_rtx_config.get(),
        &global_config.rtp_clock_rate_hz_local,
        &global_config.rtp_payload_types_local, global_config.dst_ssrc, nullptr,
        nullptr, nullptr, nullptr,
        qosrtp::MediaTransmissionDirection::kSendOnly,
        global_config.rtcp_report_interval_ms, this);
    session_config->AddMediaSessionConfig(global_config.media_session_name,
                                          std::move(media_session_config));
    std::unique_ptr<qosrtp::QosrtpSession> qosrtp_session =
        qosrtp::QosrtpSession::Create();
    if (!qosrtp_session->StartSession(std::move(session_config))->ok())
      return nullptr;
    return qosrtp_session;
  }
  void SenderThreadMain() {
    qosrtp_session_ = ConstructQosrtpSession();
    if (!qosrtp_session_) {
      std::cout << "Failed to construct qosrtp session" << std::endl;
      return;
    }
    uint64_t time_now = qosrtp::UTCTimeMillis();
    uint64_t time_first = qosrtp::UTCTimeMillis();
    uint32_t packet_count = 0;
    std::vector<uint32_t> csrcs;
    while (!stop_signal_.load()) {
      time_now = qosrtp::UTCTimeMillis();
      if ((packet_count > 0) &&
          (time_now < (packet_count * kTimeDurationMs + time_first))) {
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        continue;
      }
      if (packet_count == 0) time_first = time_now;
      std::unique_ptr<qosrtp::RtpPacket> pkt = qosrtp::RtpPacket::Create();
      uint64_t timestamp_now = packet_count * kTimeDurationMs + time_first;
      std::unique_ptr<qosrtp::DataBuffer> payload_buffer =
          qosrtp::DataBuffer::Create(1500);
      payload_buffer->SetSize(1500);
      payload_buffer->MemSet(0, 0, 1500);
      std::string packet_string =
          global_config.cname + "_" + std::to_string(packet_count);
      payload_buffer->ModifyAt(0, (const uint8_t*)packet_string.c_str(),
                               packet_string.size() + 1);
      pkt->StorePacket(0, packet_count, *(uint32_t*)(&timestamp_now),
                       global_config.src_ssrc, csrcs, nullptr,
                       std::move(payload_buffer), 0);
      qosrtp_session_->SendRtpPacket(std::move(pkt));
      packet_count++;
    }
  }
  std::atomic<bool> stop_signal_;
  std::thread sender_thread_;
  std::unique_ptr<qosrtp::QosrtpSession> qosrtp_session_;
};

int main(int argc, char* argv[]) {
  qosrtp::QosrtpInterface::Initialize(nullptr,
                                      qosrtp::QosrtpLogger::Level::kTrace);
  SenderWithRtx* test_sender_with_rtx = new SenderWithRtx();
  std::cout << "Any key will exit!" << std::endl;
  std::cin.get();
  delete test_sender_with_rtx;
  qosrtp::QosrtpInterface::UnInitialize();
  return 0;
}