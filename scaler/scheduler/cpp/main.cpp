

#include <unistd.h>

#include <coroutine>
#include <cstdio>
#include <iostream>
#include <string>

#include "address_monitor.h"
#include "async_binder.h"
#include "async_connector.h"
#include "client_manager.h"
#include "context.h"
#include "graphtask_manager.h"
#include "scheduler.h"
#include "status_reporter.h"
#include "task_manager.h"
#include "worker_manager.h"

int main() {
  scheduler sch;
  auto      loop = sch.get_loops();
  while(true) {
    loop.resume();
    // sleep(1);
  }
}
