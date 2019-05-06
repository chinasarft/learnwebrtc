#include <memory>
#include <cstdint>

enum class VideoFormat{
        Same,
        H264,
        H265,
};

enum class AudioFormat {
        Same,
        AAC,
};

class RtpRtcpWebrtcImpl;

class RtpRtcpImpl {
public:
        RtpRtcpImpl();
        int SendVideo(char *pData, int nLen, bool isKey, int64_t nTimestamp);
        int SendAduio(char *pData, int nLen, int64_t nTimestamp);
        int SendData(char *pData, int nLen, int64_t nTimestamp);
        int ChangeAVFormat(AudioFormat atype, VideoFormat vtype);
        AudioFormat GetAudioFormat(){return audioFormat_;}
        VideoFormat GetVideoFormat(){return videoFormat_;}
private:
        AudioFormat audioFormat_;
        VideoFormat videoFormat_;
        std::unique_ptr<RtpRtcpWebrtcImpl> rtpRtcpImpl_;
};
