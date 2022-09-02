#include "co_async.h"
#include <iostream>
#include <thread>
#include <chrono>
using namespace std::chrono_literals;


// 一个时钟管理类(示例辅助类)
class timers {
public:
  void update() {
    for (auto it = nodes.begin(); it != nodes.end(); ) {
      if (std::time(nullptr) >= it->end_time) {
        it->cb(static_cast<int>(it->end_time));
        it = nodes.erase(it);
      }
      else ++it;
    }
  }
  void add_timer(int delaysec, std::function<void(int sec)>&& end_cb) {
    nodes.emplace_back(timer_node{ std::time(nullptr) + delaysec, std::move(end_cb) });
  }
private:
  struct timer_node {
    time_t end_time;
    std::function<void(int sec)> cb;
  };
  std::list<timer_node> nodes;
};
timers g_timers;


co::async<int> wait_sec(int sec) {
  co_return co_await co::promise<int>([sec](auto resolve_cb) {
    g_timers.add_timer(sec, [resolve_cb](auto sec) { resolve_cb(std::move(sec)); });
    });
}

co::async<int> fun(int id) {

  std::cout << id << " wait start, time:" << std::time(nullptr) << std::endl;
  co_await wait_sec(5); // 等待5秒
  std::cout << id << " wait end, time:" << std::time(nullptr) << std::endl;

  co_return 0;
}

int main() {

  for (int i = 0; i < 10; i++) {
    fun(i);
    g_timers.update();
    std::this_thread::sleep_for(1s);
  }

  return 0;
}
