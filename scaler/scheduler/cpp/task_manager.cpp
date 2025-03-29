module;

#include <capnp/message.h>
#include <capnp/serialize.h>
#include <message.capnp.h>

#include <zmq.hpp>

#include "capnp/common.h"
#include "utility/typedefs.h"

module scheduler;
import :task_manager;
import :client_manager;
import :worker_manager;

void task_manager::routine() {
  // std::queue<std::pair<zmq::message_t, capnp::ReaderFor<Task>>> _unassigned;
  if(_unassigned.empty()) return;

  auto task_id = _unassigned.front();
  _running.insert(task_id);
  _unassigned.pop();

  auto res = _worker_manager->assign_task_to_worker(task_id, std::move(_task_id_to_task[task_id].second));
}

void task_manager::on_task_done(capnp::ReaderFor<TaskResult> task_result) {
  switch(task_result.getStatus()) {
    // TODO: inc the corresponding var not impl yet
    case TaskStatus::SUCCESS:
    case TaskStatus::FAILED:
    case TaskStatus::CANCELED:
    case TaskStatus::NOT_FOUND:
    case TaskStatus::WORKER_DIED:
    case TaskStatus::NO_WORKER:

    // What to do with this
    case TaskStatus::INACTIVE:
    case TaskStatus::RUNNING:
    case TaskStatus::CANCELING:
    default:
      break;
  }

  bytes task_id(task_result.getTaskId().asBytes().begin(), task_result.getTaskId().asBytes().end());
  if(_task_id_to_task.contains(task_id)) {
    // This func_object_name is for send_monitor
    // auto func_object_name = _object_manager.get_object_name(_task_id_to_task[task_id].getFuncObjectId());
    // auto client = _client_manager->on_task_finish(task_id);
  }

  auto client = _client_manager->on_task_finish(task_id);
  // build task_result from capnp::ReaderFor<TaskResult> to BuilderFor<Message>
  // and then send
  zmq::message_t zmq_client(client.begin(), client.end());

  capnp::MallocMessageBuilder msg;
  auto                        this_message = msg.initRoot<Message>();
  this_message.initTaskResult();
  this_message.setTaskResult(task_result);

  _binder->send(std::move(zmq_client), &msg);
}

void task_manager::on_task_new(zmq::message_t source, capnp::ReaderFor<Message> message, kj::Array<capnp::word> raw) {
  if(!_worker_manager->has_available_worker()) {
    // send no worker msg to the client
    return;
  }
  _client_manager->on_task_begin(source, message.getTask().getTaskId());

  bytes key(message.getTask().getTaskId().asBytes().begin(), message.getTask().getTaskId().asBytes().end());
  // _task_id_to_task[key] = message.getTask();
  _task_id_to_task[key] = {message.getTask(), std::move(raw)};

  _unassigned.emplace(key);

  // send monitor doing work
}
