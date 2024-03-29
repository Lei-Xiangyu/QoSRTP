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
  std::vector<uint8_t> rtp_payload_types_remote = {0, 2};
  uint8_t remote_fec_payload_type = 2;
  uint16_t max_cache_duration_ms = 40;
  uint32_t rtcp_report_interval_ms = 1000;
  uint16_t remote_rtx_max_cache_seq_difference = 100;
  uint32_t remote_rtx_ssrc = 7890;
  uint16_t fec_max_cache_seq_difference = 48;
  std::string cname = "test_receiver_with_fec";
  std::string media_session_name = "test";
} global_config;

class ReceiverWithFec : qosrtp::MediaSessionCallback {
 public:
  ReceiverWithFec() : qosrtp_session_(nullptr), fec_decoder_(nullptr) {
    qosrtp_session_ = ConstructQosrtpSession();
    if (!qosrtp_session_) {
      std::cout << "Failed to construct qosrtp session" << std::endl;
    }
    fec_decoder_ = ConstructFecDecoder();
    if (!fec_decoder_) {
      std::cout << "Failed to construct fec decoder" << std::endl;
    }
  }
  ~ReceiverWithFec() {
    qosrtp_session_.reset(nullptr);
    std::vector<std::unique_ptr<qosrtp::RtpPacket>> packets;
    QOSRTP_LOG(Trace, "Flush!");
    if (fec_decoder_) {
      fec_decoder_->Flush(packets);
    }
    std::string invalid_content = "";
    const char* content = invalid_content.c_str();
    for (auto iter_packet = packets.begin(); iter_packet != packets.end();
         iter_packet++) {
      if ((*iter_packet)->payload_type() == 0) {
        content = (const char*)((*iter_packet)->GetPayloadBuffer()->At(0));
      } else {
        content = invalid_content.c_str();
      }
      QOSRTP_LOG(Trace,
                 "Receive rtp packet, pt: %hu, seq:%hu, ts: %u, content: %s",
                 (uint16_t)(*iter_packet)->payload_type(),
                 (*iter_packet)->sequence_number(), (*iter_packet)->timestamp(),
                 content);
    }
  }
  virtual void OnRtpPacket(
      std::vector<std::unique_ptr<qosrtp::RtpPacket>> packets) override {
    std::string invalid_content = "";
    const char* content = invalid_content.c_str();
    //for (auto iter_packet = packets.begin(); iter_packet != packets.end();
    //     iter_packet++) {
    //  if ((*iter_packet)->payload_type() == 0) {
    //    content = (const char*)((*iter_packet)->GetPayloadBuffer()->At(0));
    //  } else {
    //    content = invalid_content.c_str();
    //  }
    //  QOSRTP_LOG(Trace,
    //             "Receive rtp packet, pt: %hu, seq:%hu, ts: %u, content: %s",
    //             (uint16_t)(*iter_packet)->payload_type(),
    //             (*iter_packet)->sequence_number(), (*iter_packet)->timestamp(),
    //             content);
    //}
    uint64_t trace_begin = qosrtp::UTCTimeMillis();
    std::vector<std::unique_ptr<qosrtp::RtpPacket>> recovered_packets;
    fec_decoder_->Decode(std::move(packets), recovered_packets);
    uint64_t trace_end = qosrtp::UTCTimeMillis();
    QOSRTP_LOG(Trace, "fec_decoder decode cost: %lld ms",
               trace_end - trace_begin);
    for (auto iter_packet = recovered_packets.begin();
        iter_packet != recovered_packets.end(); iter_packet++) {
    if ((*iter_packet)->payload_type() == 0) {
      content = (const char*)((*iter_packet)->GetPayloadBuffer()->At(0));
    } else {
      content = invalid_content.c_str();
    }
    QOSRTP_LOG(Trace,
                "Recover rtp packet, pt: %hu, seq:%hu, ts: %u, content: %s",
                (uint16_t)(*iter_packet)->payload_type(),
                (*iter_packet)->sequence_number(), (*iter_packet)->timestamp(),
                content);
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
    std::unique_ptr<qosrtp::RtxConfig> remote_rtx_config =
        qosrtp::RtxConfig::Create();
    remote_rtx_config->Configure(
        global_config.remote_rtx_max_cache_seq_difference,
        global_config.remote_rtx_ssrc);
    remote_rtx_config->AddRtxAndAssociatedPayloadType(1, 0);
    std::unique_ptr<qosrtp::MediaSessionConfig> media_session_config =
        qosrtp::MediaSessionConfig::Create();
    media_session_config->Configure(
        global_config.src_ssrc, nullptr, nullptr, nullptr,
        global_config.dst_ssrc, remote_rtx_config.get(),
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
  std::unique_ptr<qosrtp::FecDecoder> ConstructFecDecoder() {
    std::unique_ptr<qosrtp::FecDecoderConfig> config =
        qosrtp::FecDecoderConfig::Create();
    config->Configure(global_config.fec_max_cache_seq_difference,
                      global_config.dst_ssrc,
                      global_config.remote_fec_payload_type);
    std::unique_ptr<qosrtp::FecDecoder> fec_decoder =
        qosrtp::FecDecoder::Create(qosrtp::FecType::kUlp);
    if (!fec_decoder) {
      return nullptr;
    }
    std::unique_ptr<qosrtp::Result> result =
        fec_decoder->Configure(std::move(config));
    if (!result->ok()) {
      std::cout << "Failed to configure fec decoder, because: "
                << result->description() << std::endl;
      return nullptr;
    }
    return fec_decoder;
  }
  std::unique_ptr<qosrtp::FecDecoder> fec_decoder_;
  std::unique_ptr<qosrtp::QosrtpSession> qosrtp_session_;
};

int main(int argc, char* argv[]) {
  qosrtp::QosrtpInterface::Initialize(nullptr,
                                      qosrtp::QosrtpLogger::Level::kTrace);
  ReceiverWithFec* test_receiver_with_fec = new ReceiverWithFec();
  std::cout << "Any key will exit!" << std::endl;
  std::cin.get();
  delete test_receiver_with_fec;
  qosrtp::QosrtpInterface::UnInitialize();
  return 0;
}