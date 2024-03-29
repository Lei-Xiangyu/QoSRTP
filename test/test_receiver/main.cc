#include <atomic>
#include <iostream>
#include <thread>
#include <chrono>

#include "qosrtp.h"
#include "../../src/utils/time_utils.h"

static const struct {
  uint32_t src_ssrc = 123;
  std::string src_ip = "127.0.0.1";
  uint16_t src_port = 7777;
  uint32_t dst_ssrc = 789;
  std::string dst_ip = "127.0.0.1";
  uint16_t dst_port = 6666;
  qosrtp::TransportProtocolType protocol_type =
      qosrtp::TransportProtocolType::kUdp;
  qosrtp::MediaTransmissionDirection direction =
      qosrtp::MediaTransmissionDirection::kRecvOnly;
  uint32_t rtp_clock_rate_hz_remote = 1000;
  std::vector<uint8_t> rtp_payload_types_remote = {0};
  uint16_t max_cache_duration_ms = 40;
  uint32_t rtcp_report_interval_ms = 1000;
  std::string cname = "test_receiver";
  std::string media_session_name = "test";
} global_config;

class Receiver : qosrtp::MediaSessionCallback {
 public:
  Receiver() : qosrtp_session_(nullptr) {
    qosrtp_session_ = ConstructQosrtpSession();
    if (!qosrtp_session_) {
      std::cout << "Failed to construct qosrtp session" << std::endl;
    }
  }
  ~Receiver() = default;
  virtual void OnRtpPacket(
      std::vector<std::unique_ptr<qosrtp::RtpPacket>> packets) override {
    for (auto iter_packet = packets.begin(); iter_packet != packets.end();
         iter_packet++) {
      QOSRTP_LOG(Trace, "Receive rtp packet, seq:%hu ts: %u, content: %s",
                 (*iter_packet)->sequence_number(), (*iter_packet)->timestamp(),
                 (const char*)((*iter_packet)->GetPayloadBuffer()->At(0)))
    }
    return;
  }

 private:
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
        global_config.src_ssrc, nullptr, nullptr, nullptr,
        global_config.dst_ssrc, nullptr,
        &global_config.rtp_clock_rate_hz_remote,
        &global_config.rtp_payload_types_remote,
        &global_config.max_cache_duration_ms, global_config.direction,
        global_config.rtcp_report_interval_ms, this);
    session_config->AddMediaSessionConfig(global_config.media_session_name,
                                          std::move(media_session_config));
    std::unique_ptr<qosrtp::QosrtpSession> qosrtp_session =
        qosrtp::QosrtpSession::Create();
    if (!qosrtp_session->StartSession(std::move(session_config))->ok())
      return nullptr;
    return qosrtp_session;
  }
  std::unique_ptr<qosrtp::QosrtpSession> qosrtp_session_;
};

int main(int argc, char* argv[]) {
  qosrtp::QosrtpInterface::Initialize(nullptr,
                                      qosrtp::QosrtpLogger::Level::kTrace);
  Receiver* test_receiver = new Receiver();
  std::cout << "Any key will exit!" << std::endl;
  std::cin.get();
  delete test_receiver;
  qosrtp::QosrtpInterface::UnInitialize();
  return 0;
}