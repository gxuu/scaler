
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using bytes = std::vector<unsigned char>;

enum class actor_t {
  SCHEDULER,
  WORKER,
};

struct identity_t {
  std::array<uint64_t, 4> internal;

  std::string encode() {
    char* first = (char*)internal.data();
    char* last  = (char*)internal.data() + internal.size() * sizeof(uint64_t) / sizeof(char);
    return std::string(first, last);
  }
};
