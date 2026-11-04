//type: fp
//options: --c++26:--c++26 --gn 160100
//remark:[GH #216] The expansion statement feature-test macro.

#if !defined(__cpp_expansion_statements)
#error __cpp_expansion_statements is not defined
#elif __cpp_expansion_statements != 202506L
#error unexpected __cpp_expansion_statements value
#endif
