#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include "../../src/utils/time_utils.h"
#include "qosrtp.h"

static const struct {
  uint32_t local_ssrc = 789;
  std::string local_ip = "127.0.0.1";
  uint16_t local_port = 6666;
  uint32_t remote_ssrc = 123;
  std::string remote_ip = "127.0.0.1";
  uint16_t remote_port = 7777;
  qosrtp::TransportProtocolType protocol_type =
      qosrtp::TransportProtocolType::kUdp;
  qosrtp::MediaTransmissionDirection direction =
      qosrtp::MediaTransmissionDirection::kSendOnly;
  uint32_t rtcp_report_interval_ms = 1000;
  uint16_t local_rtx_max_cache_seq_difference = 100;
  uint32_t local_rtx_ssrc = 7890;
  uint32_t rtp_clock_rate_hz_local = 1000;
  std::vector<uint8_t> rtp_payload_types_local = {0, 2};
  std::string cname = "test_sender_with_fec";
  std::string media_session_name = "test";
  uint8_t local_payload_type = 0;
  uint8_t local_rtx_payload_type = 1;
  uint8_t local_fec_payload_type = 2;
} global_config;

class SenderWithFec : qosrtp::MediaSessionCallback {
 public:
  SenderWithFec() : stop_signal_(false) {
    sender_thread_ = std::thread(&SenderWithFec::SenderThreadMain, this);
  }
  ~SenderWithFec() {
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
    std::unique_ptr<qosrtp::TransportAddress> local_address =
        qosrtp::TransportAddress::Create(global_config.local_ip,
                                         global_config.local_port,
                                         global_config.protocol_type);
    std::unique_ptr<qosrtp::TransportAddress> remote_address =
        qosrtp::TransportAddress::Create(global_config.remote_ip,
                                         global_config.remote_port,
                                         global_config.protocol_type);
    session_config->Configure(std::move(local_address),
                              std::move(remote_address), global_config.cname);
    std::unique_ptr<qosrtp::RtxConfig> local_rtx_config =
        qosrtp::RtxConfig::Create();
    local_rtx_config->Configure(
        global_config.local_rtx_max_cache_seq_difference,
        global_config.local_rtx_ssrc);
    local_rtx_config->AddRtxAndAssociatedPayloadType(
        global_config.local_rtx_payload_type, global_config.local_payload_type);
    std::unique_ptr<qosrtp::MediaSessionConfig> media_session_config =
        qosrtp::MediaSessionConfig::Create();
    media_session_config->Configure(
        global_config.local_ssrc, local_rtx_config.get(),
        &global_config.rtp_clock_rate_hz_local,
        &global_config.rtp_payload_types_local, global_config.remote_ssrc, nullptr,
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
  std::unique_ptr<qosrtp::FecEncoder> ConstructFecEncoder() {
    std::unique_ptr<qosrtp::FecEncoderConfig> config =
        qosrtp::FecEncoderConfig::Create();
    config->Configure(global_config.local_ssrc,
                      global_config.local_fec_payload_type);
    std::unique_ptr<qosrtp::FecEncoder> fec_encoder =
        qosrtp::FecEncoder::Create(qosrtp::FecType::kUlp);
    if (!fec_encoder) {
      return nullptr;
    }
    std::unique_ptr<qosrtp::Result> result =
        fec_encoder->Configure(std::move(config));
    if (!result->ok()) {
      std::cout << "Failed to configure fec encoder, because: "
                << result->description() << std::endl;
      return nullptr;
    }
    return fec_encoder;
  }
  void SenderThreadMain() {
    std::vector<uint32_t> csrcs;
    std::unique_ptr<qosrtp::QosrtpSession> qosrtp_session =
        ConstructQosrtpSession();
    if (!qosrtp_session) {
      std::cout << "Failed to construct qosrtp session" << std::endl;
      return;
    }
    std::unique_ptr<qosrtp::FecEncoder> fec_encoder = ConstructFecEncoder();
    if (!fec_encoder) {
      std::cout << "Failed to construct fec encoder" << std::endl;
      return;
    }
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
      std::vector<const qosrtp::RtpPacket*> media_packets;
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
                         global_config.local_ssrc, csrcs, nullptr,
                         std::move(payload_buffer), 0);
        ++seq_packet;
        media_packets.push_back(pkt.get());
        send_packets.push_back(std::move(pkt));
        if (media_packets.size() == 48) {
          std::vector<std::unique_ptr<qosrtp::RtpPacket>> fec_packets;
          fec_encoder->Encode(
              media_packets, media_packets.size() >> 2,
              qosrtp::ImportantPacketsProtectionMethod::kModeOverlap, 255,
              qosrtp::FecMaskType::kFecMaskRandom, fec_packets);
          for (auto iter_fec_packet = fec_packets.begin();
               iter_fec_packet != fec_packets.end();
               ++iter_fec_packet) {
            (*iter_fec_packet)->SetTimestamp(media_packets.back()->timestamp());
            (*iter_fec_packet)->SetSequenceNumber(seq_packet);
            ++seq_packet;
            send_packets.push_back(std::move(*iter_fec_packet));
          }
          media_packets.clear();
        }
      }
      if (!media_packets.empty()) {
        std::vector<std::unique_ptr<qosrtp::RtpPacket>> fec_packets;
        fec_encoder->Encode(
            media_packets, media_packets.size() >> 2,
            qosrtp::ImportantPacketsProtectionMethod::kModeOverlap, 255,
            qosrtp::FecMaskType::kFecMaskRandom, fec_packets);
        for (auto iter_fec_packet = fec_packets.begin();
             iter_fec_packet != fec_packets.end(); ++iter_fec_packet) {
          (*iter_fec_packet)->SetTimestamp(media_packets.back()->timestamp());
          (*iter_fec_packet)->SetSequenceNumber(seq_packet);
          ++seq_packet;
          send_packets.push_back(std::move(*iter_fec_packet));
        }
        media_packets.clear();
      }
      for (auto iter_packet = send_packets.begin();
           iter_packet != send_packets.end(); ++iter_packet) {
        qosrtp_session->SendRtpPacket(std::move(*iter_packet));
      }
      seq_frame++;
    }
  }
  std::atomic<bool> stop_signal_;
  std::thread sender_thread_;
};

int main(int argc, char* argv[]) {
  qosrtp::QosrtpInterface::Initialize(nullptr,
                                      qosrtp::QosrtpLogger::Level::kTrace);
  SenderWithFec* test_sender_with_fec = new SenderWithFec();
  std::cout << "Any key will exit!" << std::endl;
  std::cin.get();
  delete test_sender_with_fec;
  qosrtp::QosrtpInterface::UnInitialize();
  return 0;
}