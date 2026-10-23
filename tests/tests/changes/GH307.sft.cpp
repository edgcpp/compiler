//type:fp
//options:--clang
//remark:[GH #307] The macOS clang bases no longer predefine __BLOCKS__ and __block
//
// The front end does not support Apple's blocks extension (there is no block
// pointer type, and "^" cannot start a declarator), so __BLOCKS__ must not be
// predefined: the macOS SDK headers declare their block-taking functions
// (atexit_b, bsearch_b, qsort_b, ...) under "#ifdef __BLOCKS__", and every
// translation unit that included <stdlib.h> got errors.

// The declaration that failed to parse.
#if __BLOCKS__
int atexit_b(void (^)(void));
#endif

#ifdef __BLOCKS__
#error "__BLOCKS__ is defined"
#endif

#ifdef __block
#error "__block is defined"
#endif

#if __has_feature(blocks) || __has_extension(blocks)
#error "the front end does not support blocks"
#endif

int main() { return 0; }
