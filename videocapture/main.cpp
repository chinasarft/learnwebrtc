
#include <stdio.h>

#include <map>
#include <memory>
#include <sstream>

#include "absl/memory/memory.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "modules/utility/include/process_thread.h"
#include "modules/video_capture/video_capture.h"
#include "modules/video_capture/video_capture_factory.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/sleep.h"
#include <assert.h>

using webrtc::SleepMs;
using webrtc::VideoCaptureCapability;
using webrtc::VideoCaptureFactory;
using webrtc::VideoCaptureModule;

#define WAIT_(ex, timeout, res)                           \
do {                                                    \
res = (ex);                                           \
int64_t start = rtc::TimeMillis();                    \
while (!res && rtc::TimeMillis() < start + timeout) { \
SleepMs(5);                                         \
res = (ex);                                         \
}                                                     \
} while (0)

#define EXPECT_TRUE_WAIT(ex, timeout) \
do {                                \
bool res;                         \
WAIT_(ex, timeout, res);          \
if (!res)                         \
EXPECT_TRUE(ex);                \
} while (0)

#define EXPECT_TRUE(c) assert(c)
#define EXPECT_EQ(a,b) assert(a == b)
#define EXPECT_FALSE(c) assert(!(c))
#define EXPECT_LE(a,b) assert(a<b)
#define ASSERT_TRUE(c) assert(c)
#define ASSERT_GT(a, b) assert(a>b)
#define ASSERT_LT(a, b) assert(a<b)
#define ASSERT_EQ(a, b) assert(a==b)

static const int kTimeOut = 5000;
static const int kTestHeight = 288;
static const int kTestWidth = 352;
static const int kTestFramerate = 30;

const char *getVideoTypeName(webrtc::VideoType vt) {
    switch(vt) {
        case webrtc::VideoType::kUnknown:
            return "kUnknown";
            break;
        case webrtc::VideoType::kI420:
            return "kI420";
            break;
        case webrtc::VideoType::kIYUV:
            return "kIYUV";
            break;
        case webrtc::VideoType::kRGB24:
            return "kRGB24";
            break;
        case webrtc::VideoType::kABGR:
            return "kABGR";
            break;
        case webrtc::VideoType::kARGB:
            return "kARGB";
            break;
        case webrtc::VideoType::kARGB4444:
            return "kARGB4444";
            break;
        case webrtc::VideoType::kRGB565:
            return "kRGB565";
            break;
        case webrtc::VideoType::kARGB1555:
            return "kARGB1555";
            break;
        case webrtc::VideoType::kYUY2:
            return "kYUY2";
            break;
        case webrtc::VideoType::kYV12:
            return "kYV12";
            break;
        case webrtc::VideoType::kUYVY:
            return "kUYVY";
            break;
        case webrtc::VideoType::kMJPEG:
            return "kMJPEG";
            break;
        case webrtc::VideoType::kNV21:
            return "kNV21";
            break;
        case webrtc::VideoType::kNV12:
            return "kNV12";
            break;
        case webrtc::VideoType::kBGRA:
            return "kBGRA";
            break;
    };
    
}

class TestVideoCaptureCallback
: public rtc::VideoSinkInterface<webrtc::VideoFrame> {
public:
    TestVideoCaptureCallback()
    : last_render_time_ms_(0),
    incoming_frames_(0),
    timing_warnings_(0),
    rotate_frame_(webrtc::kVideoRotation_0) {}
    
    ~TestVideoCaptureCallback() override {
        if (timing_warnings_ > 0)
            printf("No of timing warnings %d\n", timing_warnings_);
    }
    
    void OnFrame(const webrtc::VideoFrame& videoFrame) override {
        fprintf(stderr, "get one frame:%dx%d %u %lld %lld size:%u\n", videoFrame.width(), videoFrame.height(), videoFrame.timestamp(),
                videoFrame.ntp_time_ms(), videoFrame.timestamp_us(), videoFrame.size());
        
        rtc::CritScope cs(&capture_cs_);
        int height = videoFrame.height();
        int width = videoFrame.width();
#if defined(WEBRTC_ANDROID) && WEBRTC_ANDROID
        // Android camera frames may be rotated depending on test device
        // orientation.
        EXPECT_TRUE(height == capability_.height || height == capability_.width);
        EXPECT_TRUE(width == capability_.width || width == capability_.height);
#else
        EXPECT_EQ(height, capability_.height);
        EXPECT_EQ(width, capability_.width);
        EXPECT_EQ(rotate_frame_, videoFrame.rotation());
#endif
        // RenderTimstamp should be the time now.
        EXPECT_TRUE(videoFrame.render_time_ms() >= rtc::TimeMillis() - 30 &&
                    videoFrame.render_time_ms() <= rtc::TimeMillis());
        
        if ((videoFrame.render_time_ms() >
             last_render_time_ms_ + (1000 * 1.1) / capability_.maxFPS &&
             last_render_time_ms_ > 0) ||
            (videoFrame.render_time_ms() <
             last_render_time_ms_ + (1000 * 0.9) / capability_.maxFPS &&
             last_render_time_ms_ > 0)) {
                timing_warnings_++;
            }
        
        incoming_frames_++;
        last_render_time_ms_ = videoFrame.render_time_ms();
        last_frame_ = videoFrame.video_frame_buffer();
    }
    
    void SetExpectedCapability(VideoCaptureCapability capability) {
        rtc::CritScope cs(&capture_cs_);
        capability_ = capability;
        incoming_frames_ = 0;
        last_render_time_ms_ = 0;
    }
    int incoming_frames() {
        rtc::CritScope cs(&capture_cs_);
        return incoming_frames_;
    }
    int timing_warnings() {
        rtc::CritScope cs(&capture_cs_);
        return timing_warnings_;
    }
    VideoCaptureCapability capability() {
        rtc::CritScope cs(&capture_cs_);
        return capability_;
    }
    

    
    void SetExpectedCaptureRotation(webrtc::VideoRotation rotation) {
        rtc::CritScope cs(&capture_cs_);
        rotate_frame_ = rotation;
    }
    
private:
    rtc::CriticalSection capture_cs_;
    VideoCaptureCapability capability_;
    int64_t last_render_time_ms_;
    int incoming_frames_;
    int timing_warnings_;
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> last_frame_;
    webrtc::VideoRotation rotate_frame_;
};

class VideoCaptureTest {
public:
    VideoCaptureTest() : number_of_devices_(0) {}
    
    void SetUp()  {
        device_info_.reset(VideoCaptureFactory::CreateDeviceInfo());
        assert(device_info_.get());
        number_of_devices_ = device_info_->NumberOfDevices();
        ASSERT_GT(number_of_devices_, 0u);
    }
    
    rtc::scoped_refptr<VideoCaptureModule> OpenVideoCaptureDevice(
                                                                  unsigned int device,
                                                                  rtc::VideoSinkInterface<webrtc::VideoFrame>* callback) {
        
        std::unique_ptr<webrtc::VideoCaptureModule::DeviceInfo> info(
                                                                     webrtc::VideoCaptureFactory::CreateDeviceInfo());
        char device_name[256];
        char unique_name[256];
        int deviceCount = info->NumberOfDevices();
        for (int i = 0; i < deviceCount; i++){
           
            assert(info->GetDeviceName(static_cast<uint32_t>(i),
                                           device_name, sizeof(device_name), unique_name,
                                       sizeof(unique_name)) == 0);
            fprintf(stderr, "device:%s | %s\n", device_name, unique_name);
            VideoCaptureCapability cap;
            for (int j = 0; j < info->NumberOfCapabilities(unique_name); j++) {
                assert( info->GetCapability(unique_name, j, cap) == 0);
                fprintf(stderr, "\t%d %d %dfps %s\n", cap.width, cap.height, cap.maxFPS, getVideoTypeName(cap.videoType));

            }
            
        }
        
        EXPECT_EQ(0, device_info_->GetDeviceName(device, device_name, 256,
                                                 unique_name, 256));
        
        rtc::scoped_refptr<VideoCaptureModule> module(
                                                      VideoCaptureFactory::Create(unique_name));
        if (module.get() == NULL)
            return NULL;
        
        EXPECT_FALSE(module->CaptureStarted());
        
        module->RegisterCaptureDataCallback(callback);
        return module;
    }
    
    void StartCapture(VideoCaptureModule* capture_module,
                      VideoCaptureCapability capability) {
        ASSERT_EQ(0, capture_module->StartCapture(capability));
        EXPECT_TRUE(capture_module->CaptureStarted());
        
        VideoCaptureCapability resulting_capability;
        EXPECT_EQ(0, capture_module->CaptureSettings(resulting_capability));
        EXPECT_EQ(capability.width, resulting_capability.width);
        EXPECT_EQ(capability.height, resulting_capability.height);
    }
    
    std::unique_ptr<VideoCaptureModule::DeviceInfo> device_info_;
    unsigned int number_of_devices_;
    
    void testCap() {
        
        SetUp();
        
        for (int i = 0; i < 1; ++i) {
            int64_t start_time = rtc::TimeMillis();
            TestVideoCaptureCallback capture_observer;
            rtc::scoped_refptr<VideoCaptureModule> module(
                                                          OpenVideoCaptureDevice(0, &capture_observer));
            ASSERT_TRUE(module.get() != NULL);
            
            VideoCaptureCapability capability;
#ifndef WEBRTC_MAC
            device_info_->GetCapability(module->CurrentDeviceName(), 0, capability);
#else
            capability.width = kTestWidth;
            capability.height = kTestHeight;
            capability.maxFPS = kTestFramerate;
            capability.videoType = webrtc::VideoType::kUnknown;
#endif
            capture_observer.SetExpectedCapability(capability);
            StartCapture(module.get(), capability);
            
            // Less than 4s to start the camera.
            EXPECT_LE(rtc::TimeMillis() - start_time, 4000);
            
            // Make sure 5 frames are captured.
            EXPECT_TRUE_WAIT(capture_observer.incoming_frames() >= 5, kTimeOut);
            
            int64_t stop_time = rtc::TimeMillis();
            EXPECT_EQ(0, module->StopCapture());
            EXPECT_FALSE(module->CaptureStarted());
            
            // Less than 3s to stop the camera.
            EXPECT_LE(rtc::TimeMillis() - stop_time, 3000);
        }
    }
};

int main(int argc, char **argv) {
    VideoCaptureTest vct;
    vct.testCap();
}
