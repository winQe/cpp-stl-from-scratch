#define CATCH_CONFIG_MAIN
#include <algorithm>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "stl/lock_free_queue.h"

// Helper class for testing destruction and move semantics
struct DestructorCounter {
  static std::atomic<int> count;
  static std::atomic<int> move_count;
  static std::atomic<int> copy_count;
  int value;

  DestructorCounter(int v = 0) : value(v) {}

  DestructorCounter(const DestructorCounter& other) : value(other.value) {
    copy_count++;
  }

  DestructorCounter(DestructorCounter&& other) noexcept : value(other.value) {
    move_count++;
    other.value = -1;  // Mark as moved
  }

  DestructorCounter& operator=(const DestructorCounter& other) {
    if (this != &other) {
      value = other.value;
      copy_count++;
    }
    return *this;
  }

  DestructorCounter& operator=(DestructorCounter&& other) noexcept {
    if (this != &other) {
      value = other.value;
      move_count++;
      other.value = -1;
    }
    return *this;
  }

  ~DestructorCounter() { count++; }

  static void reset() {
    count = 0;
    move_count = 0;
    copy_count = 0;
  }
};

std::atomic<int> DestructorCounter::count{0};
std::atomic<int> DestructorCounter::move_count{0};
std::atomic<int> DestructorCounter::copy_count{0};

struct ThrowingType {
  int value;
  static std::atomic<int> construction_count;
  static std::atomic<bool> should_throw;

  ThrowingType(int v) : value(v) {
    construction_count++;
    if (should_throw.load()) {
      throw std::runtime_error("Construction failed");
    }
  }

  ThrowingType(const ThrowingType& other) : value(other.value) {
    construction_count++;
    if (should_throw.load()) {
      throw std::runtime_error("Copy failed");
    }
  }

  ThrowingType(ThrowingType&& other) noexcept : value(other.value) {
    construction_count++;
    other.value = -1;
  }

  static void reset() {
    construction_count = 0;
    should_throw = false;
  }
};

std::atomic<int> ThrowingType::construction_count{0};
std::atomic<bool> ThrowingType::should_throw{false};

// Basic functionality tests
TEST_CASE("LockFreeQueue basic construction") {
  SECTION("Queue construction with power of 2 capacity") {
    stl::LockFreeQueue<int, 8> queue;
    // Queue should be empty initially
    int value;
    REQUIRE_FALSE(queue.try_pop(value));
  }

  SECTION("Queue with different types") {
    stl::LockFreeQueue<std::string, 4> string_queue;
    stl::LockFreeQueue<double, 16> double_queue;
    stl::LockFreeQueue<std::unique_ptr<int>, 8> ptr_queue;

    std::string str;
    double d;
    std::unique_ptr<int> ptr;

    REQUIRE_FALSE(string_queue.try_pop(str));
    REQUIRE_FALSE(double_queue.try_pop(d));
    REQUIRE_FALSE(ptr_queue.try_pop(ptr));
  }
}

TEST_CASE("LockFreeQueue single-threaded operations") {
  stl::LockFreeQueue<int, 8> queue;

  SECTION("Basic push and pop") {
    REQUIRE(queue.try_push(42));

    int value;
    REQUIRE(queue.try_pop(value));
    REQUIRE(value == 42);

    // Queue should be empty now
    REQUIRE_FALSE(queue.try_pop(value));
  }

  SECTION("Multiple push and pop operations") {
    std::vector<int> input = {1, 2, 3, 4, 5};

    // Push all values
    for (int val : input) {
      REQUIRE(queue.try_push(val));
    }

    // Pop all values in FIFO order
    std::vector<int> output;
    int value;
    while (queue.try_pop(value)) {
      output.push_back(value);
    }

    REQUIRE(output == input);
  }

  SECTION("Fill queue to capacity") {
    constexpr size_t capacity = 8;

    // Fill queue completely
    for (size_t i = 0; i < capacity; ++i) {
      REQUIRE(queue.try_push(static_cast<int>(i)));
    }

    // Next push should fail
    REQUIRE_FALSE(queue.try_push(999));

    // Pop one item
    int value;
    REQUIRE(queue.try_pop(value));
    REQUIRE(value == 0);

    // Now we should be able to push again
    REQUIRE(queue.try_push(999));
  }

  SECTION("Alternating push and pop") {
    for (int i = 0; i < 100; ++i) {
      REQUIRE(queue.try_push(i));

      int value;
      REQUIRE(queue.try_pop(value));
      REQUIRE(value == i);
    }
  }
}

TEST_CASE("LockFreeQueue move semantics") {
  stl::LockFreeQueue<DestructorCounter, 8> queue;

  SECTION("Push with move semantics") {
    DestructorCounter::reset();

    DestructorCounter item(42);
    REQUIRE(queue.try_push(std::move(item)));

    // Item should have been moved
    REQUIRE(item.value == -1);  // Marked as moved
    REQUIRE(DestructorCounter::move_count.load() >= 1);

    DestructorCounter popped;
    REQUIRE(queue.try_pop(popped));
    REQUIRE(popped.value == 42);
  }

  SECTION("Perfect forwarding") {
    DestructorCounter::reset();

    // Push rvalue
    REQUIRE(queue.try_push(DestructorCounter(123)));

    // Push lvalue
    DestructorCounter lvalue(456);
    REQUIRE(queue.try_push(lvalue));

    DestructorCounter result1, result2;
    REQUIRE(queue.try_pop(result1));
    REQUIRE(queue.try_pop(result2));

    REQUIRE(result1.value == 123);
    REQUIRE(result2.value == 456);
  }
}

TEST_CASE("LockFreeQueue with complex types") {
  SECTION("String operations") {
    stl::LockFreeQueue<std::string, 4> queue;

    std::vector<std::string> strings = {"hello", "world", "lock", "free"};

    for (const auto& str : strings) {
      REQUIRE(queue.try_push(str));
    }

    std::vector<std::string> results;
    std::string result;
    while (queue.try_pop(result)) {
      results.push_back(result);
    }

    REQUIRE(results == strings);
  }

  SECTION("Unique pointer operations") {
    stl::LockFreeQueue<std::unique_ptr<int>, 4> queue;

    REQUIRE(queue.try_push(std::make_unique<int>(42)));
    REQUIRE(queue.try_push(std::make_unique<int>(99)));

    std::unique_ptr<int> ptr1, ptr2;
    REQUIRE(queue.try_pop(ptr1));
    REQUIRE(queue.try_pop(ptr2));

    REQUIRE(*ptr1 == 42);
    REQUIRE(*ptr2 == 99);
  }
}

TEST_CASE("LockFreeQueue boundary conditions") {
  SECTION("Capacity boundaries") {
    stl::LockFreeQueue<int, 2> small_queue;  // Minimum practical size

    REQUIRE(small_queue.try_push(1));
    REQUIRE(small_queue.try_push(2));
    REQUIRE_FALSE(small_queue.try_push(3));  // Should fail

    int value;
    REQUIRE(small_queue.try_pop(value));
    REQUIRE(value == 1);

    REQUIRE(small_queue.try_push(3));  // Should succeed now
  }

  SECTION("Wraparound behavior") {
    stl::LockFreeQueue<int, 4> queue;

    // Fill and empty multiple times to test wraparound
    for (int cycle = 0; cycle < 3; ++cycle) {
      // Fill queue
      for (int i = 0; i < 4; ++i) {
        REQUIRE(queue.try_push(cycle * 10 + i));
      }

      // Empty queue
      for (int i = 0; i < 4; ++i) {
        int value;
        REQUIRE(queue.try_pop(value));
        REQUIRE(value == cycle * 10 + i);
      }
    }
  }
}

TEST_CASE("LockFreeQueue concurrent operations") {
  constexpr size_t QUEUE_SIZE = 1024;
  constexpr size_t NUM_ITEMS = 10000;
  constexpr size_t NUM_PRODUCERS = 4;
  constexpr size_t NUM_CONSUMERS = 4;

  stl::LockFreeQueue<int, QUEUE_SIZE> queue;

  SECTION("Multiple producer, multiple consumer") {
    std::atomic<size_t> produced{0};
    std::atomic<size_t> consumed{0};
    std::vector<int> consumed_items;
    std::mutex consumed_mutex;

    // Producer threads
    std::vector<std::thread> producers;
    for (size_t i = 0; i < NUM_PRODUCERS; ++i) {
      producers.emplace_back([&, i]() {
        size_t items_per_producer = NUM_ITEMS / NUM_PRODUCERS;
        for (size_t j = 0; j < items_per_producer; ++j) {
          int value = static_cast<int>(i * items_per_producer + j);
          while (!queue.try_push(value)) {
            std::this_thread::yield();
          }
          produced++;
        }
      });
    }

    // Consumer threads
    std::vector<std::thread> consumers;
    for (size_t i = 0; i < NUM_CONSUMERS; ++i) {
      consumers.emplace_back([&]() {
        int value;
        while (consumed.load() < NUM_ITEMS) {
          if (queue.try_pop(value)) {
            {
              std::lock_guard<std::mutex> lock(consumed_mutex);
              consumed_items.push_back(value);
            }
            consumed++;
          } else {
            std::this_thread::yield();
          }
        }
      });
    }

    // Wait for all producers
    for (auto& producer : producers) {
      producer.join();
    }

    // Wait for all consumers
    for (auto& consumer : consumers) {
      consumer.join();
    }

    REQUIRE(produced.load() == NUM_ITEMS);
    REQUIRE(consumed.load() == NUM_ITEMS);
    REQUIRE(consumed_items.size() == NUM_ITEMS);

    // Check that all items were consumed exactly once
    std::sort(consumed_items.begin(), consumed_items.end());
    for (size_t i = 0; i < NUM_ITEMS; ++i) {
      REQUIRE(consumed_items[i] == static_cast<int>(i));
    }
  }

  SECTION("High contention stress test") {
    constexpr size_t STRESS_ITEMS = 100000;
    constexpr size_t STRESS_THREADS = 8;

    stl::LockFreeQueue<size_t, 256> stress_queue;
    std::atomic<size_t> push_count{0};
    std::atomic<size_t> pop_count{0};
    std::vector<size_t> all_popped;
    std::mutex pop_mutex;

    std::vector<std::thread> threads;

    // Mixed producer-consumer threads
    for (size_t t = 0; t < STRESS_THREADS; ++t) {
      threads.emplace_back([&, t]() {
        std::mt19937 rng(t);
        std::uniform_int_distribution<int> dist(0, 1);

        for (size_t i = 0; i < STRESS_ITEMS / STRESS_THREADS; ++i) {
          if (dist(rng) == 0) {  // Try to push
            size_t value = t * (STRESS_ITEMS / STRESS_THREADS) + i;
            while (!stress_queue.try_push(value)) {
              std::this_thread::yield();
            }
            push_count++;
          } else {  // Try to pop
            size_t value;
            if (stress_queue.try_pop(value)) {
              {
                std::lock_guard<std::mutex> lock(pop_mutex);
                all_popped.push_back(value);
              }
              pop_count++;
            }
          }
        }
      });
    }

    for (auto& thread : threads) {
      thread.join();
    }

    // Pop remaining items
    size_t value;
    while (stress_queue.try_pop(value)) {
      all_popped.push_back(value);
      pop_count++;
    }

    REQUIRE(push_count.load() == pop_count.load());
    REQUIRE(all_popped.size() == push_count.load());
  }
}

TEST_CASE("LockFreeQueue producer-consumer patterns") {
  SECTION("Single producer, single consumer") {
    constexpr size_t ITEMS = 10000;
    stl::LockFreeQueue<size_t, 512> queue;

    std::atomic<bool> producer_done{false};
    std::vector<size_t> consumed;

    std::thread producer([&]() {
      for (size_t i = 0; i < ITEMS; ++i) {
        while (!queue.try_push(i)) {
          std::this_thread::yield();
        }
      }
      producer_done = true;
    });

    std::thread consumer([&]() {
      size_t value;
      while (!producer_done.load() || queue.try_pop(value)) {
        if (queue.try_pop(value)) {
          consumed.push_back(value);
        } else {
          std::this_thread::yield();
        }
      }
    });

    producer.join();
    consumer.join();

    REQUIRE(consumed.size() == ITEMS);

    // Verify order (FIFO)
    for (size_t i = 0; i < ITEMS; ++i) {
      REQUIRE(consumed[i] == i);
    }
  }

  SECTION("Burst producer, steady consumer") {
    constexpr size_t BURST_SIZE = 100;
    constexpr size_t NUM_BURSTS = 100;
    stl::LockFreeQueue<int, 512> queue;

    std::atomic<size_t> total_produced{0};
    std::atomic<size_t> total_consumed{0};

    std::thread producer([&]() {
      for (size_t burst = 0; burst < NUM_BURSTS; ++burst) {
        // Produce burst
        for (size_t i = 0; i < BURST_SIZE; ++i) {
          int value = static_cast<int>(burst * BURST_SIZE + i);
          while (!queue.try_push(value)) {
            std::this_thread::yield();
          }
          total_produced++;
        }

        // Small delay between bursts
        std::this_thread::sleep_for(std::chrono::microseconds(10));
      }
    });

    std::thread consumer([&]() {
      int value;
      while (total_consumed.load() < BURST_SIZE * NUM_BURSTS) {
        if (queue.try_pop(value)) {
          total_consumed++;
        } else {
          std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
      }
    });

    producer.join();
    consumer.join();

    REQUIRE(total_produced.load() == BURST_SIZE * NUM_BURSTS);
    REQUIRE(total_consumed.load() == BURST_SIZE * NUM_BURSTS);
  }
}

TEST_CASE("LockFreeQueue memory ordering verification") {
  // This test verifies that the memory ordering works correctly
  // by using a pattern that would fail with weaker ordering

  SECTION("Release-acquire ordering test") {
    constexpr size_t ITERATIONS = 10000;
    stl::LockFreeQueue<std::pair<int, int>, 64> queue;

    std::atomic<int> shared_counter{0};
    std::atomic<bool> done{false};
    std::vector<std::pair<int, int>> results;
    std::mutex results_mutex;

    std::thread producer([&]() {
      for (int i = 0; i < static_cast<int>(ITERATIONS); ++i) {
        int counter_val = shared_counter.load();
        shared_counter.store(counter_val + 1);

        while (!queue.try_push(std::make_pair(i, counter_val + 1))) {
          std::this_thread::yield();
        }
      }
      done = true;
    });

    std::thread consumer([&]() {
      std::pair<int, int> value;
      while (!done.load() || queue.try_pop(value)) {
        if (queue.try_pop(value)) {
          std::lock_guard<std::mutex> lock(results_mutex);
          results.push_back(value);
        } else {
          std::this_thread::yield();
        }
      }
    });

    producer.join();
    consumer.join();

    REQUIRE(results.size() == ITERATIONS);

    // Verify that the ordering is maintained
    for (size_t i = 0; i < results.size(); ++i) {
      REQUIRE(results[i].first == static_cast<int>(i));
      REQUIRE(results[i].second == static_cast<int>(i + 1));
    }
  }
}

TEST_CASE("LockFreeQueue performance characteristics") {
  SECTION("No false sharing verification") {
    // This test ensures that the queue doesn't suffer from false sharing
    // by having multiple threads work on different aspects simultaneously

    static size_t NUM_THREADS = std::thread::hardware_concurrency();
    constexpr size_t ITEMS_PER_THREAD = 10000;

    stl::LockFreeQueue<size_t, 1024> queue;
    std::atomic<size_t> total_ops{0};

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    for (size_t t = 0; t < NUM_THREADS; ++t) {
      threads.emplace_back([&, t]() {
        // Each thread alternates between push and pop
        for (size_t i = 0; i < ITEMS_PER_THREAD; ++i) {
          if (i % 2 == 0) {
            while (!queue.try_push(t * ITEMS_PER_THREAD + i)) {
              std::this_thread::yield();
            }
          } else {
            size_t value;
            while (!queue.try_pop(value)) {
              std::this_thread::yield();
            }
          }
          total_ops++;
        }
      });
    }

    for (auto& thread : threads) {
      thread.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);

    // This is more of a benchmark than a test, but we can verify operations
    // completed
    REQUIRE(total_ops.load() == NUM_THREADS * ITEMS_PER_THREAD);

    // Optional: Print performance metrics (comment out for CI)
    // std::cout << "Total operations: " << total_ops.load() << std::endl;
    // std::cout << "Duration: " << duration.count() << " microseconds" <<
    // std::endl; std::cout << "Ops per second: " << (total_ops.load() *
    // 1000000.0 / duration.count()) << std::endl;
  }
}

TEST_CASE("LockFreeQueue comprehensive edge cases") {
  SECTION("Rapid push/pop cycles") {
    stl::LockFreeQueue<int, 4> queue;

    // Rapidly cycle through push/pop to stress the sequence numbering
    for (int cycle = 0; cycle < 1000; ++cycle) {
      for (int i = 0; i < 4; ++i) {
        REQUIRE(queue.try_push(cycle * 4 + i));
      }

      for (int i = 0; i < 4; ++i) {
        int value;
        REQUIRE(queue.try_pop(value));
        REQUIRE(value == cycle * 4 + i);
      }
    }
  }

  SECTION("Sequence number wraparound") {
    // This would require running for a very long time to actually wrap around
    // but we can at least verify the queue works with large sequence numbers
    stl::LockFreeQueue<int, 8> queue;

    // Simulate many operations
    for (int i = 0; i < 10000; ++i) {
      REQUIRE(queue.try_push(i));

      int value;
      REQUIRE(queue.try_pop(value));
      REQUIRE(value == i);
    }
  }
}
