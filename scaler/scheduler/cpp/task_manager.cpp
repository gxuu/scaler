
#include "task_manager.h"

#include <capnp/message.h>
#include <capnp/serialize.h>

#include "client_manager.h"
#include "graphtask_manager.h"
#include "object_manager.h"

task_manager::task_manager(int max_number_of_tasks_waiting)
    : _max_number_of_tasks_waiting(max_number_of_tasks_waiting) {}

void task_manager::build_channel(std::shared_ptr<async_binder> binder, std::shared_ptr<async_connector> binder_monitor,
                                 std::shared_ptr<client_manager>    client_manager,
                                 std::shared_ptr<object_manager>    object_manager,
                                 std::shared_ptr<worker_manager>    worker_manager,
                                 std::shared_ptr<graphtask_manager> graphtask_manager) {
  _binder            = binder;
  _binder_monitor    = binder_monitor;
  _client_manager    = client_manager;
  _object_manager    = object_manager;
  _worker_manager    = worker_manager;
  _graphtask_manager = graphtask_manager;
}

void task_manager::routine() {
  // std::queue<std::pair<zmq::message_t, capnp::ReaderFor<Task>>> _unassigned;
  if(_unassigned.empty()) return;

  auto task_id = _unassigned.front();
  _running.insert(task_id);
  _unassigned.pop();

  auto res = _worker_manager->assign_task_to_worker(task_id, std::move(_task_id_to_task[task_id].second));
}

void task_manager::on_task_done(capnp::ReaderFor<TaskResult> task_result) {
  printf("on_task_done\n");
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
    _task_id_to_task.erase(task_id);
    // This func_object_name is for send_monitor
    // auto func_object_name = _object_manager.get_object_name(_task_id_to_task[task_id].getFuncObjectId());
    // auto client = _client_manager->on_task_finish(task_id);
  }

  auto client = _client_manager->on_task_finish(task_id);
  if(_graphtask_manager->is_graph_sub_task(task_result)) {
    _graphtask_manager->on_graph_sub_task_done(task_result);
    return;
  }

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

void task_manager::on_task_cancel(zmq::message_t& source, capnp::ReaderFor<Message> message) {}
