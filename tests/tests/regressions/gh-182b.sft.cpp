//type:fp
//remark:[GH #182] Malformed and version-specific "GCC diagnostic" pragmas
//options:--g++ --gnu_version=150000:--g++ --gnu_version=120000:--g++ --gnu_version=40500:--clang
#pragma GCC diagnostic push              // GCC 4.6 and later
#pragma GCC diagnostic warning "-Wshadow"
#pragma GCC diagnostic error "-Wreturn-type"
#pragma GCC diagnostic ignored_attributes "vendor::"  // GCC 13 and later
#pragma GCC diagnostic fatal "-Wshadow"  // clang only
#pragma GCC diagnostic pop extra text    // Trailing text is ignored.
#pragma GCC diagnostic                   // Kind missing.
#pragma GCC diagnostic bogus             // Kind unknown.
#pragma GCC diagnostic ignored           // Option missing.
#pragma GCC diagnostic ignored -Wshadow  // Option not a string literal.
#pragma GCC diagnostic ignored L"-Wshadow"  // Option not a narrow string.
int x;
