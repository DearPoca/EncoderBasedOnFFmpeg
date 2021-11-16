#include "frame_queue.h"

FrameQueue::FrameQueue(uint buffers_num, uint buffer_size, WebmMediaEncoder *rgba_to_webm)
{
    buffers_num_ = buffers_num;
    buffer_size_ = buffer_size;
    rgba_to_webm_ = rgba_to_webm;
    for (uint i = 0; i < buffers_num_; i++)
    {
        std::shared_ptr<FrameData> frame(new FrameData());
        frame->data = (uint8_t *)malloc(buffer_size_);
        frame->size = -1;
        queue_empty_.push(frame);
        empty_.Signal();
    }
}

FrameQueue::~FrameQueue()
{
    while (!queue_empty_.empty())
    {
        std::shared_ptr<FrameData> frame = queue_empty_.front();
        if (frame->data != nullptr)
            free(frame->data);
        queue_empty_.pop();
    }
}

void FrameQueue::Start()
{
    finished_.store(false);
    auto process = [&]()
    {
        while (true)
        {
            full_.Wait();

            queue_full_locker_.lock();
            if (finished_.load() == true && queue_full_.empty())
            {
                queue_full_locker_.unlock();
                break;
            }
            std::shared_ptr<FrameData> frame = queue_full_.front();
            queue_full_.pop();
            queue_full_locker_.unlock();

            rgba_to_webm_->EncodeFrame(frame->data, frame->size);

            queue_empty_locker_.lock();
            queue_empty_.push(frame);
            queue_empty_locker_.unlock();

            empty_.Signal();
        }
        rgba_to_webm_->FinishEncoding(); //告知编码器视频已经结束
    };

    std::thread th(process);
    th.detach();
}

void FrameQueue::PushFrame(uint8_t *data, int size)
{
    empty_.Wait();

    queue_empty_locker_.lock();
    std::shared_ptr<FrameData> frame = queue_empty_.front();
    queue_empty_.pop();
    queue_empty_locker_.unlock();

    memcpy(frame->data, data, size);
    frame->size = size;

    queue_full_locker_.lock();
    queue_full_.push(frame);
    queue_full_locker_.unlock();

    full_.Signal();
}

void FrameQueue::EndEncode()
{
    finished_.store(true);
    full_.Signal();
}