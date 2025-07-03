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
#include <utility>
#include <vector>

namespace stl {
class ThreadPool {
 public:
  ThreadPool(size_t num_threads) {
    thread_workers_.reserve(num_threads);

    for (size_t i = 0; i < num_threads; i++) {
      thread_workers_.emplace_back(
          [this](std::stop_token stop_token) { this->worker(stop_token); });
    }
  }

  template <typename F, typename... Args>
  auto submit_task(F&& f, Args&&... args) -> std::future<
      std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
    using return_type =
        std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

    // Create a packaged_task to get the future
    // Perfect forwarding
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        [f = std::forward<F>(f), args = std::make_tuple(std::forward<Args>(
                                     args)...)]() mutable -> return_type {
          return std::apply(f, std::move(args));
        });

    auto future = task->get_future();

    {
      std::scoped_lock lock(mutex_);
      if (is_shutdown_.load(std::memory_order_relaxed)) {
        // If shutdown, return an invalid future or throw
        std::promise<return_type> promise;
        promise.set_exception(std::make_exception_ptr(
            std::runtime_error("ThreadPool is shut down")));
        return promise.get_future();
      }
      task_queue_.emplace([task]() { (*task)(); });
    }

    cv_.notify_one();
    return future;
  }

  void shutdown() {
    is_shutdown_.store(true, std::memory_order_relaxed);
    cv_.notify_all();
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
      std::function<void()> task;

      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&]() {
          return !task_queue_.empty() ||
                 is_shutdown_.load(std::memory_order_relaxed) ||
                 stop_token.stop_requested();
        });

        // Check if we should exit
        if (stop_token.stop_requested() ||
            (is_shutdown_.load(std::memory_order_relaxed) &&
             task_queue_.empty())) {
          return;
        }

        // If we have tasks, get one
        if (!task_queue_.empty()) {
          task = std::move(task_queue_.front());
          task_queue_.pop();
        } else {
          continue;
        }
      }  // unlocked

      if (task) {
        task();
      }
    }
  }
};

}  // namespace stl
