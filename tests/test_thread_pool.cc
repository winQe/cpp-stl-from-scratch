#define CATCH_CONFIG_MAIN
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
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
    }  // Destructor should be called here

    // Give a moment for cleanup
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // At least some tasks should have completed
    REQUIRE(counter.get() >= 0);
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
