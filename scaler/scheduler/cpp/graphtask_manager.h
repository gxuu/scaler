#pragma once

#include <capnp/common.h>

#include <queue>
#include <zmq.hpp>

#include "message.capnp.h"

class graphtask_manager {
  std::queue<std::pair<zmq::message_t, capnp::ReaderFor<Message>>> unassigned;

public:
  void on_graph_task(zmq::message_t& source, capnp::ReaderFor<Message> message) {
    // unassigned.emplace(source, message);
  }

  void on_graph_task_cancel(zmq::message_t& source, capnp::ReaderFor<Message> message) {
    // TODO: More work
  }
};
