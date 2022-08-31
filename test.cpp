#include "co_async.h"
#include <iostream>


std::function<void(int)> g_resolve_cb;

co::async<int> get_id() {
  co_return 1;
}

co::async<int> fun() {
  auto id = co_await get_id();

  auto w = co_await co::promise<int>([](auto resolve_cb) {
    g_resolve_cb = std::move(resolve_cb);
    });

  co_return id * 10 + w;
}

int main() {

  int result = fun().await([]() {
    g_resolve_cb(2);
    return true;
    });
  std::cout << "result: " << result << std::endl;

  return 0;
}

