#include <map>
#include <memory>
#include <set>
#include <cassert>

#include "api/video_codecs/video_codec.h"
#include "modules/rtp_rtcp/include/rtp_header_parser.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtcp_packet/nack.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_rtcp_impl.h"
#include "rtc_base/rate_limiter.h"
#include "test/rtcp_packet_parser.h"
#include "test/rtcp_packet_parser.cc"

using namespace webrtc;

const uint32_t kSenderSsrc = 0x12345;
const uint32_t kReceiverSsrc = 0x23456;
const int64_t kOneWayNetworkDelayMs = 100;
const uint8_t kBaseLayerTid = 0;
const uint8_t kHigherLayerTid = 1;
const uint16_t kSequenceNumber = 100;

class RtcpRttStatsTestImpl : public RtcpRttStats {
public:
        RtcpRttStatsTestImpl() : rtt_ms_(0) {}
        ~RtcpRttStatsTestImpl() override = default;
        
        void OnRttUpdate(int64_t rtt_ms) override { rtt_ms_ = rtt_ms; }
        int64_t LastProcessedRtt() const override { return rtt_ms_; }
        int64_t rtt_ms_;
};

class SendTransport : public Transport, public RtpData {
public:
        SendTransport()
        : receiver_(nullptr),
        clock_(nullptr),
        delay_ms_(0),
        rtp_packets_sent_(0),
        rtcp_packets_sent_(0),
        keepalive_payload_type_(0),
        num_keepalive_sent_(0) {}
        
        void SetRtpRtcpModule(ModuleRtpRtcpImpl* receiver) { receiver_ = receiver; }
        void SimulateNetworkDelay(int64_t delay_ms, SimulatedClock* clock) {
                clock_ = clock;
                delay_ms_ = delay_ms;
        }
        bool SendRtp(const uint8_t* data,
                     size_t len,
                     const PacketOptions& options) override {
                RTPHeader header;
                std::unique_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
                assert(parser->Parse(static_cast<const uint8_t*>(data), len, &header) == true);
                ++rtp_packets_sent_;
                if (header.payloadType == keepalive_payload_type_)
                        ++num_keepalive_sent_;
                last_rtp_header_ = header;
                return true;
        }
        bool SendRtcp(const uint8_t* data, size_t len) override {
                test::RtcpPacketParser parser;
                parser.Parse(data, len);
                last_nack_list_ = parser.nack()->packet_ids();
                
                if (clock_) {
                        clock_->AdvanceTimeMilliseconds(delay_ms_);
                }
                assert(receiver_ != nullptr);
                receiver_->IncomingRtcpPacket(data, len);
                ++rtcp_packets_sent_;
                return true;
        }
        int32_t OnReceivedPayloadData(const uint8_t* payload_data,
                                      size_t payload_size,
                                      const WebRtcRTPHeader* rtp_header) override {
                return 0;
        }
        void SetKeepalivePayloadType(uint8_t payload_type) {
                keepalive_payload_type_ = payload_type;
        }
        size_t NumKeepaliveSent() { return num_keepalive_sent_; }
        size_t NumRtcpSent() { return rtcp_packets_sent_; }
        ModuleRtpRtcpImpl* receiver_;
        SimulatedClock* clock_;
        int64_t delay_ms_;
        int rtp_packets_sent_;
        size_t rtcp_packets_sent_;
        RTPHeader last_rtp_header_;
        std::vector<uint16_t> last_nack_list_;
        uint8_t keepalive_payload_type_;
        size_t num_keepalive_sent_;
};

class RtpRtcpModule : public RtcpPacketTypeCounterObserver {
public:
        explicit RtpRtcpModule(SimulatedClock* clock)
        : receive_statistics_(ReceiveStatistics::Create(clock)),
        remote_ssrc_(0),
        clock_(clock) {
                CreateModuleImpl();
                transport_.SimulateNetworkDelay(kOneWayNetworkDelayMs, clock);
        }
        
        RtcpPacketTypeCounter packets_sent_;
        RtcpPacketTypeCounter packets_received_;
        std::unique_ptr<ReceiveStatistics> receive_statistics_;
        SendTransport transport_;
        RtcpRttStatsTestImpl rtt_stats_;
        std::unique_ptr<ModuleRtpRtcpImpl> impl_;
        uint32_t remote_ssrc_;
        RtpKeepAliveConfig keepalive_config_;
        RtcpIntervalConfig rtcp_interval_config_;
        
        void SetRemoteSsrc(uint32_t ssrc) {
                remote_ssrc_ = ssrc;
                impl_->SetRemoteSSRC(ssrc);
        }
        
        void RtcpPacketTypesCounterUpdated(
                                           uint32_t ssrc,
                                           const RtcpPacketTypeCounter& packet_counter) override {
                counter_map_[ssrc] = packet_counter;
        }
        
        RtcpPacketTypeCounter RtcpSent() {
                // RTCP counters for remote SSRC.
                return counter_map_[remote_ssrc_];
        }
        
        RtcpPacketTypeCounter RtcpReceived() {
                // Received RTCP stats for (own) local SSRC.
                return counter_map_[impl_->SSRC()];
        }
        int RtpSent() { return transport_.rtp_packets_sent_; }
        uint16_t LastRtpSequenceNumber() {
                return transport_.last_rtp_header_.sequenceNumber;
        }
        std::vector<uint16_t> LastNackListSent() {
                return transport_.last_nack_list_;
        }
        void SetKeepaliveConfigAndReset(const RtpKeepAliveConfig& config) {
                keepalive_config_ = config;
                // Need to create a new module impl, since it's configured at creation.
                CreateModuleImpl();
                transport_.SetKeepalivePayloadType(config.payload_type);
        }
        void SetRtcpIntervalConfigAndReset(const RtcpIntervalConfig& config) {
                rtcp_interval_config_ = config;
                CreateModuleImpl();
        }
        
private:
        void CreateModuleImpl() {
                RtpRtcp::Configuration config;
                config.audio = false;
                config.clock = clock_;
                config.outgoing_transport = &transport_;
                config.receive_statistics = receive_statistics_.get();
                config.rtcp_packet_type_counter_observer = this;
                config.rtt_stats = &rtt_stats_;
                config.keepalive_config = keepalive_config_;
                config.rtcp_interval_config = rtcp_interval_config_;
                
                impl_.reset(new ModuleRtpRtcpImpl(config));
                impl_->SetRTCPStatus(RtcpMode::kCompound);
        }
        
        SimulatedClock* const clock_;
        std::map<uint32_t, RtcpPacketTypeCounter> counter_map_;
};

class RtpRtcpImplTest {
protected:
        RtpRtcpImplTest()
        : clock_(133590000000000), sender_(&clock_), receiver_(&clock_) {}
        
        void SetUp() {
                // Send module.
                sender_.impl_->SetSSRC(kSenderSsrc);
                assert(0 == sender_.impl_->SetSendingStatus(true));
                sender_.impl_->SetSendingMediaStatus(true);
                sender_.SetRemoteSsrc(kReceiverSsrc);
                sender_.impl_->SetSequenceNumber(kSequenceNumber);
                sender_.impl_->SetStorePacketsStatus(true, 100);
                
                memset(&codec_, 0, sizeof(VideoCodec));
                codec_.plType = 100;
                codec_.width = 320;
                codec_.height = 180;
                sender_.impl_->RegisterVideoSendPayload(codec_.plType, "VP8");
                
                // Receive module.
                assert(0 == receiver_.impl_->SetSendingStatus(false));
                receiver_.impl_->SetSendingMediaStatus(false);
                receiver_.impl_->SetSSRC(kReceiverSsrc);
                receiver_.SetRemoteSsrc(kSenderSsrc);
                // Transport settings.
                sender_.transport_.SetRtpRtcpModule(receiver_.impl_.get());
                receiver_.transport_.SetRtpRtcpModule(sender_.impl_.get());
        }
        
        SimulatedClock clock_;
        RtpRtcpModule sender_;
        RtpRtcpModule receiver_;
        VideoCodec codec_;
        
        void SendFrame(const RtpRtcpModule* module, uint8_t tid) {
                RTPVideoHeaderVP8 vp8_header = {};
                vp8_header.temporalIdx = tid;
                RTPVideoHeader rtp_video_header;
                rtp_video_header.width = codec_.width;
                rtp_video_header.height = codec_.height;
                rtp_video_header.rotation = kVideoRotation_0;
                rtp_video_header.content_type = VideoContentType::UNSPECIFIED;
                rtp_video_header.playout_delay = {-1, -1};
                rtp_video_header.is_first_packet_in_frame = true;
                rtp_video_header.simulcastIdx = 0;
                rtp_video_header.codec = kVideoCodecVP8;
                rtp_video_header.video_type_header = vp8_header;
                rtp_video_header.video_timing = {0u, 0u, 0u, 0u, 0u, 0u, false};
                
                const uint8_t payload[100] = {0};
                assert(true == module->impl_->SendOutgoingData(
                                                                kVideoFrameKey, codec_.plType, 0, 0, payload,
                                                                sizeof(payload), nullptr, &rtp_video_header, nullptr));
        }
        
        void IncomingRtcpNack(const RtpRtcpModule* module, uint16_t sequence_number) {
                bool sender = module->impl_->SSRC() == kSenderSsrc;
                rtcp::Nack nack;
                uint16_t list[1];
                list[0] = sequence_number;
                const uint16_t kListLength = sizeof(list) / sizeof(list[0]);
                nack.SetSenderSsrc(sender ? kReceiverSsrc : kSenderSsrc);
                nack.SetMediaSsrc(sender ? kSenderSsrc : kReceiverSsrc);
                nack.SetPacketIds(list, kListLength);
                rtc::Buffer packet = nack.Build();
                module->impl_->IncomingRtcpPacket(packet.data(), packet.size());
        }
};

class RtpRtcpImplTestMain : public RtpRtcpImplTest {
public:
        void testBody() {
                SetUp();
                sender_.impl_->SetSelectiveRetransmissions(kRetransmitBaseLayer);
                assert(kRetransmitBaseLayer == sender_.impl_->SelectiveRetransmissions());
                
                // Send frames.
                assert(0 == sender_.RtpSent());
                SendFrame(&sender_, kBaseLayerTid);    // kSequenceNumber
                SendFrame(&sender_, kHigherLayerTid);  // kSequenceNumber + 1
                SendFrame(&sender_, kNoTemporalIdx);   // kSequenceNumber + 2
                assert(3 == sender_.RtpSent());
                assert(kSequenceNumber + 2 == sender_.LastRtpSequenceNumber());
                
                // Min required delay until retransmit = 5 + RTT ms (RTT = 0).
                clock_.AdvanceTimeMilliseconds(5);
                
                // Frame with kBaseLayerTid re-sent.
                IncomingRtcpNack(&sender_, kSequenceNumber);
                assert(4 == sender_.RtpSent());
                assert(kSequenceNumber == sender_.LastRtpSequenceNumber());
                // Frame with kHigherLayerTid not re-sent.
                IncomingRtcpNack(&sender_, kSequenceNumber + 1);
                assert(4 == sender_.RtpSent());
                // Frame with kNoTemporalIdx re-sent.
                IncomingRtcpNack(&sender_, kSequenceNumber + 2);
                assert(5 == sender_.RtpSent());
                assert(kSequenceNumber + 2 == sender_.LastRtpSequenceNumber());
        }
};

int main() {
        RtpRtcpImplTestMain test;
        test.testBody();
}
