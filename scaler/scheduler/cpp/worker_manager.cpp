
#include "worker_manager.h"

worker_manager::worker_manager(int timeout_seconds, int load_balance_seconds, int load_balance_trigger_times)
    : _timeout_seconds(timeout_seconds),
      _load_balance_seconds(load_balance_seconds),
      _load_balance_trigger_times(load_balance_trigger_times) {}

void worker_manager::build_channel(std::shared_ptr<async_binder>    binder,
                                   std::shared_ptr<async_connector> binder_monitor,
                                   std::shared_ptr<task_manager>    task_manager) {
  _binder         = binder;
  _binder_monitor = binder_monitor;
  _task_manager   = task_manager;
}

void worker_manager::on_client_disconnect(zmq::message_t source, capnp::ReaderFor<ClientDisconnect> message) {}
void worker_manager::on_client_shutdown(zmq::message_t& source) {}

void worker_manager::on_heartbeat(zmq::message_t& source, capnp::ReaderFor<Message> message) {
  bytes b((unsigned char*)source.data(), (unsigned char*)source.data() + source.size());

  if(_allocator.add_worker(b, message.getWorkerHeartbeat().getQueueSize())) {
    capnp::MallocMessageBuilder msg;
    Message::Builder            this_message  = msg.initRoot<Message>();
    auto                        state_builder = this_message.initStateWorker();

    kj::ArrayPtr<const capnp::byte> worker_id(reinterpret_cast<const capnp::byte*>(source.data()),
                                              source.size() / sizeof(capnp::byte));

    kj::ArrayPtr<const capnp::byte> msg_to(reinterpret_cast<const capnp::byte*>(std::string("connected").data()),
                                           source.size() / sizeof(capnp::byte));

    state_builder.setWorkerId(capnp::Data::Reader(worker_id));
    state_builder.setMessage(capnp::Data::Reader(msg_to));

    // _binder_monitor->send(std::move(this_message));
    _binder_monitor->send(&msg);
  }

  capnp::MallocMessageBuilder msg;
  Message::Builder            this_message = msg.initRoot<Message>();
  this_message.initWorkerHeartbeatEcho();
  this_message.setWorkerHeartbeatEcho({});
  _binder->send(zmq::message_t(b.data(), b.size()), &msg);
}

void worker_manager::on_task_result(capnp::ReaderFor<Message> message) {
  bytes task_id(message.getTaskResult().getTaskId().asBytes().begin(),
                message.getTaskResult().getTaskId().asBytes().end());

  auto worker = _allocator.remove_task(task_id);
  (void)worker;

  _task_manager->on_task_done(message.getTaskResult());
}

void worker_manager::on_disconnect(zmq::message_t source, capnp::ReaderFor<Message> message) {
  // TODO: disconnect_worker inside
  capnp::MallocMessageBuilder msg;
  Message::Builder            this_message        = msg.initRoot<Message>();
  auto                        disconnect_response = this_message.initDisconnectResponse();
  disconnect_response.setWorker(message.getDisconnectRequest().getWorker());
  _binder->send(std::move(source), &msg);
}

bool worker_manager::assign_task_to_worker(bytes task_id, kj::Array<capnp::word> task) {
  auto worker = _allocator.assign_task(task_id);
  if(!worker) return false;

  capnp::FlatArrayMessageReader reader(task);
  auto                          task_message = reader.getRoot<Message>();

  capnp::MallocMessageBuilder msg;
  Message::Builder            this_message = msg.initRoot<Message>();
  this_message.initTask();
  this_message.setTask(task_message.getTask());
  zmq::message_t worker_message(worker->begin(), worker->end());

  _binder->send(std::move(worker_message), &msg);

  return true;
}

bool worker_manager::has_available_worker() { return _allocator.has_available_worker(); }

std::vector<bytes> worker_manager::get_worker_ids() { return _allocator.get_worker_ids(); }
