#pragma once

#include "check.hpp"

#include <memory>
#include <vector>
#include <mutex>

// Memory manager for fixed size allocations. It allocates objects of type T and with subsequent calls to alloc
// and dealloc reserves them or makes them available. Objects are initialized once (once created constructors are
// called; destructors are called once memory manager is deleted) so clients must do custom initialization after alloc.
template <typename T, typename Dtype, typename TContext>
class FixedMemoryManager
{
public:
  FixedMemoryManager(size_t memory_size, size_t elem_count, TContext context, bool no_new_alloc) :
    memory_size_(memory_size), context_(context), elem_count_(elem_count), no_new_alloc_(no_new_alloc)
  {
    CHECK(elem_count_ > 0);
    // Create initial memory.
    AllocNewBuffer();
    // Our first free object is first object in the first buffer.
    next_free_ = reinterpret_cast<Elem*>(buffers_.back().get());
  }

  // Releases all allocated objects.
  ~FixedMemoryManager()
  {
    // GO through the all objects and invoke theirs destructors.
    size_t elem_and_memory = sizeof(Elem) + memory_size_ * sizeof(Dtype);
    for (size_t ib = 0; ib < buffers_.size(); ib++)
    {
      char* curr_mem = buffers_[ib].get();
      for (size_t ie = 0; ie < elem_count_; ie++)
      {
        reinterpret_cast<Elem*>(curr_mem)->~Elem();
        curr_mem += elem_and_memory;
      }
    }
  }

  // Returns new object. Objects are recycled without constructr/destructor calls so clinets may want to do custom 
  // initialization after this call.
  T* Alloc()
  {
    // We may be called from multiple threads so synchronize access.
    alloc_lock_.lock();

    // We always must have free object here since we preallocate at the end if necessary.
    CHECK(next_free_ != nullptr);

    // Reallocate if necessary (and configured to do so).
    if (!no_new_alloc_ && next_free_->next_ == nullptr)
    {
      AllocNewBuffer();
      next_free_->next_ = reinterpret_cast<Elem*>(buffers_.back().get());
    }

    // Take object pointer.
    T* mem = static_cast<T*>(next_free_);

    // Move to the next free object.
    next_free_ = next_free_->next_;

    alloc_lock_.unlock();

    return mem;
  }

  // Returns object to the pool of available objects.
  void Dealloc(T* img)
  {
    // We may be called from multiple threads so synchronize access.
    alloc_lock_.lock();

    // This is out new next free, update accordingly.
    Elem* elem = static_cast<Elem*>(img);
    elem->next_ = next_free_;
    next_free_ = elem;

    alloc_lock_.unlock();
  }

private:
  // Helper method to allocate new buffer.
  void AllocNewBuffer()
  {
    // One element size is equal to size of the object + size of required memory.
    size_t elem_and_memory = sizeof(Elem) + memory_size_ * sizeof(Dtype);

    // Add new buffer.
    buffers_.emplace_back(new char[elem_and_memory * elem_count_]);

    // Go through objects in the buffer and initialize them with placement new.
    char* curr_mem = buffers_.back().get();
    for (size_t ie = 0; ie < elem_count_ - 1; ie++)
    {
      // We do not need to take returned pointer from new call (since we have object at our buffer). This call must be
      // performed to properly initialize objects.
      new (curr_mem)Elem(reinterpret_cast<Dtype*>(curr_mem + sizeof(Elem)), memory_size_, context_, reinterpret_cast<Elem*>(curr_mem + elem_and_memory));
      curr_mem += elem_and_memory;
    }
    // Last object point to nullptr (no next).
    new (curr_mem)Elem(reinterpret_cast<Dtype*>(curr_mem + sizeof(Elem)), memory_size_, context_, nullptr);
  }

  // Helper class that adds linked-list capability to the type of allocated objects.
  class Elem : public T
  {
  public:
    Elem(Dtype* mem, size_t mem_size, TContext& context, Elem* next) : T(mem, mem_size, context), next_(next) {}
    Elem* next_;
  };

  // Additional context to be passed to allocated objects.
  TContext context_;
  // Indicates if buffer allocation (beside the first one) is permitted.
  bool no_new_alloc_;
  // Size of the required memory per element.
  size_t memory_size_;
  // Number of elements per one memory buffer.
  size_t elem_count_;
  // Vector of allocated memory buffers.
  std::vector<std::unique_ptr<char[]>> buffers_;
  // Pointer to the next free element.
  Elem* next_free_;
  // Allocation synchronization lock.
  std::mutex alloc_lock_;
};
