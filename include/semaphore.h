#ifndef SEMAPHORE_H_NESSAJ_340975074234223
#define SEMAPHORE_H_NESSAJ_340975074234223

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "log_util.h"

class Semaphore
{
public:
  explicit Semaphore(int count = 0);

  void Signal();

  void Wait();

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  int count_;
};

#endif