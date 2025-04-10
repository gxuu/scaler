#pragma once
#include <capnp/common.h>

#include <memory>
#include <queue>
#include <set>
#include <zmq.hpp>

#include "async_binder.h"
#include "async_connector.h"
#include "common.capnp.h"
#include "message.capnp.h"
#include "queued_allocator.h"
#include "utility/typedefs.h"

struct worker_manager;
struct graphtask_manager;
struct client_manager;
struct object_manager;

struct task_manager {
  int                                _max_number_of_tasks_waiting;
  std::shared_ptr<async_binder>      _binder;
  std::shared_ptr<async_connector>   _binder_monitor;
  std::shared_ptr<client_manager>    _client_manager;
  std::shared_ptr<object_manager>    _object_manager;
  std::shared_ptr<worker_manager>    _worker_manager;
  std::shared_ptr<graphtask_manager> _graphtask_manager;

  // Problem:
  //   zmq::message_t automatically destructs, and they are not copyable
  //   maening you cannot store them in two places simultaneously
  //
  // Solution:
  //   Either store pointers and worry about lifetime issues
  //   or store the underlying id
  //   Copying around 128 bits is nothing
  // std::queue<std::pair<zmq::message_t, capnp::ReaderFor<Task>>> _unassigned;
  using task_id_t = bytes;
  std::queue<task_id_t> _unassigned;
  // std::map<bytes, capnp::ReaderFor<Task>> _task_id_to_task;
  std::map<bytes, std::pair<capnp::ReaderFor<Task>, kj::Array<capnp::word>>> _task_id_to_task;
  std::set<task_id_t>                                                        _running;

  task_manager(int max_number_of_tasks_waiting = 1024);

  void build_channel(std::shared_ptr<async_binder> binder, std::shared_ptr<async_connector> binder_monitor,
                     std::shared_ptr<client_manager> client_manager, std::shared_ptr<object_manager> object_manager,
                     std::shared_ptr<worker_manager>    worker_manager,
                     std::shared_ptr<graphtask_manager> graphtask_manager);

  void on_task_new(zmq::message_t source, capnp::ReaderFor<Message> message, kj::Array<capnp::word> raw);

  void on_task_cancel(zmq::message_t& source, capnp::ReaderFor<Message> message);

  void on_task_done(capnp::ReaderFor<TaskResult> task_result);

  void routine();
};
