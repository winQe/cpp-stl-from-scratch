
# Rebuilding the STL in Modern C++

Reimplementing core STL components in modern C++20 to explore memory management, templates, and low-level systems design.

The goal is to learn some modern C++ features and idioms such as:
- Placement new and manual memory management
- Custom allocators and memory pools
- Perfect forwarding and move semantics
- Iterator traits and concepts
- RAII and exception safety
- A little bit of template metaprogramming

---

## 📦 What I'm Building

| Component        | Status      | Key Features                                              |
|------------------|-------------|-----------------------------------------------------------|
| `Vector`         | ✅ Done     | Manual memory management, iterators, dynamic array       |
| `HashMap`        | 🧠 Planned  | Open addressing, probing, custom hash                    |
| `UniquePtr`      | ✅ Done     | Move-only semantics, custom deleters                     |
| `SharedPtr`      | 🧠 Planned  | Reference counting, weak references, thread safety       |
| `String`         | 🧠 Planned  | Small string optimization (SSO), move semantics          |
| `LockFreeQueue`  | ✅ Done     | MPMC Vyukov Queue, memory ordering                       |
| `ThreadPool`     | ✅ Done     | jthread, future/promise, packaged_task, condvars         |
| `MemoryPool`     | 🧠 Planned  | Pool allocator, fixed-size blocks, fast allocation       |

---
## 🚀 Features & Techniques

### Memory Management
- **Memory Pool**: Fixed-size block allocator for fast allocation/deallocation
- **Custom Deleters**: Flexible resource cleanup for UniquePtr
- **Reference Counting**: Atomic operations for thread-safe SharedPtr
- **Small String Optimization**: Stack allocation for short strings

### Modern C++ Features
- **Move Semantics**: Efficient resource transfer, especially for UniquePtr
- **Perfect Forwarding**: Optimal parameter passing in smart pointers
- **Atomic Operations**: Lock-free data structures and thread safety
- **Template Metaprogramming**: Type traits for smart pointer constraints

### Concurrency & Performance
- **Lock-Free Programming**: Wait-free queue implementation with memory ordering
- **Thread Pool**: task queues, future/promise, jthreads, condvard
- **Memory Ordering**: Sequential consistency, acquire-release semantics
- **Cache-Friendly Design**: Minimize false sharing, optimize memory layout
---

## 🔧 Building & Running

```bash
git clone https://github.com/winqe/cpp-stl-from-scratch.git
cd cpp-stl-from-scratch
mkdir build && cd build
cmake ..
make
