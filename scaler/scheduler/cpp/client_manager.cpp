#include "client_manager.h"

#include "worker_manager.h"

client_manager::client_manager(int client_timeout_seconds, int protect)
    : _client_timeout_seconds(client_timeout_seconds), _protect(protect) {}

void client_manager::build_channel(std::shared_ptr<async_binder>    binder,
                                   std::shared_ptr<async_connector> binder_monitor,
                                   std::shared_ptr<object_manager>  object_manager,
                                   std::shared_ptr<task_manager>    task_manager,
                                   std::shared_ptr<worker_manager>  worker_manager) {
  _binder         = binder;
  _binder_monitor = binder_monitor;
  _object_manager = object_manager;
  _task_manager   = task_manager;
  _worker_manager = worker_manager;
}

bool client_manager::has_client_id(capnp::ReaderFor<Message> message) { return true; }

void client_manager::on_heartbeat(zmq::message_t source, capnp::ReaderFor<Message> message) {
  ::capnp::MallocMessageBuilder msg;
  Message::Builder              this_message = msg.initRoot<Message>();
  this_message.initClientHeartbeatEcho();
  this_message.setClientHeartbeatEcho(this_message.getClientHeartbeatEcho());
  _binder->send(std::move(source), &msg);
}

void client_manager::on_client_disconnect(zmq::message_t& source, capnp::ReaderFor<Message> message) {
  if(message.getClientDisconnect().getDisconnectType() == ClientDisconnect::DisconnectType::DISCONNECT) {
    // TODO: Not implemented
    __on_client_disconnect(source);
  }

  ::capnp::MallocMessageBuilder msg;
  Message::Builder              this_message      = msg.initRoot<Message>();
  auto                          shutdown_response = this_message.initClientShutdownResponse();
  shutdown_response.setAccepted(true);
  _worker_manager->on_client_shutdown(source);
  _binder->send(std::move(source), &msg);
}

void client_manager::__on_client_disconnect(zmq::message_t& client_id) {}

void client_manager::on_task_begin(zmq::message_t& client_id, capnp::Data::Reader task_id) {
  bytes client((unsigned char*)client_id.data(), (unsigned char*)client_id.data() + client_id.size());
  bytes task(task_id.asBytes().begin(), task_id.asBytes().end());
  _client_to_task_ids.add(client, task);
}

bytes client_manager::on_task_finish(bytes task_id) { return _client_to_task_ids.remove_value(task_id); }
