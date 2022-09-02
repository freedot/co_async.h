# co_async.h
单文件简易的c++20协程库


// 示例一

    #include "co_async.h"
    #include <iostream>

    std::function<void(int)> resolve_cb;

    co::async<int> get_id() {
      // 等待一个承若协程
      int r = co_await co::promise<int>([](auto resolve_cb) {
        ::resolve_cb = std::move(resolve_cb);
        });
      co_return r;
    }

    co::async<std::string> fun() {
      int id = co_await get_id();
      co_return std::format("id: {}", id);
    }

    int main() {
      auto result = fun().await([]() {

        // 当所有协程被挂起后，会执行该update函数，该函数在主线程中执行
        // 当该update返回false，则会强制结束await等待，有可能抛出异常
        // 在这里可以做恢复协程操作

        resolve_cb(1); // 恢复协程, id值为1

        return true;

        });

      std::cout << result << std::endl;

      return 0;
    }


// 示例二

    #include "co_async.h"
    #include <iostream>

    co::async<int> fun() {
      std::cout << "call fun" << std::endl;
      co_return 0;
    }

    int main() {
      // 自有的循环
      for (int i = 0; i < 10; i++) {

        fun();

        // do something
      }

      return 0;
    }
