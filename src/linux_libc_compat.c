/**
 * @file linux_libc_compat.c
 * @brief Portable Linux libc compatibility shims for glibc 2.38+ C23 symbols and musl.
 * @details Provides weak C23 integer/scanf wrappers redirecting to POSIX C99 entry points
 * and sanitizer-aware recursive mutex shims for portable Linux release binaries.
 * @ingroup edg_be_common
 *
 * Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
 * Exceptions.
 * See https://edgcpp.org/LICENSE.txt for license information.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 */

#if defined(__linux__)

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
/** @brief Expands to `noexcept` when compiled as C++ to match glibc `__THROW` specifications. */
#define EDG_LINUX_COMPAT_NOEXCEPT noexcept
/** @brief Marks intentionally unused parameters in C++17 builds. */
#define EDG_LINUX_COMPAT_MAYBE_UNUSED [[maybe_unused]]
#else
/** @brief Empty exception specification macro when compiled as C. */
#define EDG_LINUX_COMPAT_NOEXCEPT
/** @brief Marks intentionally unused parameters in C builds. */
#define EDG_LINUX_COMPAT_MAYBE_UNUSED __attribute__((unused))
#endif

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
/** @brief Non-zero when AddressSanitizer or ThreadSanitizer interceptors are active. */
#define EDG_LINUX_COMPAT_SANITIZER_ACTIVE 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
/** @brief Non-zero when Clang AddressSanitizer or ThreadSanitizer interceptors are active. */
#define EDG_LINUX_COMPAT_SANITIZER_ACTIVE 1
#else
/** @brief Zero when neither AddressSanitizer nor ThreadSanitizer is active. */
#define EDG_LINUX_COMPAT_SANITIZER_ACTIVE 0
#endif
#else
/** @brief Zero when sanitizer feature detection is unavailable and no sanitizer macros are set. */
#define EDG_LINUX_COMPAT_SANITIZER_ACTIVE 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Direct symbol binding to baseline POSIX `strtol`. */
extern long edg_posix_strtol(const char *nptr, char **endptr, int base) EDG_LINUX_COMPAT_NOEXCEPT __asm__("strtol");
/** @brief Direct symbol binding to baseline POSIX `strtoll`. */
extern long long edg_posix_strtoll(const char *nptr, char **endptr, int base) EDG_LINUX_COMPAT_NOEXCEPT __asm__("strtoll");
/** @brief Direct symbol binding to baseline POSIX `strtoul`. */
extern unsigned long edg_posix_strtoul(const char *nptr, char **endptr, int base) EDG_LINUX_COMPAT_NOEXCEPT __asm__("strtoul");
/** @brief Direct symbol binding to baseline POSIX `strtoull`. */
extern unsigned long long edg_posix_strtoull(const char *nptr, char **endptr, int base) EDG_LINUX_COMPAT_NOEXCEPT __asm__("strtoull");
/** @brief Direct symbol binding to baseline POSIX `strtoimax`. */
extern intmax_t edg_posix_strtoimax(const char *nptr, char **endptr, int base) EDG_LINUX_COMPAT_NOEXCEPT __asm__("strtoimax");
/** @brief Direct symbol binding to baseline POSIX `strtoumax`. */
extern uintmax_t edg_posix_strtoumax(const char *nptr, char **endptr, int base) EDG_LINUX_COMPAT_NOEXCEPT __asm__("strtoumax");
/** @brief Direct symbol binding to baseline POSIX `vfscanf`. */
extern int edg_posix_vfscanf(FILE *stream, const char *format, va_list ap) __asm__("vfscanf");
/** @brief Direct symbol binding to baseline POSIX `vsscanf`. */
extern int edg_posix_vsscanf(const char *str, const char *format, va_list ap) EDG_LINUX_COMPAT_NOEXCEPT __asm__("vsscanf");

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_strtol`.
 * @details Redirects C23 `strtol` calls to the baseline POSIX `strtol` symbol.
 * @param[in] nptr Input null-terminated string.
 * @param[out] endptr Optional pointer to first unparsed character.
 * @param[in] base Numeric radix.
 * @return Parsed `long` value.
 */
__attribute__((weak, visibility("default")))
long __isoc23_strtol(const char *nptr, char **endptr, int base) EDG_LINUX_COMPAT_NOEXCEPT {
  return edg_posix_strtol(nptr, endptr, base);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_strtoll`.
 * @details Redirects C23 `strtoll` calls to the baseline POSIX `strtoll` symbol.
 * @param[in] nptr Input null-terminated string.
 * @param[out] endptr Optional pointer to first unparsed character.
 * @param[in] base Numeric radix.
 * @return Parsed `long long` value.
 */
__attribute__((weak, visibility("default")))
long long __isoc23_strtoll(const char *nptr, char **endptr, int base) EDG_LINUX_COMPAT_NOEXCEPT {
  return edg_posix_strtoll(nptr, endptr, base);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_strtoul`.
 * @details Redirects C23 `strtoul` calls to the baseline POSIX `strtoul` symbol.
 * @param[in] nptr Input null-terminated string.
 * @param[out] endptr Optional pointer to first unparsed character.
 * @param[in] base Numeric radix.
 * @return Parsed `unsigned long` value.
 */
__attribute__((weak, visibility("default")))
unsigned long __isoc23_strtoul(const char *nptr, char **endptr, int base) EDG_LINUX_COMPAT_NOEXCEPT {
  return edg_posix_strtoul(nptr, endptr, base);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_strtoull`.
 * @details Redirects C23 `strtoull` calls to the baseline POSIX `strtoull` symbol.
 * @param[in] nptr Input null-terminated string.
 * @param[out] endptr Optional pointer to first unparsed character.
 * @param[in] base Numeric radix.
 * @return Parsed `unsigned long long` value.
 */
__attribute__((weak, visibility("default")))
unsigned long long __isoc23_strtoull(const char *nptr, char **endptr, int base) EDG_LINUX_COMPAT_NOEXCEPT {
  return edg_posix_strtoull(nptr, endptr, base);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_strtoimax`.
 * @details Redirects C23 `strtoimax` calls to the baseline POSIX `strtoimax` symbol.
 * @param[in] nptr Input null-terminated string.
 * @param[out] endptr Optional pointer to first unparsed character.
 * @param[in] base Numeric radix.
 * @return Parsed `intmax_t` value.
 */
__attribute__((weak, visibility("default")))
intmax_t __isoc23_strtoimax(const char *nptr, char **endptr, int base) EDG_LINUX_COMPAT_NOEXCEPT {
  return edg_posix_strtoimax(nptr, endptr, base);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_strtoumax`.
 * @details Redirects C23 `strtoumax` calls to the baseline POSIX `strtoumax` symbol.
 * @param[in] nptr Input null-terminated string.
 * @param[out] endptr Optional pointer to first unparsed character.
 * @param[in] base Numeric radix.
 * @return Parsed `uintmax_t` value.
 */
__attribute__((weak, visibility("default")))
uintmax_t __isoc23_strtoumax(const char *nptr, char **endptr, int base) EDG_LINUX_COMPAT_NOEXCEPT {
  return edg_posix_strtoumax(nptr, endptr, base);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_vfscanf`.
 * @details Redirects C23 `vfscanf` calls to the baseline POSIX `vfscanf` symbol (`stream` may block).
 * @param[in,out] stream Input `FILE` stream.
 * @param[in] format Scanf format string.
 * @param[in] ap Variadic argument list.
 * @return Number of matched items or `EOF`.
 */
__attribute__((weak, visibility("default")))
int __isoc23_vfscanf(FILE *stream, const char *format, va_list ap) {
  return edg_posix_vfscanf(stream, format, ap);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_vsscanf`.
 * @details Redirects C23 `vsscanf` calls to the baseline POSIX `vsscanf` symbol.
 * @param[in] str Input null-terminated string.
 * @param[in] format Scanf format string.
 * @param[in] ap Variadic argument list.
 * @return Number of matched items or `EOF`.
 */
__attribute__((weak, visibility("default")))
int __isoc23_vsscanf(const char *str, const char *format, va_list ap) EDG_LINUX_COMPAT_NOEXCEPT {
  return edg_posix_vsscanf(str, format, ap);
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_fscanf`.
 * @details Redirects C23 `fscanf` calls to the baseline POSIX `vfscanf` symbol (`stream` may block).
 * @param[in,out] stream Input `FILE` stream.
 * @param[in] format Scanf format string.
 * @return Number of matched items or `EOF`.
 */
__attribute__((weak, visibility("default")))
int __isoc23_fscanf(FILE *stream, const char *format, ...) {
  va_list ap;
  va_start(ap, format);
  int rc = edg_posix_vfscanf(stream, format, ap);
  va_end(ap);
  return rc;
}

/**
 * @brief Compatibility wrapper for glibc 2.38+ `__isoc23_sscanf`.
 * @details Redirects C23 `sscanf` calls to the baseline POSIX `vsscanf` symbol.
 * @param[in] str Input null-terminated string.
 * @param[in] format Scanf format string.
 * @return Number of matched items or `EOF`.
 */
__attribute__((weak, visibility("default")))
int __isoc23_sscanf(const char *str, const char *format, ...) EDG_LINUX_COMPAT_NOEXCEPT {
  va_list ap;
  va_start(ap, format);
  int rc = edg_posix_vsscanf(str, format, ap);
  va_end(ap);
  return rc;
}

#if !EDG_LINUX_COMPAT_SANITIZER_ACTIVE
/**
 * @brief Per-thread unique address token for ABI-portable recursive mutex ownership on Linux (glibc + musl).
 */
static __thread char edg_linux_thread_token;

/**
 * @brief ABI-portable `pthread_mutex_init` compatible with both glibc and musl `pthread_mutex_t` layouts.
 * @details Zeroes the owner and recursion-depth words of `mutex`.
 * @param[out] mutex Pointer to `pthread_mutex_t` storage.
 * @param[in] attr Unused mutex attribute pointer.
 * @return `0` on success, or `22` (`EINVAL`) if `mutex` is null.
 */
__attribute__((weak, visibility("default")))
int pthread_mutex_init(void *mutex, EDG_LINUX_COMPAT_MAYBE_UNUSED const void *attr) EDG_LINUX_COMPAT_NOEXCEPT {
  if (!mutex) return 22;
  uintptr_t *words = (uintptr_t *)mutex;
  __atomic_store_n(&words[0], (uintptr_t)0, __ATOMIC_RELEASE);
  __atomic_store_n(&words[1], (uintptr_t)0, __ATOMIC_RELEASE);
  return 0;
}

/**
 * @brief ABI-portable `pthread_mutex_destroy` compatible with both glibc and musl `pthread_mutex_t` layouts.
 * @details Clears the owner and recursion-depth words of `mutex`.
 * @param[in,out] mutex Pointer to `pthread_mutex_t` storage.
 * @return `0` on success, or `22` (`EINVAL`) if `mutex` is null.
 */
__attribute__((weak, visibility("default")))
int pthread_mutex_destroy(void *mutex) EDG_LINUX_COMPAT_NOEXCEPT {
  if (!mutex) return 22;
  uintptr_t *words = (uintptr_t *)mutex;
  __atomic_store_n(&words[0], (uintptr_t)0, __ATOMIC_RELEASE);
  __atomic_store_n(&words[1], (uintptr_t)0, __ATOMIC_RELEASE);
  return 0;
}

/**
 * @brief ABI-portable recursive-safe `pthread_mutex_lock` preventing glibc/musl `pthread_mutex_t` offset-16 deadlocks.
 * @details Acquires or increments recursive ownership using `&edg_linux_thread_token`.
 * @param[in,out] mutex Pointer to `pthread_mutex_t` storage.
 * @return `0` on success, or `22` (`EINVAL`) if `mutex` is null.
 */
__attribute__((weak, visibility("default")))
int pthread_mutex_lock(void *mutex) EDG_LINUX_COMPAT_NOEXCEPT {
  if (!mutex) return 22;
  uintptr_t *words = (uintptr_t *)mutex;
  uintptr_t self = (uintptr_t)&edg_linux_thread_token;
  if (__atomic_load_n(&words[0], __ATOMIC_ACQUIRE) == self) {
    words[1] += 1;
    return 0;
  }
  for (;;) {
    uintptr_t expected = 0;
    if (__atomic_compare_exchange_n(&words[0], &expected, self, 1, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
      words[1] = 1;
      return 0;
    }
  }
}

/**
 * @brief ABI-portable recursive-safe `pthread_mutex_trylock` compatible with both glibc and musl.
 * @details Attempts non-blocking acquisition or recursive increment using `&edg_linux_thread_token`.
 * @param[in,out] mutex Pointer to `pthread_mutex_t` storage.
 * @return `0` on success, `16` (`EBUSY`) if contended, or `22` (`EINVAL`) if `mutex` is null.
 */
__attribute__((weak, visibility("default")))
int pthread_mutex_trylock(void *mutex) EDG_LINUX_COMPAT_NOEXCEPT {
  if (!mutex) return 22;
  uintptr_t *words = (uintptr_t *)mutex;
  uintptr_t self = (uintptr_t)&edg_linux_thread_token;
  if (__atomic_load_n(&words[0], __ATOMIC_ACQUIRE) == self) {
    words[1] += 1;
    return 0;
  }
  uintptr_t expected = 0;
  if (__atomic_compare_exchange_n(&words[0], &expected, self, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
    words[1] = 1;
    return 0;
  }
  return 16; /* EBUSY */
}

/**
 * @brief ABI-portable recursive-safe `pthread_mutex_unlock` compatible with both glibc and musl.
 * @details Decrements recursion depth or releases ownership when depth reaches zero.
 * @param[in,out] mutex Pointer to `pthread_mutex_t` storage.
 * @return `0` on success, or `22` (`EINVAL`) if `mutex` is null.
 */
__attribute__((weak, visibility("default")))
int pthread_mutex_unlock(void *mutex) EDG_LINUX_COMPAT_NOEXCEPT {
  if (!mutex) return 22;
  uintptr_t *words = (uintptr_t *)mutex;
  uintptr_t self = (uintptr_t)&edg_linux_thread_token;
  if (__atomic_load_n(&words[0], __ATOMIC_RELAXED) == self) {
    if (words[1] > 1) {
      words[1] -= 1;
      return 0;
    }
    words[1] = 0;
    __atomic_store_n(&words[0], (uintptr_t)0, __ATOMIC_RELEASE);
  }
  return 0;
}
#endif /* !EDG_LINUX_COMPAT_SANITIZER_ACTIVE */

#ifdef __cplusplus
}
#endif

#else
/** @brief Non-empty translation-unit placeholder typedef on non-Linux hosts. */
typedef int edg_linux_libc_compat_unused_t;
#endif /* defined(__linux__) */
