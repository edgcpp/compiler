//type:cp
//options_all:--microsoft_v 1944
//options:-W;fn:--ms_c++23
  auto a = [] [[noreturn]] (auto z) { throw z; };
