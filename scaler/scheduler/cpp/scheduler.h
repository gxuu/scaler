
#include <capnp/common.h>
#include <capnp/serialize.h>

#include <memory>

#include "address_monitor.h"
#include "async_binder.h"
#include "async_connector.h"
#include "client_manager.h"
#include "config.h"
#include "context.h"
#include "graphtask_manager.h"
#include "object_manager.h"
#include "status_reporter.h"
#include "task_manager.h"
#include "utility/corotask.h"
#include "worker_manager.h"
#include "zmq.hpp"

class scheduler {
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
  scheduler(scheduler_config config);

  void on_receive_message(zmq::message_t source, capnp::ReaderFor<Message> message, kj::Array<capnp::word> raw);

  CoroTask get_loops();
};
