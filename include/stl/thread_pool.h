/**
 * @file thread_pool.h
 * @brief Implementation of thread pool
 *
 * shutdown() (and the destructor) rejects new tasks, runs the ones already
 * queued, then joins the workers. Don't call it from inside a task.
 */

#pragma once

#include <cstddef>
#include <condition_variable>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace stl {
class ThreadPool {
 public:
  explicit ThreadPool(std::size_t num_threads) {
    thread_workers_.reserve(num_threads);

    try {
      for (std::size_t i = 0; i < num_threads; i++) {
        thread_workers_.emplace_back(
            [this](std::stop_token stop_token) { this->worker(stop_token); });
      }
    } catch (...) {
      // Stop the threads that did start
      shutdown();
      throw;
    }
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;
  // Workers hold `this`
  ThreadPool(ThreadPool&&) = delete;
  ThreadPool& operator=(ThreadPool&&) = delete;

  template <typename F, typename... Args>
  auto submit_task(F&& f, Args&&... args) -> std::future<
      std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
    using return_type =
        std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

    // shared_ptr because packaged_task is move-only and std::function needs
    // something copyable
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        [f = std::forward<F>(f), args = std::make_tuple(std::forward<Args>(
                                     args)...)]() mutable -> return_type {
          return std::apply(f, std::move(args));
        });

    auto future = task->get_future();

    {
      std::scoped_lock lock(mutex_);
      if (is_shutdown_) {
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
    {
      // Has to be under mutex_, otherwise a worker can miss the notify and
      // never wake up
      std::scoped_lock lock(mutex_);
      is_shutdown_ = true;
      for (auto& thread : thread_workers_) {
        thread.request_stop();
      }
    }
    cv_.notify_all();

    for (auto& thread : thread_workers_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

  ~ThreadPool() { shutdown(); }

 private:
  std::queue<std::function<void()>> task_queue_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool is_shutdown_ = false;  // guarded by mutex_

  // Last so the threads are joined before the members above are destroyed
  std::vector<std::jthread> thread_workers_;

  void worker(std::stop_token stop_token) {
    for (;;) {
      std::function<void()> task;

      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [&]() {
          return !task_queue_.empty() || stop_token.stop_requested();
        });

        // Drain the queue before exiting
        if (task_queue_.empty()) {
          return;
        }

        task = std::move(task_queue_.front());
        task_queue_.pop();
      }  // unlocked

      // packaged_task stores any exception in the future, so this never
      // throws.
      task();
    }
  }
};

}  // namespace stl
