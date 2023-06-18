#ifndef _RTSP_CONNECTION_
#define _RTSP_CONNECTION_
#include <map>

#include "net/TcpConnection.h"
#include "net/RtspServer.h"
#include "net/RtpInstance.h"
#include "net/MediaSession.h"

class RtspServer;

class RtspConnection : public TcpConnection
{
public:
    enum Method
    {
        OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN, GET_PARAMETER, RTCP,
        NONE,
    };

    // 创建RTSP连接
    static RtspConnection* createNew(RtspServer* rtspServer, int sockfd);
    
    // RTSP连接初始化
    RtspConnection(RtspServer* rtspServer, int sockfd);
    ~RtspConnection();

protected:
    virtual void handleReadBytes();

private:
    bool parseRequest();                                     // 解析客户端发送的RTSP请求
    bool parseRequest1(const char* begin, const char* end);  // 解析请求行
    bool parseRequest2(const char* begin, const char* end);

    bool parseCSeq(std::string& message);
    bool parseAccept(std::string& message);
    bool parseTransport(std::string& message);
    bool parseMediaTrack();
    bool parseSessionId(std::string& message);

    bool handleCmdOption();
    bool handleCmdDescribe();
    bool handleCmdSetup();
    bool handleCmdPlay();
    bool handleCmdTeardown();
    bool handleCmdGetParamter();

    int sendMessage(void* buf, int size);  // 向缓存区中写入数据并发送
    int sendMessage();

    // 为RTP、RTCP在UDP上的传输创建地址和端口
    bool createRtpRtcpOverUdp(MediaSession::TrackId trackId, std::string peerIp,
                        uint16_t peerRtpPort, uint16_t peerRtcpPort);
    bool createRtpOverTcp(MediaSession::TrackId trackId, int sockfd, uint8_t rtpChannel);

    void handleRtpOverTcp();

private:
    RtspServer* mRtspServer;
    std::string mPeerIp;
    Method mMethod;
    std::string mUrl;
    std::string mSuffix;
    uint32_t mCSeq;
    uint16_t mPeerRtpPort;
    uint16_t mPeerRtcpPort;
    MediaSession::TrackId mTrackId;
    RtpInstance* mRtpInstances[MEDIA_MAX_TRACK_NUM];
    RtcpInstance* mRtcpInstances[MEDIA_MAX_TRACK_NUM];
    MediaSession* mSession;
    int mSessionId;
    bool mIsRtpOverTcp;
    uint8_t mRtpChannel;
};

#endif //_RTSP_CONNECTION_