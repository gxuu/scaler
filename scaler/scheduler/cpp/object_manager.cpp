#include "object_manager.h"

void object_manager::build_channel(std::shared_ptr<async_binder>    binder,
                                   std::shared_ptr<async_connector> binder_monitor,
                                   std::shared_ptr<client_manager>  client_manager,
                                   std::shared_ptr<worker_manager>  worker_manager) {
  _binder         = binder;
  _binder_monitor = binder_monitor;
  _client_manager = client_manager;
  _worker_manager = worker_manager;
}

void object_manager::on_object_instruction(zmq::message_t& source, capnp::ReaderFor<Message> message) {
  switch(message.getObjectInstruction().getInstructionType()) {
    case ObjectInstruction::ObjectInstructionType::CREATE:
      printf("create\n");
      on_object_create(source, message);
      break;
    case ObjectInstruction::ObjectInstructionType::DELETE:
      printf("delete\n");
      on_del_objects(source, message);
      break;

    case ObjectInstruction::ObjectInstructionType::CLEAR:
      printf("clear\n");
    default:
      break;
  }
  return;
}

void object_manager::on_del_objects(zmq::message_t& source, capnp::ReaderFor<Message> message) {
  auto ids = message.getObjectInstruction().getObjectContent().getObjectIds();
  for(const auto& id : ids) {
    bytes byte_id(id.asBytes().begin(), id.asBytes().end());
    _object_storage.remove_one_block_for_objects(byte_id, source);
  }
}

void object_manager::on_object_request(zmq::message_t source, capnp::ReaderFor<Message> message) {
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

void object_manager::on_add_object(_object_creation creation) {
  _object_storage.add_object(creation);
  _object_storage.add_blocks_for_one_object(creation.get_object_key(), {creation.object_creator});
}

void object_manager::on_object_create(zmq::message_t& source, capnp::ReaderFor<Message> message) {
  printf("on_object_create\n");
  if(!_client_manager->has_client_id(message)) {
    assert(0);
    return;
  }

  auto object_ids   = message.getObjectInstruction().getObjectContent().getObjectIds();
  auto object_types = message.getObjectInstruction().getObjectContent().getObjectTypes();
  auto object_names = message.getObjectInstruction().getObjectContent().getObjectNames();
  auto object_bytes = message.getObjectInstruction().getObjectContent().getObjectBytes();
  auto object_user  = message.getObjectInstruction().getObjectUser();

  size_t len = object_ids.size();

  for(auto ii = 0z; ii < len; ++ii) {
    _object_creation creation{
        .object_id      = bytes(object_ids[ii].asBytes().begin(), object_ids[ii].asBytes().end()),
        .object_creator = bytes(object_user.asBytes().begin(), object_user.asBytes().end()),
        .object_type    = object_types[ii],
        .object_name    = bytes(object_names[ii].asBytes().begin(), object_names[ii].asBytes().end()),
        .object_bytes   = {},
    };

    for(const auto& b : object_bytes[ii]) {
      creation.object_bytes.emplace_back(b.asBytes().begin(), b.asBytes().end());
    }

    on_add_object(std::move(creation));
  }
}

void object_manager::process_get_request(zmq::message_t source, capnp::ReaderFor<Message> message) {
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

void object_manager::routine() { routine_send_objects_deletions(); }

void object_manager::routine_send_objects_deletions() {
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
