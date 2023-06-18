#include "net/MediaSource.h"
#include "base/Logging.h"
#include "base/New.h"

// 媒体资源初始化
MediaSource::MediaSource(UsageEnvironment* env) :
    mEnv(env)
{
    mMutex = Mutex::createNew();
    for(int i = 0; i < DEFAULT_FRAME_NUM; ++i)    // 把初始化后的frame加入到输入队列中
        mAVFrameInputQueue.push(&mAVFrames[i]);
    
    mTask.setTaskCallback(taskCallback, this);    // 线程任务设置任务回调
}

MediaSource::~MediaSource()
{
    //delete mMutex;
    Delete::release(mMutex);
}

// 从输入队列中获取初始化后的空白帧资源
AVFrame* MediaSource::getFrame()
{
    MutexLockGuard mutexLockGuard(mMutex);  // 加锁

    if(mAVFrameOutputQueue.empty())
    {
        return NULL;
    }

    AVFrame* frame = mAVFrameOutputQueue.front();    
    mAVFrameOutputQueue.pop();

    return frame;
}

// 把使用过的frame重新插入输入队列，用于循环使用
void MediaSource::putFrame(AVFrame* frame)
{
    MutexLockGuard mutexLockGuard(mMutex);

    mAVFrameInputQueue.push(frame);      // 加入到frame输入队列中
    
    mEnv->threadPool()->addTask(mTask);  // 线程池加入当前的线程任务
}

// 任务回调
void MediaSource::taskCallback(void* arg)
{
    MediaSource* source = (MediaSource*)arg;
    source->readFrame();  // 读取H264或AAC音视频资源的数据帧
}
