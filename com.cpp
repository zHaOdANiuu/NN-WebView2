#pragma once

#include <type_traits>
#include "stdafx.hpp"
#include "debug.hpp"

#if defined(_X86_) || defined(_AMD64_)
#  define unknown_barrier_after_interlock()
#elif defined(_ARM_)
#  define unknown_barrier_after_interlock() __dmb(_ARM_BARRIER_ISH)
#elif defined(_ARM64_)
#  define unknown_barrier_after_interlock() __dmb(_ARM64_BARRIER_ISH)
#endif

namespace com {
template <class T>
class BaseClass : public T {
protected:
  BaseClass() : refcount_(1) {}
  BaseClass(BaseClass&&) = delete;
  virtual ~BaseClass() {
    refcount_ = -(LONG_MAX / 2);
  }

public:
  auto QueryInterface(REFIID riid, _Outptr_result_nullonfailure_ void** ppv) -> HRESULT override {
    *ppv = nullptr;
    if (InlineIsEqualGUID(riid, IID_IUnknown)) {
      *ppv = this;
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  auto AddRef() -> ULONG override {
    auto val = refcount_;
    while (
      val != LONG_MAX &&
      (InterlockedCompareExchange(&refcount_, val + 1, val) != val)
    )
      val = refcount_;
    return val != LONG_MAX ? val + 1 : LONG_MAX;
  }
  auto Release() -> ULONG override {
    auto val = refcount_;
    while (val != LONG_MAX && (InterlockedCompareExchange(&refcount_, val - 1, val) != val))
      val = refcount_;
    auto ref = val - 1;
    if (ref == 0) {
      unknown_barrier_after_interlock();
      delete this;
    }
    return ref;
  }

private:
  long refcount_;
};

template <class T>
class Ptr {
private:
  T* ptr_;

public:
  Ptr() : ptr_(nullptr) {}
  Ptr(T* ptr) : ptr_(ptr) {
    if (ptr_) [[likely]]
      ptr_->AddRef();
  }
  Ptr(const Ptr& other) : Ptr(other.get()) {}
  template <class U> Ptr(const Ptr<U>& other) : Ptr(static_cast<T*>(other.get())) {}
  Ptr(Ptr&& other) : Ptr(other.detach()) {}
  template <class U> Ptr(Ptr<U>&& other) noexcept : Ptr(static_cast<T*>(other.detach())) {}
  ~Ptr() {
    if (ptr_) [[likely]]
      ptr_->Release();
  }
  auto operator=(T* other) {
    auto ptr = ptr_;
    ptr_     = other;
    if (ptr_) [[likely]]
      ptr_->AddRef();
    if (ptr) [[likely]]
      ptr->Release();
    return *this;
  }
  auto operator=(const Ptr& other) {
    return operator=(other.get());
  }
  template <class U> auto operator=(const Ptr<U>& other) {
    return operator=(static_cast<T*>(other.get()));
  }
  auto operator=(Ptr&& other) {
    attach(other.detach());
    return *this;
  }
  template <class U> auto operator=(Ptr<U>&& other) noexcept {
    attach(other.detach());
    return *this;
  }

  auto operator&() {
    auto tmp = ptr_;
    ptr_     = nullptr;
    if (tmp)
      tmp->Release();
    return &ptr_;
  }
  [[nodiscard]] explicit operator bool() const {
    return ptr_ != nullptr;
  }
  [[nodiscard]] auto operator->() const {
    return ptr_;
  }
  [[nodiscard]] auto operator*() const {
    return *ptr_;
  }

  [[nodiscard]] auto get() const {
    return ptr_;
  }
  [[nodiscard]] auto detach() {
    auto tmp = ptr_;
    ptr_     = nullptr;
    return tmp;
  }
  auto attach(T* other) {
    auto tmp = ptr_;
    ptr_     = other;
    if (tmp) [[likely]] {
      ULONG ref = tmp->Release();
      DEBUG_ASSERT(((other != tmp) || (ref > 0)), "Self-attach with ref==0 would cause double delete");
    }
  }
};

template <class T, typename... Args>
  requires(!std::is_abstract_v<T>)
auto ptrMake(Args&&... args) {
  auto ret    = Ptr<T>{};
  auto buffer = (char*)operator new(sizeof(T), static_cast<std::align_val_t>(alignof(T)), ::std::nothrow);
  if (buffer != nullptr)
    ret.attach(new (buffer) T(std::forward<Args>(args)...));
  buffer = nullptr;
  return ret;
};
}
