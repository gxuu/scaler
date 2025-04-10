#pragma once

#include <string>

enum class zmq_message_t {
  INPROC,
  IPC,
  TCP,
};

struct zmq_config {
  zmq_message_t message_type;
  std::string   host;
  int           port;

  std::string to_address() { return "tcp://" + host + ":" + std::to_string(port); }
};
