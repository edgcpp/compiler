//type:fn
//remark:[GH #182] Malformed and unsupported "GCC diagnostic" pragmas
//options:--g++ --gnu_version=150000:--g++ --gnu_version=120000:--g++ --gnu_version=110000:--g++ --gnu_version=40500:--clang
//options_all:--diag_error=unrecognized_gcc_pragma --diag_error=exp_string_literal
#pragma GCC diagnostic                   // Error: kind missing.
#pragma GCC diagnostic bogus             // Error: kind unknown.
#pragma GCC diagnostic ignored           // Error: option missing.
#pragma GCC diagnostic ignored -Wshadow  // Error: not a string literal.
#pragma GCC diagnostic ignored L"-Wshadow"  // Error: not a narrow string.
#if !defined(__clang__) && __GNUC__ == 4 && __GNUC_MINOR__ < 6
#pragma GCC diagnostic push              // Error: GCC 4.6 and later only.
#pragma GCC diagnostic pop               // Error: GCC 4.6 and later only.
#endif
#if defined(__clang__) || __GNUC__ < 12
#pragma GCC diagnostic ignored_attributes "vendor::"  // Error: GCC 12+ only.
#endif
#if !defined(__clang__)
#pragma GCC diagnostic fatal "-Wshadow"  // Error: clang only.
#endif
int x;
