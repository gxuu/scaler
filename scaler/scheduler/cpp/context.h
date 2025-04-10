#pragma once

#include "zmq.hpp"

class context {
public:
  std::shared_ptr<zmq::context_t> _context;

  context(int io_threads_count = 3) : _context(new zmq::context_t(io_threads_count)) {}
};
