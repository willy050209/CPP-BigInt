#ifndef NUMERIC_C_API_H
#define NUMERIC_C_API_H

/**
 * @file c_api.h
 * @brief C-compatible ABI (extern "C") interface for CPP-BigInt.
 * @details Enables consumption by pure C, Python (ctypes/cffi), Rust (bindgen),
 *          .NET ([DllImport]/[LibraryImport]), and Go (cgo).
 *          Provides both heap-allocated opaque handles (bigint_t*) and
 *          zero-allocation stack-based SBO storage (bigint_sbo256_t).
 */

#include <stdint.h>
#include <stddef.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(BIGINT_C_BUILD_DLL)
#    define BIGINT_C_API __declspec(dllexport)
#  elif defined(BIGINT_C_SHARED)
#    define BIGINT_C_API __declspec(dllimport)
#  else
#    define BIGINT_C_API
#  endif
#else
#  if defined(BIGINT_C_BUILD_DLL) && defined(__GNUC__) && __GNUC__ >= 4
#    define BIGINT_C_API __attribute__((visibility("default")))
#  else
#    define BIGINT_C_API
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- */
/* Error Codes & Types                                                       */
/* ------------------------------------------------------------------------- */

typedef enum bigint_status_t {
    BIGINT_OK                   = 0,
    BIGINT_ERR_INVALID_ARG      = 1,
    BIGINT_ERR_DIV_BY_ZERO      = 2,
    BIGINT_ERR_BUFFER_TOO_SMALL = 3,
    BIGINT_ERR_NULL_PTR         = 4,
    BIGINT_ERR_OUT_OF_MEMORY    = 5,
    BIGINT_ERR_OVERFLOW         = 6
} bigint_status_t;

/**
 * @brief Opaque BigInt handle representing an arbitrary-precision integer.
 */
typedef struct bigint_t bigint_t;

/**
 * @brief 256-bit (4 limbs) Stack SBO storage buffer.
 * Exactly 64 bytes and 8-byte aligned on 64-bit platforms.
 * Matches sizeof(numeric::BasicBigInt<4>).
 */
typedef struct bigint_sbo256_t {
    uint64_t _raw[8];
} bigint_sbo256_t;

/**
 * @brief 512-bit (8 limbs) Stack SBO storage buffer.
 * Exactly 96 bytes and 8-byte aligned on 64-bit platforms.
 * Matches sizeof(numeric::BasicBigInt<8>).
 */
typedef struct bigint_sbo512_t {
    uint64_t _raw[12];
} bigint_sbo512_t;

/* ------------------------------------------------------------------------- */
/* Lifecycle & Memory Management (Opaque Handle Mode)                        */
/* ------------------------------------------------------------------------- */

BIGINT_C_API bigint_t* bigint_create(void);
BIGINT_C_API bigint_t* bigint_create_i64(int64_t val);
BIGINT_C_API bigint_t* bigint_create_u64(uint64_t val);
BIGINT_C_API bigint_t* bigint_create_str(const char* str, int base);
BIGINT_C_API bigint_t* bigint_clone(const bigint_t* src);
BIGINT_C_API void      bigint_destroy(bigint_t* b);

/* ------------------------------------------------------------------------- */
/* Zero-Heap Stack SBO Initializers & Destructors                            */
/* ------------------------------------------------------------------------- */

BIGINT_C_API bigint_t* bigint_init_sbo256(bigint_sbo256_t* storage, int64_t val);
BIGINT_C_API bigint_t* bigint_init_sbo256_u64(bigint_sbo256_t* storage, uint64_t val);
BIGINT_C_API bigint_t* bigint_init_sbo256_str(bigint_sbo256_t* storage, const char* str, int base);
BIGINT_C_API void      bigint_destroy_sbo256(bigint_sbo256_t* storage);

BIGINT_C_API bigint_t* bigint_init_sbo512(bigint_sbo512_t* storage, int64_t val);
BIGINT_C_API bigint_t* bigint_init_sbo512_u64(bigint_sbo512_t* storage, uint64_t val);
BIGINT_C_API bigint_t* bigint_init_sbo512_str(bigint_sbo512_t* storage, const char* str, int base);
BIGINT_C_API void      bigint_destroy_sbo512(bigint_sbo512_t* storage);

/* ------------------------------------------------------------------------- */
/* Assignment & Mutation                                                     */
/* ------------------------------------------------------------------------- */

BIGINT_C_API bigint_status_t bigint_assign(bigint_t* dst, const bigint_t* src);
BIGINT_C_API bigint_status_t bigint_set_i64(bigint_t* dst, int64_t val);
BIGINT_C_API bigint_status_t bigint_set_u64(bigint_t* dst, uint64_t val);
BIGINT_C_API bigint_status_t bigint_set_str(bigint_t* dst, const char* str, int base);

/* ------------------------------------------------------------------------- */
/* Basic Arithmetic (out = a OP b)                                           */
/* ------------------------------------------------------------------------- */

BIGINT_C_API bigint_status_t bigint_add(bigint_t* out, const bigint_t* a, const bigint_t* b);
BIGINT_C_API bigint_status_t bigint_sub(bigint_t* out, const bigint_t* a, const bigint_t* b);
BIGINT_C_API bigint_status_t bigint_mul(bigint_t* out, const bigint_t* a, const bigint_t* b);
BIGINT_C_API bigint_status_t bigint_div(bigint_t* out, const bigint_t* a, const bigint_t* b);
BIGINT_C_API bigint_status_t bigint_mod(bigint_t* out, const bigint_t* a, const bigint_t* b);
BIGINT_C_API bigint_status_t bigint_div_rem(bigint_t* q, bigint_t* r, const bigint_t* a, const bigint_t* b);
BIGINT_C_API bigint_status_t bigint_neg(bigint_t* out, const bigint_t* a);
BIGINT_C_API bigint_status_t bigint_abs(bigint_t* out, const bigint_t* a);

/* ------------------------------------------------------------------------- */
/* In-Place Arithmetic (a = a OP b)                                          */
/* ------------------------------------------------------------------------- */

BIGINT_C_API bigint_status_t bigint_add_inplace(bigint_t* a, const bigint_t* b);
BIGINT_C_API bigint_status_t bigint_sub_inplace(bigint_t* a, const bigint_t* b);
BIGINT_C_API bigint_status_t bigint_mul_inplace(bigint_t* a, const bigint_t* b);
BIGINT_C_API bigint_status_t bigint_div_inplace(bigint_t* a, const bigint_t* b);
BIGINT_C_API bigint_status_t bigint_mod_inplace(bigint_t* a, const bigint_t* b);

/* ------------------------------------------------------------------------- */
/* Bitwise & Shift Operations                                                */
/* ------------------------------------------------------------------------- */

BIGINT_C_API bigint_status_t bigint_and(bigint_t* out, const bigint_t* a, const bigint_t* b);
BIGINT_C_API bigint_status_t bigint_or(bigint_t* out, const bigint_t* a, const bigint_t* b);
BIGINT_C_API bigint_status_t bigint_xor(bigint_t* out, const bigint_t* a, const bigint_t* b);
BIGINT_C_API bigint_status_t bigint_not(bigint_t* out, const bigint_t* a);
BIGINT_C_API bigint_status_t bigint_shl(bigint_t* out, const bigint_t* a, size_t shift);
BIGINT_C_API bigint_status_t bigint_shr(bigint_t* out, const bigint_t* a, size_t shift);

/* ------------------------------------------------------------------------- */
/* Comparisons                                                               */
/* ------------------------------------------------------------------------- */

BIGINT_C_API int bigint_cmp(const bigint_t* a, const bigint_t* b);     /* -1, 0, 1 */
BIGINT_C_API int bigint_cmp_i64(const bigint_t* a, int64_t b);        /* -1, 0, 1 */
BIGINT_C_API int bigint_is_zero(const bigint_t* a);                   /* 1 if 0, else 0 */
BIGINT_C_API int bigint_is_negative(const bigint_t* a);               /* 1 if < 0, else 0 */
BIGINT_C_API int bigint_sign(const bigint_t* a);                      /* -1, 0, 1 */

/* ------------------------------------------------------------------------- */
/* High-Precision Math Functions                                             */
/* ------------------------------------------------------------------------- */

BIGINT_C_API bigint_status_t bigint_pow(bigint_t* out, const bigint_t* base, uint32_t exp);
BIGINT_C_API bigint_status_t bigint_gcd(bigint_t* out, const bigint_t* a, const bigint_t* b);
BIGINT_C_API bigint_status_t bigint_lcm(bigint_t* out, const bigint_t* a, const bigint_t* b);
BIGINT_C_API bigint_status_t bigint_isqrt(bigint_t* out, const bigint_t* a);

/* ------------------------------------------------------------------------- */
/* Conversions & String Formatting                                           */
/* ------------------------------------------------------------------------- */

BIGINT_C_API int64_t  bigint_to_i64(const bigint_t* a, bigint_status_t* status);
BIGINT_C_API uint64_t bigint_to_u64(const bigint_t* a, bigint_status_t* status);
BIGINT_C_API size_t   bigint_to_str_len(const bigint_t* a, int base);
BIGINT_C_API bigint_status_t bigint_to_str(const bigint_t* a, char* buf, size_t buf_len, int base);
BIGINT_C_API char*    bigint_to_str_alloc(const bigint_t* a, int base);
BIGINT_C_API void     bigint_free_str(char* str);

/* ------------------------------------------------------------------------- */
/* Utilities & Introspection                                                 */
/* ------------------------------------------------------------------------- */

BIGINT_C_API size_t      bigint_bit_length(const bigint_t* a);
BIGINT_C_API size_t      bigint_limb_count(const bigint_t* a);
BIGINT_C_API int         bigint_is_sbo(const bigint_t* a);
BIGINT_C_API const char* bigint_version(void);
BIGINT_C_API const char* bigint_status_message(bigint_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* NUMERIC_C_API_H */
