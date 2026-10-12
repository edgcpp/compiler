//type:cppbe
//options:--c++20:--gn 160200:--clang_version 230100
//options_all:-w
//require:INCLUDE_UNRECOGNIZED_PRAGMAS_IN_IL 1
//match_regex:#pragma clang diagnostic push
//match_regex:#pragma clang diagnostic pop
//match_regex:#pragma GCC diagnostic push
//match_regex:#pragma GCC diagnostic pop

#pragma clang diagnostic push
#pragma clang diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic pop
