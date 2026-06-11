// Copyright 2010 Google Inc. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the COPYING file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS. All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
// -----------------------------------------------------------------------------
//
//  Common types + memory wrappers
//
// Author: Skal (pascal.massimino@gmail.com)

#ifndef CCGEN_WEBP_TYPES_H_
#define CCGEN_WEBP_TYPES_H_

#include <stddef.h>  // IWYU pragma: export for size_t

#ifndef _MSC_VER
#include <inttypes.h>  // IWYU pragma: export
#if defined(__cplusplus) || !defined(__STRICT_ANSI__) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#define CCGEN_WEBP_INLINE inline
#else
#define CCGEN_WEBP_INLINE
#endif
#else
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long long int uint64_t;
typedef long long int int64_t;
#define CCGEN_WEBP_INLINE __forceinline
#endif /* _MSC_VER */

#ifndef CCGEN_WEBP_NODISCARD
#if defined(CCGEN_WEBP_ENABLE_NODISCARD) && CCGEN_WEBP_ENABLE_NODISCARD
#if (defined(__cplusplus) && __cplusplus >= 201703L) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)
#define CCGEN_WEBP_NODISCARD [[nodiscard]]
#else
// gcc's __attribute__((warn_unused_result)) does not work for enums.
#if defined(__clang__) && defined(__has_attribute)
#if __has_attribute(warn_unused_result)
#define CCGEN_WEBP_NODISCARD __attribute__((warn_unused_result))
#else
#define CCGEN_WEBP_NODISCARD
#endif /* __has_attribute(warn_unused_result) */
#else
#define CCGEN_WEBP_NODISCARD
#endif /* defined(__clang__) && defined(__has_attribute) */
#endif /* (defined(__cplusplus) && __cplusplus >= 201700L) || \
          (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L) */
#else
#define CCGEN_WEBP_NODISCARD
#endif /* defined(CCGEN_WEBP_ENABLE_NODISCARD) && CCGEN_WEBP_ENABLE_NODISCARD */
#endif /* CCGEN_WEBP_NODISCARD */

#ifndef CCGEN_WEBP_EXTERN
// This explicitly marks library functions and allows for changing the
// signature for e.g., Windows DLL builds.
#if defined(_WIN32) && defined(CCGEN_WEBP_DLL)
#define CCGEN_WEBP_EXTERN __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#define CCGEN_WEBP_EXTERN extern __attribute__((visibility("default")))
#else
#define CCGEN_WEBP_EXTERN extern
#endif /* defined(_WIN32) && defined(CCGEN_WEBP_DLL) */
#endif /* CCGEN_WEBP_EXTERN */

#ifndef CCGEN_WEBP_FALLTHROUGH
#if (defined(__cplusplus) && __cplusplus >= 201703L) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L)
#define CCGEN_WEBP_FALLTHROUGH [[fallthrough]]
#else
#define CCGEN_WEBP_FALLTHROUGH
#endif
#endif /* CCGEN_WEBP_FALLTHROUGH */

// Macro to check ABI compatibility (same major revision number)
#define CCGEN_WEBP_ABI_IS_INCOMPATIBLE(a, b) (((a) >> 8) != ((b) >> 8))

#ifdef __cplusplus
extern "C" {
#endif

// Allocates 'size' bytes of memory. Returns NULL upon error. Memory
// must be deallocated by calling ccgen_WebPFree(). This function is made available
// by the core 'libwebp' library.
CCGEN_WEBP_NODISCARD CCGEN_WEBP_EXTERN void* ccgen_WebPMalloc(size_t size);

// Releases memory returned by the ccgen_WebPDecode*() functions (from decode.h).
CCGEN_WEBP_EXTERN void ccgen_WebPFree(void* ptr);

#ifdef __cplusplus
}  // extern "C"
#endif

#include <string.h>  // For memcpy and friends

#ifdef CCGEN_WEBP_SUPPORT_FBOUNDS_SAFETY

// As explained in src/utils/bounds_safety.h, the below macros are defined
// somewhat delicately to handle a three-state setup:
//
// State 1: No -fbounds-safety enabled anywhere, all macros below should act
//          as-if -fbounds-safety doesn't exist.
// State 2: A file with -fbounds-safety enabled calling into files with or
//          without -fbounds-safety.
// State 3: A file without -fbounds-safety enabled calling into files with
//          -fbounds-safety. ABI breaking annotations must stay to force a
//          build failure and force us to use non-ABI breaking annotations.
//
// Currently, we only allow non-ABI changing annotations in this file to ensure
// we don't accidentally change the ABI for public functions.

#include <ptrcheck.h>

#define CCGEN_WEBP_ASSUME_UNSAFE_INDEXABLE_ABI \
  __ptrcheck_abi_assume_unsafe_indexable()

#define CCGEN_WEBP_COUNTED_BY(x) __counted_by(x)
#define CCGEN_WEBP_COUNTED_BY_OR_NULL(x) __counted_by_or_null(x)
#define CCGEN_WEBP_SIZED_BY(x) __sized_by(x)
#define CCGEN_WEBP_SIZED_BY_OR_NULL(x) __sized_by_or_null(x)
#define CCGEN_WEBP_ENDED_BY(x) __ended_by(x)

#define CCGEN_WEBP_UNSAFE_INDEXABLE __unsafe_indexable
#define CCGEN_WEBP_SINGLE __single

#define CCGEN_WEBP_UNSAFE_FORGE_SINGLE(typ, ptr) __unsafe_forge_single(typ, ptr)

#define CCGEN_WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(typ, ptr, size) \
  __unsafe_forge_bidi_indexable(typ, ptr, size)

// Provide memcpy/memset/memmove wrappers to make migration easier.
#define CCGEN_WEBP_UNSAFE_MEMCPY(dst, src, size)                               \
  do {                                                                   \
    memcpy(CCGEN_WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(uint8_t*, dst, size),        \
           CCGEN_WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(uint8_t*, src, size), size); \
  } while (0)

#define CCGEN_WEBP_UNSAFE_MEMSET(dst, c, size)                                    \
  do {                                                                      \
    memset(CCGEN_WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(uint8_t*, dst, size), c, size); \
  } while (0)

#define CCGEN_WEBP_UNSAFE_MEMMOVE(dst, src, size)                               \
  do {                                                                    \
    memmove(CCGEN_WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(uint8_t*, dst, size),        \
            CCGEN_WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(uint8_t*, src, size), size); \
  } while (0)

#define CCGEN_WEBP_UNSAFE_MEMCMP(s1, s2, size)                       \
  memcmp(CCGEN_WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(uint8_t*, s1, size), \
         CCGEN_WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(uint8_t*, s2, size), size)

#else  // CCGEN_WEBP_SUPPORT_FBOUNDS_SAFETY

#define CCGEN_WEBP_ASSUME_UNSAFE_INDEXABLE_ABI

#define CCGEN_WEBP_COUNTED_BY(x)
#define CCGEN_WEBP_COUNTED_BY_OR_NULL(x)
#define CCGEN_WEBP_SIZED_BY(x)
#define CCGEN_WEBP_SIZED_BY_OR_NULL(x)
#define CCGEN_WEBP_ENDED_BY(x)

#define CCGEN_WEBP_UNSAFE_INDEXABLE
#define CCGEN_WEBP_SINGLE

#define CCGEN_WEBP_UNSAFE_MEMCPY(dst, src, size) memcpy(dst, src, size)
#define CCGEN_WEBP_UNSAFE_MEMSET(dst, c, size) memset(dst, c, size)
#define CCGEN_WEBP_UNSAFE_MEMMOVE(dst, src, size) memmove(dst, src, size)
#define CCGEN_WEBP_UNSAFE_MEMCMP(s1, s2, size) memcmp(s1, s2, size)

#define CCGEN_WEBP_UNSAFE_FORGE_SINGLE(typ, ptr) ((typ)(ptr))
#define CCGEN_WEBP_UNSAFE_FORGE_BIDI_INDEXABLE(typ, ptr, size) ((typ)(ptr))

#endif  // CCGEN_WEBP_SUPPORT_FBOUNDS_SAFETY

// This macro exists to indicate intentionality with self-assignments and
// silence -Wself-assign compiler warnings.
#define CCGEN_WEBP_SELF_ASSIGN(x) x = x

#endif  // CCGEN_WEBP_TYPES_H_
