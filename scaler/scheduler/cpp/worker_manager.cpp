
module;
#include <capnp/common.h>
#include <capnp/message.h>

#include <zmq.hpp>

#include "message.capnp.h"
#include "queued_allocator.h"

module scheduler;
import :worker_manager;
import :task_manager;

void worker_manager::on_task_result(capnp::ReaderFor<Message> message) {
  bytes task_id(message.getTaskResult().getTaskId().asBytes().begin(),
                message.getTaskResult().getTaskId().asBytes().end());

  auto worker = _allocator.remove_task(task_id);
  (void)worker;

  _task_manager->on_task_done(message.getTaskResult());
}
