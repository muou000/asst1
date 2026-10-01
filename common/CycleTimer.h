#ifndef SYRAH_CYCLE_TIMER_H_
#define SYRAH_CYCLE_TIMER_H_

#include <chrono>
#include <cstdint>

class CycleTimer {
 public:
  using SysClock = std::uint64_t;
  using Clock = std::chrono::steady_clock;
  using Seconds = std::chrono::duration<double>;

  // 返回自第一次调用以来经过的秒数，使用单调时钟（monotonic clock）。
  // 自 C++11 起是线程安全的（静态局部变量的初始化是线程安全的）。
  static inline double currentSeconds() {
    static const Clock::time_point t0 = Clock::now();
    return Seconds{Clock::now() - t0}.count();
  }

  // 工具类：禁止实例化。
  CycleTimer() = delete;
  ~CycleTimer() = delete;
  CycleTimer(const CycleTimer&) = delete;
  CycleTimer& operator=(const CycleTimer&) = delete;
};

#endif  // SYRAH_CYCLE_TIMER_H_