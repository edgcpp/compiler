//type:fp
//remark:[GH #182] Version-specific "GCC diagnostic" pragmas
//options:--g++ --gnu_version=150000:--g++ --gnu_version=120000:--g++ --gnu_version=110000:--g++ --gnu_version=40500:--clang
//options_all:--diag_error=unrecognized_gcc_pragma --diag_error=exp_string_literal
#pragma GCC diagnostic warning "-Wshadow"
#pragma GCC diagnostic error "-Wreturn-type"
#if defined(__clang__) || __GNUC__ > 4 || __GNUC_MINOR__ >= 6
#pragma GCC diagnostic push              // GCC 4.6 and later
#pragma GCC diagnostic pop extra text    // Trailing text is ignored.
#endif
#if !defined(__clang__) && __GNUC__ >= 12
#pragma GCC diagnostic ignored_attributes "vendor::"  // GCC 12 and later
#endif
#if defined(__clang__)
#pragma GCC diagnostic fatal "-Wshadow"  // clang only
#endif
int x;
