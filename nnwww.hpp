#pragma once

#include <cstdint>

extern "C" char __www_start[];
extern "C" char __www_end[];

using NNWww = struct {
  const char** names;
  const char*  entry;
  const uint8_t** data;
  const uint64_t* bytes;
  const uint32_t  count;
};
