#include "thread.h"

#if defined(_MSC_VER)
#include <Windows.h>
#if defined(max)
#undef max
#endif  // max
#if defined(min)
#undef min
#endif  // min
#endif

#include "../include/log.h"
#include "../utils/time_utils.h"

namespace qosrtp {
CallableWrapper::CallableWrapper() = default;

CallableWrapper::~CallableWrapper() = default;

ThreadWaitTask::ThreadWaitTask() = default;

ThreadWaitTask::~ThreadWaitTask() = default;

Thread::DelayedTask::DelayedTask(std::unique_ptr<CallableWrapper> task,
                                 uint64_t wait_duration_ms)
    : f(std::move(task)),
      execution_time_utc_ms(UTCTimeMillis() + wait_duration_ms) {}

Thread::DelayedTask::~DelayedTask() = default;

Thread::Thread(std::string thread_name, ThreadWaitTask* wait_task)
    : thread_name_(thread_name), thread_() {
  wait_task_ = wait_task;
  runing_.store(false);
  should_stop_.store(false);
}

Thread::~Thread() { Stop(); }

void Thread::Start() {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    if ((runing_.load())) {
      return;
    }
  }
  should_stop_.store(false);
  thread_ = std::thread(&Thread::ThreadMain, this);
  {
    std::unique_lock<std::mutex> lock(mutex_);
    runing_.store(true);
  }
}

void Thread::Stop() {
  {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!(runing_.load())) {
      return;
    }
  }
  should_stop_.store(true);
  if (wait_task_) wait_task_->WaitUp();
  if (thread_.joinable()) {
    thread_.join();
  }
  {
    std::unique_lock<std::mutex> lock(mutex_);
    while (!task_queue_.empty()) {
      task_queue_.pop();
    }
    delayed_task_list_.clear();
    should_stop_.store(false);
    runing_.store(false);
  }
}

void Thread::PushTask(std::unique_ptr<CallableWrapper> f) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!(runing_.load())) {
    return;
  }
  task_queue_.push(std::move(f));
  if (wait_task_) wait_task_->WaitUp();
}

void Thread::PushTask(std::unique_ptr<CallableWrapper> f,
                      uint64_t wait_duration_ms) {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!(runing_.load())) {
    return;
  }
  std::unique_ptr<DelayedTask> delayed_task =
      std::make_unique<DelayedTask>(std::move(f), wait_duration_ms);
  delayed_task_list_.push_back(std::move(delayed_task));
  if (wait_task_) wait_task_->WaitUp();
}

void Thread::ThreadMain() {
  QOSRTP_LOG(Info, "Thread begin: %s", thread_name_.c_str());
#if defined(_MSC_VER)
  int size_thread_name =
      MultiByteToWideChar(CP_UTF8, 0, thread_name_.c_str(), -1, NULL, 0);
  wchar_t* thread_name = new wchar_t[size_thread_name];
  MultiByteToWideChar(CP_UTF8, 0, thread_name_.c_str(), -1, thread_name,
                      size_thread_name);
  HRESULT hr = SetThreadDescription(GetCurrentThread(), thread_name);
  delete[] thread_name;
  if (FAILED(hr)) {
    QOSRTP_LOG(Error, "Failed to name thread: %s", thread_name_.c_str());
  }
#endif
  std::unique_ptr<CallableWrapper> task_f = nullptr;
  std::unique_ptr<DelayedTask> delayed_task = nullptr;
  uint64_t wait_duration_ms = ThreadWaitTask::kForever;
  uint64_t utc_ms_now = UTCTimeMillis();
  while (!should_stop_.load()) {
    wait_duration_ms = ThreadWaitTask::kForever;
    /* todo 加入对延迟队列的处理，并且需要改变rtcp的调度方法 */
    do {
      task_f.reset(nullptr);
      delayed_task.reset(nullptr);
      {
        std::unique_lock<std::mutex> lock(mutex_);
        utc_ms_now = UTCTimeMillis();
        for (auto iter_delayed_task = delayed_task_list_.begin();
             iter_delayed_task != delayed_task_list_.end();
             iter_delayed_task++) {
          if ((*iter_delayed_task)->execution_time_utc_ms < wait_duration_ms)
            wait_duration_ms = (*iter_delayed_task)->execution_time_utc_ms;
          if ((*iter_delayed_task)->execution_time_utc_ms <= utc_ms_now) {
            delayed_task = std::move(*iter_delayed_task);
            delayed_task_list_.erase(iter_delayed_task);
            wait_duration_ms = 0;
            break;
          }
        }
        if (!task_queue_.empty()) {
          task_f = std::move(task_queue_.front());
          task_queue_.pop();
        }
        if ((!task_f) && (!delayed_task)) break;
      }
      if (task_f)
        (*task_f)();
      if (delayed_task)
        (*(delayed_task->f))();
    } while (!should_stop_.load());
    if (should_stop_.load()) break;
    if (wait_task_) wait_task_->Wait(wait_duration_ms);
  }
  QOSRTP_LOG(Info, "Thread end: %s", thread_name_.c_str());
}
}  // namespace qosrtp