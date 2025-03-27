module;

#include <memory>

#include "async_binder.h"

export module scheduler:status_reporter;
import :client_manager;
import :object_manager;
import :task_manager;
import :worker_manager;

struct status_reporter {
  std::shared_ptr<async_binder>   _binder;
  std::shared_ptr<client_manager> _client_manager;
  std::shared_ptr<object_manager> _object_manager;
  std::shared_ptr<task_manager>   _task_manager;
  std::shared_ptr<worker_manager> _worker_manager;

  void build_channel(std::shared_ptr<async_binder> binder, std::shared_ptr<client_manager> client_manager,
                     std::shared_ptr<object_manager> object_manager, std::shared_ptr<task_manager> task_manager,
                     std::shared_ptr<worker_manager> worker_manager) {
    _binder         = binder;
    _client_manager = client_manager;
    _object_manager = object_manager;
    _task_manager   = task_manager;
    _worker_manager = worker_manager;
  }
};
