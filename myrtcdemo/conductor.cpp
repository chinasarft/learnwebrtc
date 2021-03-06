/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "conductor.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/audio_options.h"
#include "api/create_peerconnection_factory.h"
#include "api/rtp_sender_interface.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "api/video_codecs/video_decoder_factory.h"
#include "api/video_codecs/video_encoder_factory.h"
#include "defaults.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"
#include "p2p/base/port_allocator.h"
#include "pc/video_track_source.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/ref_counted_object.h"
#include "rtc_base/rtc_certificate_generator.h"
#include "fixjson.h"
#include "test/vcm_capturer.h"
#ifndef USE_WIN32
#include "socket_notifier.h"
#endif

// not invode peerconnection.addtrack
// check this is involved with sdp generation. no media info in sdp id not invoke?
#define DO_ADD_AUDIO_TRACK 1
#define DO_ADD_VIDEO_TRACK 1

namespace {
    // Names used for a IceCandidate JSON object.
    const char kCandidateSdpMidName[] = "sdpMid";
    const char kCandidateSdpMlineIndexName[] = "sdpMLineIndex";
    const char kCandidateSdpName[] = "candidate";
    
    // Names used for a SessionDescription JSON object.
    const char kSessionDescriptionTypeName[] = "type";
    const char kSessionDescriptionSdpName[] = "sdp";
    
    class DummySetSessionDescriptionObserver
    : public webrtc::SetSessionDescriptionObserver {
    public:
        static DummySetSessionDescriptionObserver* Create() {
            return new rtc::RefCountedObject<DummySetSessionDescriptionObserver>();
        }
        virtual void OnSuccess() { RTC_LOG(INFO) << __FUNCTION__; }
        virtual void OnFailure(webrtc::RTCError error) {
            RTC_LOG(INFO) << __FUNCTION__ << " " << ToString(error.type()) << ": "
            << error.message();
        }
    };
    
    class CapturerTrackSource : public webrtc::VideoTrackSource {
    public:
        static rtc::scoped_refptr<CapturerTrackSource> Create() {
            const size_t kWidth = 640;
            const size_t kHeight = 480;
            const size_t kFps = 30;
            std::unique_ptr<webrtc::test::VcmCapturer> capturer;
            std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
                                                                         webrtc::VideoCaptureFactory::CreateDeviceInfo());
            if (!info) {
                return nullptr;
            }
            int num_devices = info->NumberOfDevices();
            for (int i = 0; i < num_devices; ++i) {
                capturer = absl::WrapUnique(
                                            webrtc::test::VcmCapturer::Create(kWidth, kHeight, kFps, i));
                if (capturer) {
                    return new
                    rtc::RefCountedObject<CapturerTrackSource>(std::move(capturer));
                }
            }
            
            return nullptr;
        }
        
    protected:
        explicit CapturerTrackSource(
                                     std::unique_ptr<webrtc::test::VcmCapturer> capturer)
        : VideoTrackSource(/*remote=*/false), capturer_(std::move(capturer)) {}
        
    private:
        rtc::VideoSourceInterface<webrtc::VideoFrame>* source() override {
            return capturer_.get();
        }
        std::unique_ptr<webrtc::test::VcmCapturer> capturer_;
    };
    
}  // namespace

Conductor::Conductor(PeerConnectionClient* client, MainWindow* main_wnd)
: peer_id_(-1), loopback_(false), client_(client), main_wnd_(main_wnd) {
    client_->RegisterObserver(this);
    main_wnd->RegisterObserver(this);
    
    
    if (!InitializePeerConnection()) {
        RTC_LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
    }
    else {
        isCreatedPc_ = true;
    }
}

Conductor::~Conductor() {
    RTC_DCHECK(!peer_connection_);
}

bool Conductor::connection_active() const {
    return peer_connection_ != nullptr;
}

void Conductor::Close() {
    client_->SignOut();
    DeletePeerConnection();
}

bool Conductor::InitializePeerConnection() {
    RTC_DCHECK(!peer_connection_factory_);
    RTC_DCHECK(!peer_connection_);
#ifndef USE_WIN32
    SignalHandler::GetSignalHandler()->GetThreadPtr()->Invoke<int>(
                                                                   RTC_FROM_HERE,
                                                                   [this](){
                                                                       peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
                                                                                                                                      nullptr /* network_thread */, nullptr /* worker_thread */,
                                                                                                                                      SignalHandler::GetSignalHandler()->GetThreadPtr() /* signaling_thread */, nullptr /* default_adm */,
                                                                                                                                      webrtc::CreateBuiltinAudioEncoderFactory(),
                                                                                                                                      webrtc::CreateBuiltinAudioDecoderFactory(),
                                                                                                                                      webrtc::CreateBuiltinVideoEncoderFactory(),
                                                                                                                                      webrtc::CreateBuiltinVideoDecoderFactory(), nullptr /* audio_mixer */,
                                                                                                                                      nullptr /* audio_processing */);
                                                                       return 0;
                                                                   }
                                                                   );
#else
    peer_connection_factory_ = webrtc::CreatePeerConnectionFactory(
                                                                   nullptr /* network_thread */, nullptr /* worker_thread */,
                                                                   SocketNotifier::GetSocketNotifier()->GetThreadPtr() /* signaling_thread */, nullptr /* default_adm */,
                                                                   webrtc::CreateBuiltinAudioEncoderFactory(),
                                                                   webrtc::CreateBuiltinAudioDecoderFactory(),
                                                                   webrtc::CreateBuiltinVideoEncoderFactory(),
                                                                   webrtc::CreateBuiltinVideoDecoderFactory(), nullptr /* audio_mixer */,
                                                                   nullptr /* audio_processing */);
#endif
    if (!peer_connection_factory_) {
        main_wnd_->MessageBox("Error", "Failed to initialize PeerConnectionFactory",
                              true);
        DeletePeerConnection();
        return false;
    }
    
    if (!CreatePeerConnection(/*dtls=*/true)) {
        main_wnd_->MessageBox("Error", "CreatePeerConnection failed", true);
        DeletePeerConnection();
    }
    
    AddTracks();
    
    return peer_connection_ != nullptr;
}

bool Conductor::ReinitializePeerConnectionForLoopback() {
    loopback_ = true;
    std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> senders =
    peer_connection_->GetSenders();
    peer_connection_ = nullptr;
    if (CreatePeerConnection(/*dtls=*/false)) {
        for (const auto& sender : senders) {
            peer_connection_->AddTrack(sender->track(), sender->stream_ids());
        }
        peer_connection_->CreateOffer(
                                      this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    }
    return peer_connection_ != nullptr;
}

bool Conductor::CreatePeerConnection(bool dtls) {
    RTC_DCHECK(peer_connection_factory_);
    RTC_DCHECK(!peer_connection_);
    
    webrtc::PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
    config.enable_dtls_srtp = dtls;
    webrtc::PeerConnectionInterface::IceServer server;
    server.uri = GetPeerConnectionString();
    config.servers.push_back(server);
    
    peer_connection_ = peer_connection_factory_->CreatePeerConnection(
                                                                      config, nullptr, nullptr, this);
    return peer_connection_ != nullptr;
}

void Conductor::DeletePeerConnection() {
    main_wnd_->StopLocalRenderer();
    main_wnd_->StopRemoteRenderer();
    peer_connection_ = nullptr;
    peer_connection_factory_ = nullptr;
    peer_id_ = -1;
    loopback_ = false;
}

void Conductor::EnsureStreamingUI() {
    RTC_DCHECK(peer_connection_);
    if (main_wnd_->current_ui() != MainWindow::STREAMING)
        main_wnd_->SwitchToStreamingUI();
}

//
// PeerConnectionObserver implementation.
//

void Conductor::OnAddTrack(
                           rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
                           const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) {
    //receiver->id() is not OnTrack::transceiver->mid()
    //receiver->track().get() is equal to OnTrack::transceiver->receiver()->track().get()
    RTC_LOG(INFO) << __FUNCTION__ << " " << receiver->id()<<"|"<<receiver->track().get()<<"|streamptr:"<<streams.size();
    for (int i = 0; i < streams.size(); i++) {
        RTC_LOG(INFO) << __FUNCTION__ << " " <<streams[i].get();
    }
    //main_wnd_->QueueUIThreadCallback(NEW_TRACK_ADDED, receiver->track().release());
}

void Conductor::OnTrack(
                        rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) {
    
    RTC_LOG(INFO) << __FUNCTION__ << " " << transceiver->mid().value()<<"|trackptr:"<<transceiver->receiver()->track().get();
    main_wnd_->QueueUIThreadCallback(NEW_TRACK_ADDED,
                                     transceiver->receiver()->track().release());
}

void Conductor::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) {
    RTC_LOG(INFO) << __FUNCTION__ << " " << stream.get();
    auto atracks = stream->GetAudioTracks();
    for (int i = 0; i < atracks.size(); i++) {
        RTC_LOG(INFO) << __FUNCTION__ << " atrackptr:"<<atracks.at(i).get();
    }
    auto vtracks = stream->GetVideoTracks();
    for (int i = 0; i < vtracks.size(); i++) {
        RTC_LOG(INFO) << __FUNCTION__ << " vtrackptr:"<<vtracks.at(i).get();
    }
    // can do this, not need to overrive OnTrack or OnAddTrack
    // OnAddStream is callback after OnTrack
    //main_wnd_->QueueUIThreadCallback(NEW_TRACK_ADDED, vtracks.at(i));
}

void Conductor::OnRemoveTrack(
                              rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) {
    RTC_LOG(INFO) << __FUNCTION__ << " " << receiver->id();
    main_wnd_->QueueUIThreadCallback(TRACK_REMOVED, receiver->track().release());
}

void Conductor::OnIceCandidate(const webrtc::IceCandidateInterface* candidate) {
    std::string candidateStr;
    candidate->ToString(&candidateStr);
    //RTC_LOG(INFO) <<"mylog:"<< __FUNCTION__ << " " << candidate->sdp_mline_index()<<candidateStr;
    // For loopback test. To save some connecting delay.
    if (loopback_) {
        if (!peer_connection_->AddIceCandidate(candidate)) {
            RTC_LOG(WARNING) << "Failed to apply the received candidate";
        }
        return;
    }
    
    char jsonstr[10240];
    memset(jsonstr, 0, sizeof(jsonstr));
    
    std::string sdp;
    if (!candidate->ToString(&sdp)) {
        RTC_LOG(LS_ERROR) << "Failed to serialize candidate";
        return;
    }
    
    std::string::size_type pos = 0;
    std::string srcStr = "\r\n";
    std::string dstStr = "\\r\\n";
    while ((pos = sdp.find(srcStr, pos)) != std::string::npos) {
        sdp.replace(pos, srcStr.length(), dstStr);
        pos += dstStr.length();
    }
    
    snprintf(jsonstr, sizeof(jsonstr), "{\"%s\":\"%s\", \"%s\":%d, \"%s\":\"%s\"}",kCandidateSdpMidName, candidate->sdp_mid().c_str(),
             kCandidateSdpMlineIndexName, candidate->sdp_mline_index(),
             kCandidateSdpName, sdp.c_str());
    SendMessage(jsonstr);
}

//
// PeerConnectionClientObserver implementation.
//

void Conductor::OnSignedIn() {
    RTC_LOG(INFO) << __FUNCTION__;
    main_wnd_->SwitchToPeerList(client_->peers());
}

void Conductor::OnDisconnected() {
    RTC_LOG(INFO) << __FUNCTION__;
    
    DeletePeerConnection();
    
}

void Conductor::OnPeerConnected(int id, const std::string& name) {
    RTC_LOG(INFO) << __FUNCTION__;
    // Refresh the list if we're showing it.
    if (main_wnd_->current_ui() == MainWindow::LIST_PEERS)
        main_wnd_->SwitchToPeerList(client_->peers());
}

void Conductor::OnPeerDisconnected(int id) {
    RTC_LOG(INFO) << __FUNCTION__;
    if (id == peer_id_) {
        RTC_LOG(INFO) << "Our peer disconnected";
        main_wnd_->QueueUIThreadCallback(PEER_CONNECTION_CLOSED, NULL);
    } else {
        // Refresh the list if we're showing it.
        if (main_wnd_->current_ui() == MainWindow::LIST_PEERS)
            main_wnd_->SwitchToPeerList(client_->peers());
    }
}

void Conductor::OnMessageFromPeer(int peer_id, const std::string& message) {
    RTC_DCHECK(peer_id_ == peer_id || peer_id_ == -1);
    RTC_DCHECK(!message.empty());
    
    RTC_LOG(WARNING) << "mylog:OnMessageFromPeer:" << message;
    if (peer_id_ == -1) {
        peer_id_ = peer_id;
    } else if (peer_id != peer_id_) {
        RTC_DCHECK(peer_id_ != -1);
        RTC_LOG(WARNING)
        << "Received a message from unknown peer while already in a "
        "conversation with a different peer.";
        return;
    }
    
    std::string json_object;
    
    char jsonvalue[10240];
    memset(jsonvalue, 0, sizeof(jsonvalue));
    int vlen = sizeof(jsonvalue);
    int ret = LinkGetJsonStringByKey(message.c_str(), kSessionDescriptionTypeName, jsonvalue, &vlen);
    std::string type_str;
    if (ret == 0 )
        type_str = jsonvalue;
    memset(jsonvalue, 0, sizeof(jsonvalue));
    vlen = sizeof(jsonvalue);
    
    if (!type_str.empty()) {
        if (type_str == "offer-loopback") {
            // This is a loopback call.
            // Recreate the peerconnection with DTLS disabled.
            if (!ReinitializePeerConnectionForLoopback()) {
                RTC_LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
                DeletePeerConnection();
                client_->SignOut();
            }
            return;
        }
        absl::optional<webrtc::SdpType> type_maybe =
        webrtc::SdpTypeFromString(type_str);
        if (!type_maybe) {
            RTC_LOG(LS_ERROR) << "Unknown SDP type: " << type_str;
            return;
        }
        webrtc::SdpType type = *type_maybe;
        std::string sdp;
        ret = LinkGetJsonStringByKey(message.c_str(), kSessionDescriptionSdpName, jsonvalue, &vlen);
        if (ret == 0) {
            sdp = jsonvalue;
            std::string::size_type pos = 0;
            std::string srcStr = "\\r\\n";
            std::string dstStr = "\r\n";
            while ((pos = sdp.find(srcStr, pos)) != std::string::npos) {
                sdp.replace(pos, srcStr.length(), dstStr);
                pos += dstStr.length();
            }
        } else
            RTC_LOG(WARNING) << "get json fail";
        memset(jsonvalue, 0, sizeof(jsonvalue));
        vlen = sizeof(jsonvalue);
        if (sdp.empty()) {
            RTC_LOG(WARNING) << "Can't parse received session description message.";
            return;
        }
        webrtc::SdpParseError error;
        std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
        webrtc::CreateSessionDescription(type, sdp, &error);
        if (!session_description) {
            RTC_LOG(WARNING) << "Can't parse received session description message. "
            << "SdpParseError was: " << error.description;
            return;
        }
        RTC_LOG(INFO) << " Received session description :" << message;
        peer_connection_->SetRemoteDescription(
                                               DummySetSessionDescriptionObserver::Create(),
                                               session_description.release());
        if (type == webrtc::SdpType::kOffer) {
            peer_connection_->CreateAnswer(
                                           this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
        }
    } else {
        std::string sdp_mid;
        int sdp_mlineindex = 0;
        std::string sdp;
        
        ret = LinkGetJsonStringByKey(message.c_str(), kCandidateSdpMidName, jsonvalue, &vlen);
        if (ret == 0)
            sdp_mid = jsonvalue;
        else
            RTC_LOG(WARNING) << "get json fail";
        memset(jsonvalue, 0, sizeof(jsonvalue));
        vlen = sizeof(jsonvalue);
        
        
        sdp_mlineindex = LinkGetJsonIntByKey(message.c_str(), kCandidateSdpMlineIndexName);
        
        ret = LinkGetJsonStringByKey(message.c_str(), kCandidateSdpName, jsonvalue, &vlen);
        if (ret == 0)
            sdp = jsonvalue;
        memset(jsonvalue, 0, sizeof(jsonvalue));
        vlen = sizeof(jsonvalue);
        
        if (sdp_mlineindex < 0 || sdp.empty() || sdp_mid.empty()) {
            RTC_LOG(WARNING) << "Can't parse received message.";
            return;
        }
        webrtc::SdpParseError error;
        std::unique_ptr<webrtc::IceCandidateInterface> candidate(
                                                                 webrtc::CreateIceCandidate(sdp_mid, sdp_mlineindex, sdp, &error));
        if (!candidate.get()) {
            RTC_LOG(WARNING) << "Can't parse received candidate message. "
            << "SdpParseError was: " << error.description;
            return;
        }
        if (!peer_connection_->AddIceCandidate(candidate.get())) {
            RTC_LOG(WARNING) << "Failed to apply the received candidate";
            return;
        }
        RTC_LOG(INFO) << " Received candidate :" << message;
    }
}

void Conductor::OnMessageSent(int err) {
    // Process the next pending message if any.
    main_wnd_->QueueUIThreadCallback(SEND_MESSAGE_TO_PEER, NULL);
}

void Conductor::OnServerConnectionFailure() {
    main_wnd_->MessageBox("Error", ("Failed to connect to " + server_).c_str(),
                          true);
}

//
// MainWndCallback implementation.
//

void Conductor::StartLogin(const std::string& server, int port) {
    if (client_->is_connected())
        return;
    server_ = server;
    client_->Connect(server, port, GetPeerName());
}

void Conductor::DisconnectFromServer() {
    if (client_->is_connected())
        client_->SignOut();
}

void Conductor::ConnectToPeer(int peer_id) {
    RTC_DCHECK(peer_id_ == -1);
    RTC_DCHECK(peer_id != -1);
    
    if (isCreatedPc_) {
        peer_id_ = peer_id;
        peer_connection_->CreateOffer(
                                      this, webrtc::PeerConnectionInterface::RTCOfferAnswerOptions());
    } else {
        main_wnd_->MessageBox("Error", "Failed to initialize PeerConnection", true);
    }
}

void Conductor::AddTracks() {
    if (!peer_connection_->GetSenders().empty()) {
        return;  // Already added tracks.
    }
    
    localMediaStream_ = peer_connection_factory_->CreateLocalMediaStream("ARDAMS");
    
    AddAudioTrack();
    AddVideoTrack();
    
    main_wnd_->SwitchToStreamingUI();
}

void Conductor::AddAudioTrack() {
    audio_track_ = peer_connection_factory_->CreateAudioTrack(
                                                              kAudioLabel, peer_connection_factory_->CreateAudioSource(cricket::AudioOptions()));
#ifdef DO_ADD_AUDIO_TRACK
    auto result_or_error = peer_connection_->AddTrack(audio_track_, {kStreamId});
    if (!result_or_error.ok()) {
        RTC_LOG(LS_ERROR) << "Failed to add audio track to PeerConnection: "
        << result_or_error.error().message();
    } else {
        audio_rtp_sender_ = result_or_error.MoveValue();
#endif
        if (!localMediaStream_->AddTrack(audio_track_)) {
            RTC_LOG(LS_ERROR) << "Failed to add audio track to PeerConnection: ";
        }
#ifdef DO_ADD_AUDIO_TRACK
    }
#endif
}

void Conductor::AddVideoTrack() {
    
    static rtc::scoped_refptr<CapturerTrackSource> video_device = CapturerTrackSource::Create();;
    if (video_device) {
        video_track_ = peer_connection_factory_->CreateVideoTrack(kVideoLabel, video_device);
        main_wnd_->StartLocalRenderer(video_track_);
#ifdef DO_ADD_VIDEO_TRACK
        auto result_or_error = peer_connection_->AddTrack(video_track_, { kStreamId });
        if (!result_or_error.ok()) {
            RTC_LOG(LS_ERROR) << "Failed to add video track to PeerConnection: "
            << result_or_error.error().message();
        }
        else {
            video_rtp_sender_ = result_or_error.MoveValue();
#endif
            if (!localMediaStream_->AddTrack(video_track_)) {
                RTC_LOG(LS_ERROR) << "Failed to add video track to PeerConnection: ";
            }
#ifdef DO_ADD_VIDEO_TRACK
        }
#endif
    } else {
        RTC_LOG(LS_ERROR) << "OpenVideoCaptureDevice failed";
    }
}

void Conductor::DisconnectFromCurrentPeer() {
    RTC_LOG(INFO) << __FUNCTION__;
    if (peer_connection_.get()) {
        client_->SendHangUp(peer_id_);
        DeletePeerConnection();
    }
    
    main_wnd_->SwitchToPeerList(client_->peers());
}

void Conductor::UIThreadCallback(int msg_id, void* data) {
    switch (msg_id) {
        case PEER_CONNECTION_CLOSED:
            RTC_LOG(INFO) << "PEER_CONNECTION_CLOSED";
            DeletePeerConnection();
            
            if (client_->is_connected()) {
                main_wnd_->SwitchToPeerList(client_->peers());
            } else {
                //main_wnd_->SwitchToConnectUI();
            }
            break;
            
        case SEND_MESSAGE_TO_PEER: {
            std::string* msg = reinterpret_cast<std::string*>(data);
            if (msg) {
                RTC_LOG(INFO) << "mylog:SEND_MESSAGE_TO_PEER"<<*(reinterpret_cast<std::string*>(data));
                // For convenience, we always run the message through the queue.
                // This way we can be sure that messages are sent to the server
                // in the same order they were signaled without much hassle.
                pending_messages_.push_back(msg);
            }
            else
                RTC_LOG(INFO) << "mylog:SEND_MESSAGE_TO_PEER";
            
            if (!pending_messages_.empty() && !client_->IsSendingMessage()) {
                msg = pending_messages_.front();
                pending_messages_.pop_front();
                
                if (!client_->SendToPeer(peer_id_, *msg) && peer_id_ != -1) {
                    RTC_LOG(LS_ERROR) << "SendToPeer failed";
                    DisconnectFromServer();
                }
                delete msg;
            }
            
            if (!peer_connection_.get())
                peer_id_ = -1;
            
            break;
        }
            
        case NEW_TRACK_ADDED: {
            auto* track = reinterpret_cast<webrtc::MediaStreamTrackInterface*>(data);
            if (track->kind() == webrtc::MediaStreamTrackInterface::kVideoKind) {
                auto* video_track = static_cast<webrtc::VideoTrackInterface*>(track);
                main_wnd_->StartRemoteRenderer(video_track);
            }
            track->Release();
            break;
        }
            
        case TRACK_REMOVED: {
            // Remote peer stopped sending a track.
            auto* track = reinterpret_cast<webrtc::MediaStreamTrackInterface*>(data);
            track->Release();
            break;
        }
            
        default:
            RTC_NOTREACHED();
            break;
    }
}

void Conductor::OnSuccess(webrtc::SessionDescriptionInterface* desc) {
    peer_connection_->SetLocalDescription(
                                          DummySetSessionDescriptionObserver::Create(), desc);
    
    std::string sdp;
    desc->ToString(&sdp);
    
    //RTC_LOG(LERROR) << "mylog:createsdp:" <<sdp;
    // For loopback test. To save some connecting delay.
    if (loopback_) {
        // Replace message type from "offer" to "answer"
        std::unique_ptr<webrtc::SessionDescriptionInterface> session_description =
        webrtc::CreateSessionDescription(webrtc::SdpType::kAnswer, sdp);
        peer_connection_->SetRemoteDescription(
                                               DummySetSessionDescriptionObserver::Create(),
                                               session_description.release());
        return;
    }
    
    char jsonstr[10240];
    memset(jsonstr, 0, sizeof(jsonstr));
    
    std::string::size_type pos = 0;
    std::string srcStr = "\r\n";
    std::string dstStr = "\\r\\n";
    while ((pos = sdp.find(srcStr, pos)) != std::string::npos) {
        sdp.replace(pos, srcStr.length(), dstStr);
        pos += dstStr.length();
    }
    
    snprintf(jsonstr, sizeof(jsonstr), "{\"%s\":\"%s\", \"%s\":\"%s\"}",
             kSessionDescriptionTypeName, webrtc::SdpTypeToString(desc->GetType()),
             kSessionDescriptionSdpName, sdp.c_str());
    
    SendMessage(jsonstr);
}

void Conductor::OnFailure(webrtc::RTCError error) {
    RTC_LOG(LERROR) << "createoffer" << ToString(error.type()) << ": " << error.message();
}

void Conductor::OnFailure(const std::string& error) {
    RTC_LOG(LERROR) << "createoffer" << ": " << error;
}

void Conductor::SendMessage(const std::string& json_object) {
    std::string* msg = new std::string(json_object);
    main_wnd_->QueueUIThreadCallback(SEND_MESSAGE_TO_PEER, msg);
}


bool Conductor::RemoveLocalAudioTrack() {
    if (localMediaStream_ && audio_track_) {
        if(localMediaStream_->RemoveTrack(audio_track_)){
#ifdef DO_ADD_AUDIO_TRACK
            peer_connection_->RemoveTrack(audio_rtp_sender_); // remove from sdp
            audio_rtp_sender_ = nullptr;
#endif
            audio_track_ = nullptr;
            return true;
        } else {
            return false;
        }
    }
    return false;
}

bool Conductor::RemoveLocalVideoTrack() {
    if (localMediaStream_ && video_track_) {
        video_track_->set_enabled(false); // most important
        if( localMediaStream_->RemoveTrack(video_track_)){
#ifdef DO_ADD_VIDEO_TRACK
            peer_connection_->RemoveTrack(video_rtp_sender_); // remove from sdp
            video_rtp_sender_ = nullptr;
#endif
            video_track_ = nullptr;
            return true;
        } else {
            return false;
        }
    }
    return false;
}

void Conductor::AddLocalAudioTrack() {
    AddAudioTrack();
}

void Conductor::AddLocalVideoTrack() {
    AddVideoTrack();
}
