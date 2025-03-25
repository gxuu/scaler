
#include <capnp/common.h>
#include <capnp/serialize.h>

#include <coroutine>
#include <exception>
#include <memory>

#include "address_monitor.h"
#include "async_binder.h"
#include "async_connector.h"
#include "client_manager.h"
#include "context.h"
#include "graphtask_manager.h"
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
  scheduler()
      : _address_monitor(new address_monitor),
        _context(new context),
        _binder(new async_binder(_context, "scheduler")),
        _binder_monitor(new async_connector(_context, "scheduler_monitor", zmq::socket_type::pub, "bind")),
        _client_manager(new client_manager),
        _graphtask_manager(new graphtask_manager),
        _object_manager(new object_manager),
        _status_reporter(new status_reporter),
        _task_manager(new task_manager),
        _worker_manager(new worker_manager) {
    _binder->build_channel([this](zmq::message_t source, kj::Array<capnp::word> message) {
      capnp::FlatArrayMessageReader reader(message);
      this->on_receive_message(std::move(source), reader.getRoot<Message>(), std::move(message));
    });

    _client_manager->build_channel(_binder, _binder_monitor, _object_manager, _task_manager, _worker_manager);
    _object_manager->build_channel(_binder, _binder_monitor, _client_manager, _worker_manager);
    _task_manager->build_channel(_binder, _binder_monitor, _client_manager, _object_manager, _worker_manager,
                                 _graphtask_manager);
    _worker_manager->build_channel(_binder, _binder_monitor, _task_manager);
    _status_reporter->build_channel(_binder, _client_manager, _object_manager, _task_manager, _worker_manager);
  }

  void on_receive_message(zmq::message_t source, capnp::ReaderFor<Message> message, kj::Array<capnp::word> raw) {
    switch(message.which()) {
      // =====================================================================================
      // receive from upstream
      case Message::CLIENT_HEARTBEAT:
        printf("Message::CLIENT_HEARTBEAT\n");
        _client_manager->on_heartbeat(std::move(source), std::move(message));
        break;
      case Message::GRAPH_TASK:
        printf("Message::GRAPH_TASK\n");
        // TODO: Not implemented
        _graphtask_manager->on_graph_task(source, message);
        break;
      case Message::GRAPH_TASK_CANCEL:
        printf("Message::GRAPH_TASK_CANCEL\n");
        // TODO: Not implemented
        _graphtask_manager->on_graph_task_cancel(source, message);
        break;
      case Message::TASK:
        printf("Message::TASK\n");
        _task_manager->on_task_new(std::move(source), std::move(message), std::move(raw));
        break;
      case Message::TASK_CANCEL:
        printf("Message::TASK_CANCEL\n");
        // TODO: Not implemented
        _task_manager->on_task_cancel(source, message);
        break;
      case Message::CLIENT_DISCONNECT:
        printf("Message::CLIENT_DISCONNECT\n");
        _client_manager->on_client_disconnect(source, message);
        break;

      // =====================================================================================
      // receive from downstream
      // receive worker heartbeat from downstream
      case Message::WORKER_HEARTBEAT:
        printf("Message::WORKER_HEARTBEAT\n");
        _worker_manager->on_heartbeat(source, message);
        break;
      case Message::TASK_RESULT:
        printf("Message::TASK_RESULT\n");
        _worker_manager->on_task_result(message);
        break;
      case Message::DISCONNECT_REQUEST:
        printf("Message::DISCONNECT_REQUEST\n");
        // TODO: Not implemented
        _worker_manager->on_disconnect(std::move(source), std::move(message));
        break;

      // =====================================================================================
      // object related
      // scheduler receives object request from upstream
      case Message::OBJECT_INSTRUCTION:
        printf("Message::OBJECT_INSTRUCTION\n");
        _object_manager->on_object_instruction(source, message);
        break;
      case Message::OBJECT_REQUEST:
        printf("Message::OBJECT_REQUEST\n");
        _object_manager->on_object_request(std::move(source), message);
        break;

      // What about these?
      case Message::OBJECT_RESPONSE:
      case Message::CLIENT_HEARTBEAT_ECHO:
      case Message::WORKER_HEARTBEAT_ECHO:
      case Message::DISCONNECT_RESPONSE:
      case Message::STATE_CLIENT:
      case Message::STATE_OBJECT:
      case Message::STATE_BALANCE_ADVICE:
      case Message::STATE_SCHEDULER:
      case Message::STATE_WORKER:
      case Message::STATE_TASK:
      case Message::STATE_GRAPH_TASK:
      case Message::CLIENT_SHUTDOWN_RESPONSE:
      case Message::PROCESSOR_INITIALIZED:
      default:
        // TODO: Make a funeral
        std::terminate();
    }
  }

  CoroTask get_loops() {
    // auto task1 = create_corotine([this]() { this->_binder->routine(); });
    // auto task2 = create_corotine([this]() { this->_task_manager->routine(); });
    // auto task3 = create_corotine([this]() { this->_object_manager->routine(); });
    // auto task1 = helper(_binder.routine());
    // auto task1 = helper(_binder.routine());
    // auto task1 = helper(_binder.routine());
    // auto task1 = helper(_binder.routine());

    while(true) {
      _binder->routine();
      _task_manager->routine();
      _object_manager->routine();
      // task1.resume();
      // task2.resume();
      // task3.resume();
      // task1.resume();
      // task1.resume();
      // task1.resume();
      // task1.resume();

      // using namespace std::chrono_literals;
      // std::this_thread::sleep_for(0.5ms);
      // sleep(1);
      co_await std::suspend_always{};
    }
  }
};
