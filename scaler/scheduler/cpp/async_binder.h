#pragma once
#include <capnp/c++.capnp.h>
#include <capnp/common.h>
#include <capnp/generated-header-support.h>
#include <capnp/message.h>
#include <capnp/orphan.h>
#include <capnp/serialize.h>
#include <fcntl.h>
#include <kj/common.h>
#include <unistd.h>

#include <cstdint>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <optional>
#include <zmq.hpp>
#include <zmq_addon.hpp>

#include "context.h"
#include "kj/array.h"
#include "message.capnp.h"
#include "utility/typedefs.h"
#include "zmq_config.h"

struct async_binder {
  using callback_t = std::function<void(zmq::message_t, kj::Array<capnp::word>)>;
  std::shared_ptr<context> _context;
  actor_t                  _name;
  callback_t               _callback;
  zmq::socket_t            _sock;
  zmq_config               _address;
  identity_t               _identity;  // change this type later

  std::map<Message::Which, int> _received;
  std::map<Message::Which, int> _sent;

  async_binder(std::shared_ptr<context> context, actor_t name, zmq_config address, std::optional<identity_t> identity)
      : _context(context),
        _name(name),
        _sock(zmq::socket_t(*_context->_context, zmq::socket_type::router)),
        _address(address),
        _identity(initialize_identity(identity)) {
    _sock.set(zmq::sockopt::routing_id, _identity.encode());
    _sock.set(zmq::sockopt::sndhwm, 0);
    _sock.set(zmq::sockopt::rcvhwm, 0);
    _sock.bind(address.to_address());
  }

  // [
  //   [upper 32: actor type][lower 32: pid]]
  //   [first 64 bits from random]
  //   [second 64 bits from random]
  //   [0]
  // ]
  identity_t initialize_identity(std::optional<identity_t> identity) {
    if(identity) return *identity;
    uint64_t first  = (uint64_t)actor_t::SCHEDULER << 31 & (uint64_t)getpid();
    uint64_t second = 0;
    uint64_t third  = 0;

    int random_data = open("/dev/urandom", O_RDONLY);
    if(random_data < 0) {
      // something went wrong
    } else {
      ssize_t result = read(random_data, &second, sizeof second);
      if(result < 0) {
        // something went wrong
      }
      result = read(random_data, &third, sizeof third);
      if(result < 0) {
        // something went wrong
      }
    }
    close(random_data);

    uint64_t fourth = 0;

    identity->internal = {first, second, third, fourth};
    return *identity;
  }

  void build_channel(callback_t f) { _callback = f; }

  // maybe put that in the dtor
  void destroy(int linger = 0) {
    _sock.set(zmq::sockopt::linger, linger);
    _sock.close();
    _context->_context->close();
  }

  ~async_binder() { destroy(); }

  void routine() {
    std::vector<zmq::message_t> msgs;

    auto ret = zmq::recv_multipart(_sock, std::back_inserter(msgs));
    if(!ret) return;
    if(!valid_message()) {
      // do work and return
      return;
    }

    auto message = deserialize(msgs[1].data(), msgs[1].size());

    capnp::FlatArrayMessageReader reader(message);
    count_received(reader.getRoot<Message>().which());

    _callback(std::move(msgs[0]), std::move(message));
  }

  bool valid_message() { return true; }

  // because we need to keep the array alive
  kj::Array<capnp::word> deserialize(void* raw_bytes, int size) {
    // https://stackoverflow.com/questions/75006774/capn-proto-unaligned-data-error-while-trying-to-sendreceive-serialized-object
    auto buffer = kj::heapArray<capnp::word>(size / sizeof(capnp::word));
    memcpy(buffer.asBytes().begin(), raw_bytes, buffer.asBytes().size());
    return buffer;
  }

  void send(zmq::message_t client, capnp::MallocMessageBuilder* message) {
    // count_send()
    auto flat_array = capnp::messageToFlatArray(*message);
    // Surprise: flat_array.size() != flat_array.asBytes().size()
    std::array<zmq::message_t, 2> send_msgs = {
        std::move(client), zmq::message_t(flat_array.asBytes().begin(), flat_array.asBytes().end())};
    zmq::send_multipart(_sock, send_msgs);
  }

  void get_status() {}

private:
  void count_received(Message::Which type) { ++_received[type]; }
  void count_send(Message::Which type) { ++_sent[type]; }
  void get_prefix() {}
};
