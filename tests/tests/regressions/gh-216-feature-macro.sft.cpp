//type: fp
//options: --c++26:--c++26 --gn 160100
//remark:[GH #216] The expansion statement feature-test macro.

/* Do not advertise the feature until all expansion forms are supported. */
#if defined(__cpp_expansion_statements)
#error __cpp_expansion_statements is defined for partial support
#endif
