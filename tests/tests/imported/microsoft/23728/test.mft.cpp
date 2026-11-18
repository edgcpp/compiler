//options_all:--microsoft_version 1928 --ms_c++latest -w
//source_files:coro_support.h
#include "coro_support.h"

void f()
{
  auto g = []() -> std::generator<int> {
    co_yield 1;
    co_yield 2;
  };
}
