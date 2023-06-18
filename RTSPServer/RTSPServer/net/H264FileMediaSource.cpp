#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "net/H264FileMediaSource.h"
#include "base/Logging.h"
#include "base/New.h"

static inline int startCode3(uint8_t* buf);
static inline int startCode4(uint8_t* buf);

// 创建一个H264视频流
H264FileMediaSource* H264FileMediaSource::createNew(UsageEnvironment* env, std::string file)
{
    //return new H264FileMediaSource(env, file);
    return New<H264FileMediaSource>::allocate(env, file);
}

// H264视频流资源初始化
H264FileMediaSource::H264FileMediaSource(UsageEnvironment* env, const std::string& file) :
    MediaSource(env),
    mFile(file)
{
    mFd = ::open(file.c_str(), O_RDONLY);
    assert(mFd > 0);

    setFps(25);

    for(int i = 0; i < DEFAULT_FRAME_NUM; ++i)
        mEnv->threadPool()->addTask(mTask);  // 线程池初始化添加几个线程任务
}

H264FileMediaSource::~H264FileMediaSource()
{
    ::close(mFd);
}

// 读取H264视频帧
void H264FileMediaSource::readFrame()
{
    MutexLockGuard mutexLockGuard(mMutex);

    if(mAVFrameInputQueue.empty())
        return;

    // 从frame输入队列中取出数据帧作为 H264视频帧资源的载体
    AVFrame* frame = mAVFrameInputQueue.front(); 
    // 获取H264视频帧，返回帧大小， 从mFd中读取帧数据到缓存区中
    frame->mFrameSize = getFrameFromH264File(mFd, frame->mBuffer, FRAME_MAX_SIZE);
    if(frame->mFrameSize < 0)
        return;

    if(startCode3(frame->mBuffer))
    {
        frame->mFrame = frame->mBuffer+3;
        frame->mFrameSize -= 3;
    }
    else
    {
        frame->mFrame = frame->mBuffer+4;
        frame->mFrameSize -= 4;
    }

    mAVFrameInputQueue.pop();         // 将载有视频帧资源的frame从输入队列出栈（生产完毕）
    mAVFrameOutputQueue.push(frame);  // 将视频帧资源frame加入输出队列中，等待消费（输出队列中的数据由定时器进行回调读取并发送）
}

static inline int startCode3(uint8_t* buf)
{
    if(buf[0] == 0 && buf[1] == 0 && buf[2] == 1)
        return 1;
    else
        return 0;
}

static inline int startCode4(uint8_t* buf)
{
    if(buf[0] == 0 && buf[1] == 0 && buf[2] == 0 && buf[3] == 1)
        return 1;
    else
        return 0;
}

static uint8_t* findNextStartCode(uint8_t* buf, int len)
{
    int i;

    if(len < 3)
        return NULL;

    for(i = 0; i < len-3; ++i)
    {
        if(startCode3(buf) || startCode4(buf))
            return buf;
        
        ++buf;
    }

    if(startCode3(buf))
        return buf;

    return NULL;
}

// 从H264文件中读取视频帧资源
int H264FileMediaSource::getFrameFromH264File(int fd, uint8_t* frame, int size)
{
    int rSize, frameSize;
    uint8_t* nextStartCode;

    if(fd < 0)
        return fd;

    rSize = read(fd, frame, size);
    if(!startCode3(frame) && !startCode4(frame))
        return -1;
    
    nextStartCode = findNextStartCode(frame+3, rSize-3);
    if(!nextStartCode)
    {
        lseek(fd, 0, SEEK_SET);
        frameSize = rSize;
    }
    else
    {
        frameSize = (nextStartCode-frame);
        lseek(fd, frameSize-rSize, SEEK_CUR);
    }

    return frameSize;
}