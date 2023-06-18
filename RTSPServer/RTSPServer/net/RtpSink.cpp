#include <arpa/inet.h>

#include "net/RtpSink.h"
#include "base/Logging.h"
#include "base/New.h"


RtpSink::RtpSink(UsageEnvironment* env, MediaSource* mediaSource, int payloadType) :
    mMediaSource(mediaSource),
    mSendPacketCallback(NULL),
    mEnv(env),
    mCsrcLen(0),
    mExtension(0),
    mPadding(0),
    mVersion(RTP_VESION),
    mPayloadType(payloadType),
    mMarker(0),
    mSeq(0),
    mTimestamp(0),
    mTimerId(0)
    
{
    mTimerEvent = TimerEvent::createNew(this);         // 创建一个定时事件
    mTimerEvent->setTimeoutCallback(timeoutCallback);  // 设置定时事件的超时回调

    mSSRC = rand();
}

RtpSink::~RtpSink()
{
    mEnv->scheduler()->removeTimedEvent(mTimerId);
    //delete mTimerEvent;
    Delete::release(mTimerEvent);
}

void RtpSink::setSendFrameCallback(SendPacketCallback cb, void* arg1, void* arg2)
{
    mSendPacketCallback = cb;
    mArg1 = arg1;
    mArg2 = arg2;
}

void RtpSink::sendRtpPacket(RtpPacket* packet)
{
    RtpHeader* rtpHead = packet->mRtpHeadr;
    rtpHead->csrcLen = mCsrcLen;
    rtpHead->extension = mExtension;
    rtpHead->padding = mPadding;
    rtpHead->version = mVersion;
    rtpHead->payloadType = mPayloadType;
    rtpHead->marker = mMarker;
    rtpHead->seq = htons(mSeq);
    rtpHead->timestamp = htonl(mTimestamp);
    rtpHead->ssrc = htonl(mSSRC);
    packet->mSize += RTP_HEADER_SIZE;
    
    if(mSendPacketCallback)
        mSendPacketCallback(mArg1, mArg2, packet);
}

// RTP包处理时的超时回调函数
void RtpSink::timeoutCallback(void* arg)
{
    RtpSink* rtpSink = (RtpSink*)arg;
    AVFrame* frame = rtpSink->mMediaSource->getFrame();  // 从音媒体资源中获取帧数据
    if(!frame)
    {
        return;
    }

    // 由具体子类实现发送逻辑
    rtpSink->handleFrame(frame);            
    // 将使用过的frame插入输入队列，插入输入队列后，加入一个子线程task（frame作为运输载体，帧资源循环使用）
    rtpSink->mMediaSource->putFrame(frame);  
}

// 开启定时， 每次事件轮回，对定时器进行处理
void RtpSink::start(int ms)
{
    // 事件调度器添加定时事件
    mTimerId = mEnv->scheduler()->addTimedEventRunEvery(mTimerEvent, ms);
}

void RtpSink::stop()
{
    mEnv->scheduler()->removeTimedEvent(mTimerId);
}