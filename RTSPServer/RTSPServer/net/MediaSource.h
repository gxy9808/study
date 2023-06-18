#ifndef _MEDIA_SOURCE_H_
#define _MEDIA_SOURCE_H_
#include <queue>
#include <stdint.h>

#include "net/UsageEnvironment.h"
#include "base/Mutex.h"

#define FRAME_MAX_SIZE (1024*500)
#define DEFAULT_FRAME_NUM 4

class AVFrame
{
public:
    AVFrame() :
        mBuffer(new uint8_t[FRAME_MAX_SIZE]),
        mFrameSize(0)
    { }

    ~AVFrame()
    { delete mBuffer; }

    uint8_t* mBuffer;
    uint8_t* mFrame;
    int mFrameSize;
};

// 音视频媒体资源
class MediaSource
{
public:
    virtual ~MediaSource();

    AVFrame* getFrame();
    void putFrame(AVFrame* frame);
    int getFps() const { return mFps; }

protected:
    MediaSource(UsageEnvironment* env);
    virtual void readFrame() = 0;
    void setFps(int fps) { mFps = fps; }   // 设置fps

private:
    static void taskCallback(void*);       // 任务回调

protected:
    UsageEnvironment* mEnv;                   // 环境设置（封装了事件调度器和线程池）
    AVFrame mAVFrames[DEFAULT_FRAME_NUM];     // AV帧资源初始化数组
    std::queue<AVFrame*> mAVFrameInputQueue;  // AV帧输入队列，用于取出帧资源进行生产
    std::queue<AVFrame*> mAVFrameOutputQueue; // AV帧输出队列，用于取出帧资源进行消费
    Mutex* mMutex;
    ThreadPool::Task mTask;                   // 线程任务
    int mFps;
};

#endif //_MEDIA_SOURCE_H_