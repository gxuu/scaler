
module;
#include <capnp/common.h>
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/array.h>

#include <zmq.hpp>

#include "async_binder.h"
#include "message.capnp.h"
#include "queued_allocator.h"

module scheduler;
import :worker_manager;
import :task_manager;

void worker_manager::on_heartbeat(zmq::message_t& source, capnp::ReaderFor<Message> message) {
  bytes b((unsigned char*)source.data(), (unsigned char*)source.data() + source.size());

  if(_allocator.add_worker(b, message.getWorkerHeartbeat().getQueueSize())) {
    printf("add_worker in\n");
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

  printf("add_worker out\n");
  capnp::MallocMessageBuilder msg;
  Message::Builder            this_message = msg.initRoot<Message>();
  this_message.initWorkerHeartbeatEcho();
  this_message.setWorkerHeartbeatEcho({});
  _binder->send(zmq::message_t(b.data(), b.size()), &msg);
}

void worker_manager::on_disconnect(zmq::message_t source, capnp::ReaderFor<Message> message) {
  // TODO: disconnect_worker inside
  capnp::MallocMessageBuilder msg;
  Message::Builder            this_message        = msg.initRoot<Message>();
  auto                        disconnect_response = this_message.initDisconnectResponse();
  disconnect_response.setWorker(message.getDisconnectRequest().getWorker());
  _binder->send(std::move(source), &msg);
}

void worker_manager::on_task_result(capnp::ReaderFor<Message> message) {
  bytes task_id(message.getTaskResult().getTaskId().asBytes().begin(),
                message.getTaskResult().getTaskId().asBytes().end());

  auto worker = _allocator.remove_task(task_id);
  (void)worker;

  _task_manager->on_task_done(message.getTaskResult());
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
