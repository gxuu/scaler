#pragma once

#include <coroutine>
#include <functional>

#include "corotask.h"

CoroTask create_corotine(std::function<void()> routine) {
  while(true) {
    routine();
    // awaitable
    co_await std::suspend_always{};
  }
}
