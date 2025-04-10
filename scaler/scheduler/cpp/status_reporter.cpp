
#include "status_reporter.h"

void status_reporter::build_channel(std::shared_ptr<async_binder>   binder,
                                    std::shared_ptr<client_manager> client_manager,
                                    std::shared_ptr<object_manager> object_manager,
                                    std::shared_ptr<task_manager>   task_manager,
                                    std::shared_ptr<worker_manager> worker_manager) {
  _binder         = binder;
  _client_manager = client_manager;
  _object_manager = object_manager;
  _task_manager   = task_manager;
  _worker_manager = worker_manager;
}
