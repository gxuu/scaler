#pragma once

#include <string>

#include "zmq_config.h"

struct scheduler_config {
  std::string event_loop;
  int         io_threads_count;
  zmq_config  address;
  int         max_number_of_tasks_waiting;
  int         client_timeout_seconds;
  int         worker_timeout_seconds;
  int         object_retention_seconds;
  int         load_balance_seconds;
  int         load_balance_trigger_times;
  bool        protected_mode;
};
