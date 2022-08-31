#include <coroutine>
#include <functional>

namespace co {
  template<typename T>
  using promise_cb_t = std::function<void(std::function<void(T&& v)>&& resolve_cb)>;

  template<typename T>
  auto promise(promise_cb_t<T>&& cb) {
    struct awaitable {
      T value;
      promise_cb_t<T> cb;
      bool await_ready() { return false; }
      void await_suspend(std::coroutine_handle<> resolve) {
        cb([this, resolve](T&& v) {
          value = std::move(v);
          resolve.resume();
          });
      }
      T await_resume() { return std::move(value); }
      awaitable(promise_cb_t<T>&& cb) noexcept : cb(std::move(cb)) {}
      ~awaitable() noexcept {}
    };
    return awaitable(std::move(cb));
  }

  template<typename T>
  struct async {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    struct awaitable_final;

    struct promise_type {
      bool done = false;

      enum class value_type { empty, value, exception };
      value_type type = value_type::empty;
      union {
        T value;
        std::exception_ptr exception;
      };
      async get_return_object() {
        return async(handle_type::from_promise(*this), *this);
      }
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
      promise_type(promise_type&&) = delete;
      promise_type& operator= (const promise_type&) = delete;
      promise_type& operator= (promise_type&&) = delete;
    };

    handle_type handle;
    promise_type& promise;
    bool destroying;

    struct awaitable_value {
      async<T>* a;
      bool await_ready() {
        return false;
      }
      auto await_suspend(handle_type h) {
        return h;
      }
      T await_resume() {
        auto r = std::move(a->result());
        a->destroy();
        return r;
      }
      explicit awaitable_value(async<T>* a) noexcept : a(a) {}
      awaitable_value(const awaitable_value&) = delete;
      awaitable_value(awaitable_value&&) = delete;
      awaitable_value& operator= (const awaitable_value&) = delete;
      awaitable_value& operator= (awaitable_value&&) = delete;
    };

    struct awaitable_final {
      promise_type& promise;
      bool await_ready() const noexcept { return false; }
      void await_suspend(handle_type h) noexcept {
        promise.done = true;
      }
      void await_resume() noexcept {}
      explicit awaitable_final(promise_type& promise) noexcept : promise(promise) {}
      awaitable_final(const awaitable_final&) = delete;
      awaitable_final(awaitable_final&&) = delete;
      awaitable_final& operator= (const awaitable_final&) = delete;
      awaitable_final& operator= (awaitable_final&&) = delete;
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

    explicit async(handle_type handle, promise_type& promise) noexcept : handle(handle), promise(promise), destroying(false) {}
    ~async() { destroy(); }
    async() = delete;
    async(const async&) = delete;
    async& operator= (const async&) = delete;
    async& operator= (async&&) = delete;

  private:
    T& result()& {
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
      if (!destroying) {
        destroying = true;
        handle.destroy();
      }
    }
  };
}
