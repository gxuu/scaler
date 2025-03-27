

#include <unistd.h>

#include <coroutine>
#include <cstdio>
#include <iostream>
#include <string>

#include "address_monitor.h"
#include "async_binder.h"
#include "async_connector.h"

import scheduler;

int main() {
  scheduler sch;
  auto      loop = sch.get_loops();
  while(true) {
    loop.resume();
    // sleep(1);
  }
}
