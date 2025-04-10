

#include <unistd.h>

#include <cstdio>

#include "config.h"
#include "scheduler.h"
#include "utility/defaults.h"
#include "zmq_config.h"

int main() {
  scheduler_config config{
      .event_loop                  = "builtin",
      .io_threads_count            = DEFAULT_IO_THREADS,
      .address                     = {.message_type = zmq_message_t::TCP, .host = "127.0.0.1", .port = 2345},
      .max_number_of_tasks_waiting = DEFAULT_MAX_NUMBER_OF_TASKS_WAITING,
      .client_timeout_seconds      = DEFAULT_CLIENT_TIMEOUT_SECONDS,
      .worker_timeout_seconds      = DEFAULT_WORKER_TIMEOUT_SECONDS,
      .object_retention_seconds    = DEFAULT_OBJECT_RETENTION_SECONDS,
      .load_balance_seconds        = DEFAULT_LOAD_BALANCE_SECONDS,
      .load_balance_trigger_times  = DEFAULT_LOAD_BALANCE_TRIGGER_TIMES,
      .protected_mode              = false,
  };

  scheduler sch(config);
  auto      loop = sch.get_loops();
  while(true) {
    loop.resume();
    // sleep(1);
  }
}
