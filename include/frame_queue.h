#ifndef FRAME_QUEUE_H_425246988959242C
#define FRAME_QUEUE_H_425246988959242C

#include <unistd.h>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

#include "semaphore.h"
#include "log_util.h"
#include "webm_encoder.h"

struct FrameData
{
    int size;
    uint8_t *data;
};

class FrameQueue
{
    std::queue<std::shared_ptr<FrameData>> queue_full_;
    std::mutex queue_full_locker_;
    std::queue<std::shared_ptr<FrameData>> queue_empty_;
    std::mutex queue_empty_locker_;
    uint buffer_size_;
    uint buffers_num_;
    Semaphore full_;
    Semaphore empty_;
    std::atomic_bool finished_;

public:
    FrameQueue(uint buffers_num, uint buffer_size, WebmMediaEncoder *rgba_to_webm);

    ~FrameQueue();

    void Start();

    void PushFrame(uint8_t *data, int size);

    void EndEncode();

    WebmMediaEncoder *rgba_to_webm_;
};

#endif