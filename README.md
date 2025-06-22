
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

## 📦 What I'm building

| Component     | Status   | Key Features                                 |
|---------------|----------|----------------------------------------------|
| `Vector`      | 🚧 WIP    | Manual memory management, iterators, dynamic array |
| `HashMap`     | 🚧 WIP    | Open addressing, probing, custom hash        |
| `UniquePtr`   | 🧠 Planned| Move-only semantics, custom deleters|
| `SharedPtr`    | 🧠 Planned| Reference counting, weak references, thread safety     |

---

## 🔧 Building & Running

```bash
git clone https://github.com/winqe/cpp-stl-from-scratch.git
cd cpp-stl-from-scratch
mkdir build && cd build
cmake ..
make
