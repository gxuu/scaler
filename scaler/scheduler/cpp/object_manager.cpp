#include "object_manager.h"

// make it a data class for comparison and other wonderful thing

void object_manager::on_object_create(zmq::message_t& source, capnp::ReaderFor<Message> message) {
  if(!_client_manager->has_client_id(message)) {
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
