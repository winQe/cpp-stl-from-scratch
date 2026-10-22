#define CATCH_CONFIG_MAIN
#include <algorithm>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <exception>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "stl/thread_pool.h"

// Helper function for testing
int multiply(int a, int b) { return a * b; }

// Helper class for testing with state
struct Counter {
  std::atomic<int> count{0};
  void increment() { count.fetch_add(1); }
  int get() const { return count.load(); }
};

TEST_CASE("ThreadPool basic construction") {
  SECTION("Default construction with threads") {
    REQUIRE_NOTHROW([]() { stl::ThreadPool pool(4); }());
  }

  SECTION("Single thread construction") {
    REQUIRE_NOTHROW([]() { stl::ThreadPool pool(1); }());
  }
}

TEST_CASE("ThreadPool task submission") {
  SECTION("Submit simple task") {
    stl::ThreadPool pool(2);

    auto future = pool.submit_task(multiply, 6, 7);

    // This should compile and return a future
    REQUIRE(future.valid());
    REQUIRE(future.get() == 42);
  }

  SECTION("Submit lambda task") {
    stl::ThreadPool pool(2);

    auto future = pool.submit_task([](int x) { return x * 2; }, 21);

    REQUIRE(future.valid());
    REQUIRE(future.get() == 42);
  }

  SECTION("Submit multiple tasks") {
    stl::ThreadPool pool(4);
    std::vector<std::future<int>> futures;

    // Submit 10 tasks
    for (int i = 0; i < 10; ++i) {
      futures.push_back(pool.submit_task([i]() { return i * i; }));
    }

    // Verify all results
    for (int i = 0; i < 10; ++i) {
      REQUIRE(futures[i].get() == i * i);
    }
  }
}

TEST_CASE("ThreadPool concurrent execution") {
  SECTION("Concurrent task execution") {
    stl::ThreadPool pool(4);
    Counter counter;
    std::vector<std::future<void>> futures;

    // Submit 20 tasks that increment counter
    for (int i = 0; i < 20; ++i) {
      futures.push_back(pool.submit_task([&counter]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        counter.increment();
      }));
    }

    // Wait for all tasks to complete
    for (auto& future : futures) {
      future.wait();
    }

    REQUIRE(counter.get() == 20);
  }

  SECTION("Task execution order independence") {
    stl::ThreadPool pool(4);
    std::atomic<int> execution_order{0};
    std::vector<std::future<int>> futures;

    // Submit tasks that return their execution order
    for (int i = 0; i < 10; ++i) {
      futures.push_back(pool.submit_task(
          [&execution_order]() { return execution_order.fetch_add(1); }));
    }

    std::vector<int> results;
    for (auto& future : futures) {
      results.push_back(future.get());
    }

    // Results should be 0-9 in some order
    std::sort(results.begin(), results.end());
    for (int i = 0; i < 10; ++i) {
      REQUIRE(results[i] == i);
    }
  }
}

TEST_CASE("ThreadPool shutdown") {
  SECTION("Shutdown stops accepting new tasks") {
    stl::ThreadPool pool(2);

    // Submit a task to keep threads busy
    auto future = pool.submit_task([]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      return 42;
    });

    pool.shutdown();

    // Original task should still complete
    REQUIRE(future.get() == 42);
  }

  SECTION("Destructor calls shutdown") {
    Counter counter;

    {
      stl::ThreadPool pool(2);

      // Submit tasks
      for (int i = 0; i < 5; ++i) {
        pool.submit_task([&counter]() {
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
          counter.increment();
        });
      }

      // Let some tasks start
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    REQUIRE(counter.get() == 5);
  }

  SECTION("Shutdown drains queued tasks") {
    constexpr int kTasks = 100;
    stl::ThreadPool pool(2);
    Counter counter;
    std::vector<std::future<void>> futures;

    // Keep both workers busy so the rest stay queued
    for (int i = 0; i < 2; ++i) {
      futures.push_back(pool.submit_task([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
      }));
    }
    for (int i = 0; i < kTasks; ++i) {
      futures.push_back(pool.submit_task([&counter]() { counter.increment(); }));
    }

    pool.shutdown();

    REQUIRE(counter.get() == kTasks);
    for (auto& future : futures) {
      REQUIRE(future.wait_for(std::chrono::seconds(0)) ==
              std::future_status::ready);
      REQUIRE_NOTHROW(future.get());
    }
  }

  SECTION("Submit after shutdown is rejected") {
    stl::ThreadPool pool(2);
    pool.shutdown();

    auto future = pool.submit_task([]() { return 1; });
    REQUIRE_THROWS_AS(future.get(), std::runtime_error);
  }

  SECTION("Shutdown is idempotent") {
    stl::ThreadPool pool(2);
    pool.shutdown();
    REQUIRE_NOTHROW(pool.shutdown());
  }
}

// Lost wakeup on shutdown used to hang the destructor
TEST_CASE("ThreadPool create/destroy stress") {
  constexpr auto kTimeout = std::chrono::seconds(60);

  auto run_with_timeout = [&](auto body) {
    std::promise<void> done;
    auto finished = done.get_future();
    std::thread runner([body, done = std::move(done)]() mutable {
      try {
        body();
        done.set_value();
      } catch (...) {
        done.set_exception(std::current_exception());
      }
    });
    if (finished.wait_for(kTimeout) == std::future_status::timeout) {
      runner.detach();
      FAIL("ThreadPool create/destroy loop did not finish (shutdown hang)");
    }
    runner.join();
    finished.get();
  };

  SECTION("Empty pools") {
    run_with_timeout([]() {
      for (int i = 0; i < 2000; ++i) {
        stl::ThreadPool pool(4);
      }
    });
  }

  SECTION("Pools with queued tasks") {
    auto executed = std::make_shared<std::atomic<int>>(0);
    constexpr int kIterations = 500;
    constexpr int kTasksPerPool = 8;

    run_with_timeout([executed]() {
      for (int i = 0; i < kIterations; ++i) {
        stl::ThreadPool pool(4);
        for (int j = 0; j < kTasksPerPool; ++j) {
          pool.submit_task([executed]() { executed->fetch_add(1); });
        }
      }
    });

    REQUIRE(executed->load() == kIterations * kTasksPerPool);
  }

  SECTION("Explicit shutdown with queued tasks") {
    run_with_timeout([]() {
      for (int i = 0; i < 500; ++i) {
        stl::ThreadPool pool(4);
        std::vector<std::future<int>> futures;
        for (int j = 0; j < 8; ++j) {
          futures.push_back(pool.submit_task([j]() { return j; }));
        }
        pool.shutdown();
        for (int j = 0; j < 8; ++j) {
          if (futures[j].get() != j) {
            throw std::logic_error("wrong result after shutdown");
          }
        }
      }
    });
  }
}

TEST_CASE("ThreadPool perfect forwarding") {
  SECTION("Forward movable arguments") {
    stl::ThreadPool pool(2);

    auto task = [](std::unique_ptr<int> ptr) { return *ptr; };

    auto ptr = std::make_unique<int>(42);
    auto future = pool.submit_task(task, std::move(ptr));

    REQUIRE(future.get() == 42);
  }

  SECTION("Forward reference arguments") {
    stl::ThreadPool pool(2);
    Counter counter;

    auto task = [](Counter& c) { c.increment(); };

    auto future = pool.submit_task(task, std::ref(counter));
    future.wait();

    REQUIRE(counter.get() == 1);
  }
}

TEST_CASE("ThreadPool exception handling") {
  SECTION("Exception in task") {
    stl::ThreadPool pool(2);

    auto future = pool.submit_task(
        []() -> int { throw std::runtime_error("Test exception"); });

    REQUIRE_THROWS_AS(future.get(), std::runtime_error);
  }

  SECTION("Multiple tasks with exceptions") {
    stl::ThreadPool pool(2);
    std::vector<std::future<int>> futures;

    // Submit mix of normal and exception tasks
    for (int i = 0; i < 5; ++i) {
      if (i % 2 == 0) {
        futures.push_back(pool.submit_task([i]() { return i; }));
      } else {
        futures.push_back(pool.submit_task(
            [i]() -> int { throw std::runtime_error("Test"); }));
      }
    }

    // Check results
    for (int i = 0; i < 5; ++i) {
      if (i % 2 == 0) {
        REQUIRE(futures[i].get() == i);
      } else {
        REQUIRE_THROWS_AS(futures[i].get(), std::runtime_error);
      }
    }
  }
}
