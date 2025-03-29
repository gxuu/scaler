module;

#include <capnp/common.h>
#include <capnp/serialize.h>

#include <coroutine>
#include <exception>
#include <memory>

#include "address_monitor.h"
#include "async_binder.h"
#include "async_connector.h"
#include "context.h"
#include "utility/corotask.h"
#include "zmq.hpp"

export module scheduler;

// import :task_manager;
// import :worker_manager;
// import :client_manager;
// import :object_manager;
// import :graphtask_manager;
// import :status_reporter;

// TODO: We will move scheduler impl into cpp file
export struct worker_manager;
export struct task_manager;
export struct client_manager;
export struct object_manager;
export struct graphtask_manager;
export struct status_reporter;

export class scheduler {
public:
  std::shared_ptr<address_monitor>   _address_monitor;
  std::shared_ptr<context>           _context;
  std::shared_ptr<async_binder>      _binder;
  std::shared_ptr<async_connector>   _binder_monitor;
  std::shared_ptr<client_manager>    _client_manager;
  std::shared_ptr<graphtask_manager> _graphtask_manager;
  std::shared_ptr<object_manager>    _object_manager;
  std::shared_ptr<status_reporter>   _status_reporter;
  std::shared_ptr<task_manager>      _task_manager;
  std::shared_ptr<worker_manager>    _worker_manager;

public:
  scheduler();

  void on_receive_message(zmq::message_t source, capnp::ReaderFor<Message> message, kj::Array<capnp::word> raw);

  CoroTask get_loops();
};
