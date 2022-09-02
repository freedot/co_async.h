// https://github.com/freedot/co_async.h
#pragma once
#include <coroutine>
#include <functional>

namespace co {
  template<typename T>
  using promise_cb_t = std::function<void(std::function<void(T&& v)>&& resolve_cb)>;

  template<typename T>
  auto promise(promise_cb_t<T>&& cb) {
    struct awaitable {
      bool await_ready() { return false; }
      void await_suspend(std::coroutine_handle<> resolve) {
        cb([this, resolve](T&& v) {
          ::new (static_cast<void*>(std::addressof(value)))
            T(std::forward<T>(v));
          value_inited = true;
          resolve.resume();
          });
      }
      T await_resume() { return std::move(value); }
      awaitable(promise_cb_t<T>&& cb) noexcept : cb(std::move(cb)), value_inited(false) {}
      ~awaitable() noexcept {
        if (std::exchange(value_inited, false)) {
          value.~T();
        }
      }
      awaitable(const awaitable&) = delete;
      awaitable& operator=(const awaitable&) = delete;
    private:
      T value;
      promise_cb_t<T> cb;
      bool value_inited;
    };
    return awaitable(std::move(cb));
  }

  template<typename T>
  struct async {
    struct awaitable_final;

    struct promise_type {
      std::coroutine_handle<> await_handle;
      bool done = false;

      enum class value_type { empty, value, exception };
      value_type type = value_type::empty;
      union {
        T value;
        std::exception_ptr exception;
      };
      async get_return_object() { return async(std::coroutine_handle<promise_type>::from_promise(*this), *this);}
      std::suspend_never initial_suspend() { return {}; }
      awaitable_final final_suspend() noexcept { return awaitable_final(*this); }
      template<std::convertible_to<T> From>
      void return_value(From&& from) {
        ::new (static_cast<void*>(std::addressof(value)))
          T(std::forward<From>(from));
        type = value_type::value;
      }
      void unhandled_exception() noexcept {
        ::new (static_cast<void*>(std::addressof(exception)))
          std::exception_ptr(std::current_exception());
        type = value_type::exception;
      }
      promise_type() noexcept {}
      ~promise_type() noexcept {
        if (type == value_type::value) {
          value.~T();
        }
        else if (type == value_type::exception) {
          exception.~exception_ptr();
        }
      }
      promise_type(const promise_type&) = delete;
      promise_type& operator=(const promise_type&) = delete;
    };

    std::coroutine_handle<> handle;
    promise_type& promise;

    struct awaitable_value {
      async<T>* a;
      bool await_ready() {
        return false;
      }
      void await_suspend(std::coroutine_handle<> h) {
        if (!a->promise.done) {
          a->promise.await_handle = h;
        }
        else {
          h.resume();
        }
      }
      T await_resume() {
        auto r = std::move(a->result());
        a->destroy();
        return r;
      }
      explicit awaitable_value(async<T>* a) noexcept : a(a) {}
      awaitable_value(const awaitable_value&) = delete;
      awaitable_value& operator=(const awaitable_value&) = delete;
    };

    struct awaitable_final {
      promise_type& promise;
      bool await_ready() const noexcept { return false; }
      void await_suspend(std::coroutine_handle<> h) noexcept {
        promise.done = true;
        if (promise.await_handle) {
          promise.await_handle.resume();
        }
      }
      void await_resume() noexcept {}
      explicit awaitable_final(promise_type& promise) noexcept : promise(promise) {}
      awaitable_final(const awaitable_final&) = delete;
      awaitable_final& operator=(const awaitable_final&) = delete;
    };

    auto operator co_await() {
      return awaitable_value(this);
    }

    T await(std::function<bool()> update) {
      while (!promise.done && update());
      auto r = std::move(result());
      destroy();
      return r;
    }

    explicit async(std::coroutine_handle<> handle, promise_type& promise) noexcept : handle(handle), promise(promise) {}
    async(const async&) = delete;
    async& operator=(const async&) = delete;

  private:
    T& result() {
      if (promise.type == promise_type::value_type::value) {
        return promise.value;
      }
      else if (promise.type == promise_type::value_type::exception) {
        auto e = std::move(promise.exception);
        destroy();
        std::rethrow_exception(e);
      }
      else {
        destroy();
        throw std::exception("co::async return value is empty!");
      }
    }

    void destroy() {
      handle.destroy();
    }
  };
}
