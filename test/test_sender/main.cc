#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "qosrtp.h"
#include "../../src/utils/time_utils.h"

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
  uint32_t rtp_clock_rate_hz_local = 1000;
  std::vector<uint8_t> rtp_payload_types_local = {0};
  uint32_t rtcp_report_interval_ms = 1000;
  std::string cname = "test_sender";
  std::string media_session_name = "test";
} global_config;

class Sender : qosrtp::MediaSessionCallback {
 public:
  Sender() : stop_signal_(false), qosrtp_session_(nullptr) {
    sender_thread_ = std::thread(&Sender::SenderThreadMain, this);
  }
  ~Sender() {
    stop_signal_.store(true);
    sender_thread_.join();
  }
  virtual void OnRtpPacket(
      std::vector<std::unique_ptr<qosrtp::RtpPacket>> packets) override {
    return;
  }

 private:
  static constexpr double kDataRateMbps = 2.5;
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
    std::unique_ptr<qosrtp::MediaSessionConfig> media_session_config =
        qosrtp::MediaSessionConfig::Create();
    media_session_config->Configure(
        global_config.src_ssrc, nullptr, &global_config.rtp_clock_rate_hz_local,
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
    std::vector<uint32_t> csrcs;
    uint64_t time_now = qosrtp::UTCTimeMillis();
    uint64_t time_first = 0;
    uint16_t seq_frame = 0;
    uint16_t seq_packet = 0;
    uint32_t compressed_frame_size_bytes = kDataRateMbps * 1024.0 * 1024.0 / 8;
    uint16_t per_packet_payload_size_bytes = 1400;
    while (!stop_signal_.load()) {
      time_now = qosrtp::UTCTimeMillis();
      if ((seq_frame > 0) &&
          (time_now < (seq_frame * kTimeDurationMs + time_first))) {
        std::this_thread::sleep_for(std::chrono::milliseconds(3));
        continue;
      }
      if (seq_frame == 0) time_first = time_now;
      uint64_t timestamp_now = seq_frame * kTimeDurationMs + time_first;
      std::vector<std::unique_ptr<qosrtp::RtpPacket>> send_packets;
      for (uint32_t sent_compressed_frame_data_size_bytes = 0;
           sent_compressed_frame_data_size_bytes <
           compressed_frame_size_bytes / (1000 / kTimeDurationMs);
           sent_compressed_frame_data_size_bytes +=
           per_packet_payload_size_bytes) {
        std::unique_ptr<qosrtp::RtpPacket> pkt = qosrtp::RtpPacket::Create();
        std::unique_ptr<qosrtp::DataBuffer> payload_buffer =
            qosrtp::DataBuffer::Create(per_packet_payload_size_bytes);
        payload_buffer->SetSize(per_packet_payload_size_bytes);
        payload_buffer->MemSet(0, 0, per_packet_payload_size_bytes);
        std::string packet_string =
            global_config.cname + "_" + std::to_string(seq_frame);
        payload_buffer->ModifyAt(0, (const uint8_t*)packet_string.c_str(),
                                 packet_string.size() + 1);
        pkt->StorePacket(0, seq_packet, *(uint32_t*)(&timestamp_now),
                         global_config.src_ssrc, csrcs, nullptr,
                         std::move(payload_buffer), 0);
        ++seq_packet;
        send_packets.push_back(std::move(pkt));
      }
      QOSRTP_LOG(Trace, "Frame seq: %hu, seq: %hu - %hu", seq_frame,
                 send_packets.front()->sequence_number(),
                 send_packets.back()->sequence_number());
      for (auto iter_packet = send_packets.begin();
           iter_packet != send_packets.end(); ++iter_packet) {
        qosrtp_session_->SendRtpPacket(std::move(*iter_packet));
      }
      seq_frame++;
    }
  }
  std::atomic<bool> stop_signal_;
  std::thread sender_thread_;
  std::unique_ptr<qosrtp::QosrtpSession> qosrtp_session_;
};

int main(int argc, char* argv[]) {
  qosrtp::QosrtpInterface::Initialize(nullptr,
                                      qosrtp::QosrtpLogger::Level::kTrace);
  Sender* test_sender = new Sender();
  std::cout << "Any key will exit!" << std::endl;
  std::cin.get();
  delete test_sender;
  qosrtp::QosrtpInterface::UnInitialize();
  return 0;
}