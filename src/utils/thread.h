#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <limits>
#include <mutex>
#include <queue>
#include <list>
#include <thread>
#include <tuple>
#include <type_traits>

namespace qosrtp {
class CallableWrapper {
 public:
  template <class Callable>
  static std::unique_ptr<CallableWrapper> Wrap(Callable&& callable_obj) {
    static_assert(!std::is_lvalue_reference_v<Callable>,
                  "bad CallableWrapper::wrap call");
    static_assert(std::is_invocable_v<Callable>, "Callable must be invocable");
    return std::make_unique<CallableWrapperImpl<Callable>>(
        std::forward<Callable>(callable_obj));
  }
  // Note: All incoming parameters will be copied or moved into the class for
  // storage. Exercise caution when invoking the function with reference
  // parameters or types that can only be moved, especially in
  // repeated calls, as the parameters may hold unexpected values in subsequent
  // invocations.
  template <class Callable, class... Args>
  static std::unique_ptr<CallableWrapper> Wrap(Callable&& callable_obj,
                                               Args&&... args) {
    static_assert(!std::is_lvalue_reference_v<Callable>,
                  "bad CallableWrapper::wrap call");
    static_assert(std::is_invocable_v<Callable, Args...>,
                  "Callable must be invocable with the provided arguments");
    return std::make_unique<CallableWrapperImplWithArgs<Callable, Args...>>(
        std::forward<Callable>(callable_obj), std::forward<Args>(args)...);
  }

  CallableWrapper();
  virtual ~CallableWrapper();
  virtual void operator()() = 0;
};

template <class Callable, class... Args>
class CallableWrapperImplWithArgs : public CallableWrapper {
 public:
  CallableWrapperImplWithArgs(Callable&& callable_obj, Args&&... args)
      : CallableWrapper(),
        callable_obj_(std::forward<Callable>(callable_obj)),
        args_(std::forward<Args>(args)...) {}
  virtual ~CallableWrapperImplWithArgs() override = default;
  virtual void operator()() { std::apply(callable_obj_, std::move(args_)); }

 private:
  std::remove_reference_t<Callable> callable_obj_;
  std::tuple<std::remove_reference_t<Args>...> args_;
};

template <class Callable>
class CallableWrapperImpl : public CallableWrapper {
 public:
  CallableWrapperImpl(Callable&& callable_obj)
      : CallableWrapper(),
        callable_obj_(std::forward<Callable>(callable_obj)) {}
  virtual ~CallableWrapperImpl() override = default;
  virtual void operator()() { callable_obj_(); }

 private:
  std::remove_reference_t<Callable> callable_obj_;
};

class ThreadWaitTask {
 public:
#ifdef max
#undef max
#endif
  static constexpr uint64_t kForever = std::numeric_limits<uint64_t>::max();
  // max_wait_duration_ms = std::numeric_limits<uint64_t>::max() means no time
  // limit
  virtual void Wait(uint64_t max_wait_duration_ms) = 0;
  virtual void WaitUp() = 0;

 protected:
  ThreadWaitTask();
  ~ThreadWaitTask();
};

class Thread {
 public:
  Thread() = delete;
  Thread(std::string thread_name, ThreadWaitTask* wait_task);
  ~Thread();
  void Start();
  void Stop();
  void PushTask(std::unique_ptr<CallableWrapper> f);
  void PushTask(std::unique_ptr<CallableWrapper> f, uint64_t wait_duration_ms);
  bool IsCurrent() { return (std::this_thread::get_id() == thread_.get_id()); }

 private:
  void ThreadMain();
  std::thread thread_;
  std::mutex mutex_;
  std::queue<std::unique_ptr<CallableWrapper>> task_queue_;
  struct DelayedTask {
    DelayedTask(std::unique_ptr<CallableWrapper> task,
                uint64_t wait_duration_ms);
    ~DelayedTask();
    uint64_t execution_time_utc_ms;
    std::unique_ptr<CallableWrapper> f;
  };
  std::list<std::unique_ptr<DelayedTask>> delayed_task_list_;
  std::atomic<bool> should_stop_;
  std::atomic<bool> runing_;
  ThreadWaitTask* wait_task_;
  const std::string thread_name_;
};
}  // namespace qosrtp