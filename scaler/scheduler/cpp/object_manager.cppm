module;

#include <capnp/blob.h>
#include <capnp/common.h>
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/common.h>

#include <memory>
#include <queue>
#include <ranges>
#include <zmq.hpp>

#include "async_binder.h"
#include "async_connector.h"
// #include "client_manager.h"
#include <set>

#include "common.capnp.h"
#include "message.capnp.h"
#include "utility/object_tracker.h"
#include "utility/typedefs.h"
// #include "worker_manager.h"

export module scheduler:object_manager;
import :worker_manager;
import :client_manager;
export struct client_manager;

struct _object_creation {
  bytes                            object_id;
  bytes                            object_creator;
  ObjectContent::ObjectContentType object_type;
  bytes                            object_name;
  std::vector<bytes>               object_bytes;

  bytes get_object_key() { return object_id; }
};

struct object_tracker {
  std::vector<_object_creation>        creations;
  std::vector<std::pair<bytes, bytes>> _object_key_to_block;
  std::set<bytes>                      current;

  void add_object(_object_creation creation) { creations.push_back(creation); }

  void add_blocks_for_one_object(bytes object_key, std::set<bytes> blocks) {
    for(const auto& block : blocks) {
      _object_key_to_block.push_back({object_key, block});
    }
    current.merge(blocks);
  }

  void remove_one_block_for_objects(bytes id, zmq::message_t& source) {
    printf("remove_one_block_for_objects\n");
    return;
  }

  _object_creation get_object(bytes object_id) {
    for(int ii = 0; ii < creations.size(); ++ii) {
      if(creations[ii].get_object_key() == object_id) {
        return creations[ii];
      }
    }
    assert(false);
    return {};
  }
};

export struct object_manager {
  std::shared_ptr<async_binder>    _binder;
  std::shared_ptr<async_connector> _binder_monitor;
  std::shared_ptr<client_manager>  _client_manager;
  std::shared_ptr<worker_manager>  _worker_manager;

  std::queue<bytes> _queue_deleted_object_ids;

  object_tracker _object_storage;

  void build_channel(std::shared_ptr<async_binder> binder, std::shared_ptr<async_connector> binder_monitor,
                     std::shared_ptr<client_manager> client_manager, std::shared_ptr<worker_manager> worker_manager) {
    _binder         = binder;
    _binder_monitor = binder_monitor;
    _client_manager = client_manager;
    _worker_manager = worker_manager;
  }

  void on_object_instruction(zmq::message_t& source, capnp::ReaderFor<Message> message) {
    switch(message.getObjectInstruction().getInstructionType()) {
      case ObjectInstruction::ObjectInstructionType::CREATE:
        on_object_create(source, message);
        break;
      case ObjectInstruction::ObjectInstructionType::DELETE:
        on_del_objects(source, message);
        break;

      case ObjectInstruction::ObjectInstructionType::CLEAR:
      default:
        break;
    }
    return;
  }

  void on_object_create(zmq::message_t& source, capnp::ReaderFor<Message> message);
  void on_del_objects(zmq::message_t& source, capnp::ReaderFor<Message> message) {
    auto ids = message.getObjectInstruction().getObjectContent().getObjectIds();
    for(const auto& id : ids) {
      bytes byte_id(id.asBytes().begin(), id.asBytes().end());
      _object_storage.remove_one_block_for_objects(byte_id, source);
    }
  }

  void on_object_request(zmq::message_t source, capnp::ReaderFor<Message> message) {
    printf("on_object_request\n");
    switch(message.getObjectRequest().getRequestType()) {
      case ObjectRequest::ObjectRequestType::GET:
        process_get_request(std::move(source), message);
        break;
      default:
        break;
    }
    return;
  }

  void on_add_object(_object_creation creation) {
    _object_storage.add_object(creation);
    _object_storage.add_blocks_for_one_object(creation.get_object_key(), {creation.object_creator});
  }

  void process_get_request(zmq::message_t source, capnp::ReaderFor<Message> message) {
    capnp::MallocMessageBuilder msg;
    auto                        this_message = msg.initRoot<Message>();
    auto                        response     = this_message.initObjectResponse();
    auto                        content      = response.initObjectContent();

    auto obj_ids = message.getObjectRequest().getObjectIds();
    auto size    = obj_ids.size();

    auto ids       = content.initObjectIds(size);
    auto types     = content.initObjectTypes(size);
    auto names     = content.initObjectNames(size);
    auto obj_bytes = content.initObjectBytes(size);

    for(int ii = 0; ii < size; ++ii) {
      bytes id(obj_ids[ii].asBytes().begin(), obj_ids[ii].asBytes().end());
      auto  obj_info = _object_storage.get_object(id);

      ids.set(ii, capnp::Data::Reader(obj_info.object_id.data(), obj_info.object_id.size()));
      types.set(ii, obj_info.object_type);
      names.set(ii, capnp::Data::Reader(obj_info.object_name.data(), obj_info.object_name.size()));

      obj_bytes.init(ii, obj_info.object_bytes.size());
      for(int jj = 0; jj < obj_info.object_bytes.size(); ++jj) {
        obj_bytes[ii].set(jj, capnp::Data::Reader(obj_info.object_bytes[jj].data(), obj_info.object_bytes[jj].size()));
      }
    }

    _binder->send(std::move(source), &msg);
  }

  void routine() { routine_send_objects_deletions(); }

  void routine_send_objects_deletions() {
    std::vector<bytes> deleted_object_ids{_queue_deleted_object_ids.front()};
    while(!_queue_deleted_object_ids.empty()) {
      deleted_object_ids.push_back(_queue_deleted_object_ids.front());
      _queue_deleted_object_ids.pop();
    }
    auto worker_ids = _worker_manager->get_worker_ids();
    for(const auto& id : worker_ids) {
      zmq::message_t client(id.begin(), id.end());

      capnp::MallocMessageBuilder msg;
      auto                        this_message = msg.initRoot<Message>();
      auto                        instruction  = this_message.initObjectInstruction();
      auto                        content      = instruction.initObjectContent();
      instruction.setInstructionType(ObjectInstruction::ObjectInstructionType::DELETE);

      auto ids = content.initObjectIds(deleted_object_ids.size());
      for(int ii = 0; ii < deleted_object_ids.size(); ++ii) {
        ids.set(ii, capnp::Data::Reader(deleted_object_ids[ii].data(), deleted_object_ids[ii].size()));
      }

      _binder->send(std::move(client), &msg);
    }
  }
};
