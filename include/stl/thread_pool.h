/**
 * @file thread_pool.h
 * @brief Implemetation of thread pool
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <vector>

namespace stl {
class ThreadPool {
 public:
  ThreadPool(size_t num_threads) {
    thread_workers_.reserve(num_threads);

    for (int i = 0; i < num_threads; i++) {
      thread_workers_.emplace_back(&ThreadPool::worker, this);
    }
  }

  template <typename F, typename... Args>
  auto submit_task(F&& f, Args args)
      -> std::future<std::invoke_result<F, Args...>> {
    std::scoped_lock lock(mutex_);

    // TODO: fix this with perfect forwarding
    auto task = std::bind(f, args);
    task_queue_.push(std::move(task));

    cv_.notify_one();
    // TODO: return here
  }

  void shutdown() {
    is_shutdown_.store(true, std::memory_order_relaxed);
    for (auto& thread : thread_workers_) {
      thread.request_stop();
    }
  }

  ~ThreadPool() {
    shutdown();
    // jthreads will automatically join and request_stop, so no need to join
  }

 private:
  std::vector<std::jthread> thread_workers_;
  std::queue<std::function<void()>> task_queue_;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> is_shutdown_ = false;

  void worker(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [&]() {
        return !task_queue_.empty() ||
               is_shutdown_.load(std::memory_order_relaxed);
      });

      // Check if shutting down already
      if (is_shutdown_.load(std::memory_order_relaxed)) {
        return;
      }

      auto task = task_queue_.front();
      task_queue_.pop();
      lock.unlock();

      task();
    }
  }
};

}  // namespace stl
