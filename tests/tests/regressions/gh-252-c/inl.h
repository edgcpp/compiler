/* Definitions only for inlining. */
#if __GNUC_GNU_INLINE__
#define INLINE_ONLY extern inline
#else
#define INLINE_ONLY inline
#endif
INLINE_ONLY int h(void) { return 1; }
INLINE_ONLY int v(int n, ...) { return n; }
#ifdef __GNUC__
extern inline __attribute__((gnu_inline)) int k(void) { return 1; }
#else
INLINE_ONLY int k(void) { return 1; }
#endif
