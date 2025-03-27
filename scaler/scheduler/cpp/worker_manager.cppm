module;

#include <kj/array.h>

#include <memory>
#include <zmq.hpp>

#include "async_binder.h"
#include "async_connector.h"
#include "message.capnp.h"
#include "queued_allocator.h"

export module scheduler:worker_manager;

export struct task_manager;

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

  void on_heartbeat(zmq::message_t& source, capnp::ReaderFor<Message> message);

  void on_task_result(capnp::ReaderFor<Message> message);

  void on_disconnect(zmq::message_t source, capnp::ReaderFor<Message> message);

  bool assign_task_to_worker(bytes task_id, kj::Array<capnp::word> task);

  bool has_available_worker() { return _allocator.has_available_worker(); }

  std::vector<bytes> get_worker_ids() { return _allocator.get_worker_ids(); }
};
