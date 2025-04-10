

#include "graphtask_manager.h"

void graphtask_manager::on_graph_task(zmq::message_t& source, capnp::ReaderFor<Message> message,
                                      kj::Array<capnp::word> raw) {
  // Need to do two things, first copy source out, then copy message out (which is raw)
  bytes client((unsigned char*)source.data(), (unsigned char*)source.data() + source.size());
  bytes graphtask = bytes(raw.asBytes().begin(), raw.asBytes().end());
  _unassigned.emplace(std::move(client), std::move(graphtask));
}

void graphtask_manager::on_graph_task_cancel(zmq::message_t& source, capnp::ReaderFor<Message> message) {
  // TODO: More work
}

void graphtask_manager::routine() {
  if(_unassigned.empty()) return;
  auto [client, graphtask] = _unassigned.front();
  _unassigned.pop();

  add_new_graph(std::move(client), std::move(graphtask));
}

// void graphtask_manager::add_new_graph(bytes client, bytes graphtask) {}
