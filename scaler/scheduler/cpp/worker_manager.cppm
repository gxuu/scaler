module;
// #pragma once

#include <capnp/common.h>
#include <capnp/message.h>
#include <capnp/serialize-packed.h>
#include <capnp/serialize.h>
#include <kj/array.h>

#include <cstring>
#include <memory>
#include <set>
#include <zmq.hpp>

#include "queued_allocator.h"
// #include "async_binder.h"
// #include "async_connector.h"
// #include "message.capnp.h"
// #include "task_manager.h"

struct task_manager;

export module WorkerManager;
import AsyncBinder;
import AsyncConnector;
import TaskManager;

export struct worker_manager {
  int                              _timeout_seconds;
  int                              _load_balance_seconds;
  int                              _load_balance_trigger_times;
  std::shared_ptr<async_binder>    _binder;
  std::shared_ptr<async_connector> _binder_monitor;
  std::shared_ptr<task_manager>    _task_manager;

  // content of the allocator here

  queued_allocator _allocator;

  worker_manager(int timeout_seconds = 100, int load_balance_seconds = 1, int load_balance_trigger_times = 1)
      : _timeout_seconds(timeout_seconds),
        _load_balance_seconds(load_balance_seconds),
        _load_balance_trigger_times(load_balance_trigger_times) {}

  void build_channel(std::shared_ptr<async_binder> binder, std::shared_ptr<async_connector> binder_monitor,
                     std::shared_ptr<task_manager> task_manager) {
    _binder         = binder;
    _binder_monitor = binder_monitor;
    _task_manager   = task_manager;
  }

  void on_client_disconnect(zmq::message_t source, capnp::ReaderFor<ClientDisconnect> message) {}
  void on_client_shutdown(zmq::message_t& source) {}

  void on_heartbeat(zmq::message_t& source, capnp::ReaderFor<Message> message) {
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

  void on_task_result(capnp::ReaderFor<Message> message);

  void on_disconnect(zmq::message_t source, capnp::ReaderFor<Message> message) {
    // TODO: disconnect_worker inside
    capnp::MallocMessageBuilder msg;
    Message::Builder            this_message        = msg.initRoot<Message>();
    auto                        disconnect_response = this_message.initDisconnectResponse();
    disconnect_response.setWorker(message.getDisconnectRequest().getWorker());
    _binder->send(std::move(source), &msg);
  }

  bool assign_task_to_worker(bytes task_id, kj::Array<capnp::word> task) {
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

  bool has_available_worker() { return _allocator.has_available_worker(); }

  std::vector<bytes> get_worker_ids() { return _allocator.get_worker_ids(); }
};
