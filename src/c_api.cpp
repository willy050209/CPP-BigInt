/**
 * @file c_api.cpp
 * @brief Implementation of the C-compatible ABI (extern "C") for CPP-BigInt.
 */

#include "numeric/c_api.h"
#include "numeric/BigInt.hpp"
#include "numeric/BigIntMath.hpp"

#include <new>
#include <cstring>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <limits>
#include <cstdlib>

/* ------------------------------------------------------------------------- */
/* Static Assertions: Verify C-ABI Struct Alignment and Sizes               */
/* ------------------------------------------------------------------------- */

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
static_assert(sizeof(bigint_sbo256_t) == sizeof(numeric::BasicBigInt<4>),
    "bigint_sbo256_t size must match BasicBigInt<4> exactly (64 bytes)");
static_assert(alignof(bigint_sbo256_t) == alignof(numeric::BasicBigInt<4>),
    "bigint_sbo256_t alignment must match BasicBigInt<4> (8 bytes)");

static_assert(sizeof(bigint_sbo512_t) == sizeof(numeric::BasicBigInt<8>),
    "bigint_sbo512_t size must match BasicBigInt<8> exactly (96 bytes)");
static_assert(alignof(bigint_sbo512_t) == alignof(numeric::BasicBigInt<8>),
    "bigint_sbo512_t alignment must match BasicBigInt<8> (8 bytes)");
#endif

/* ------------------------------------------------------------------------- */
/* Internal Pointer Shims                                                    */
/* ------------------------------------------------------------------------- */

static inline numeric::BigIntBase& to_base(bigint_t* p) {
    return *reinterpret_cast<numeric::BigIntBase*>(p);
}

static inline const numeric::BigIntBase& to_base(const bigint_t* p) {
    return *reinterpret_cast<const numeric::BigIntBase*>(p);
}

static inline bigint_t* to_handle(numeric::BigIntBase* p) {
    return reinterpret_cast<bigint_t*>(p);
}

static inline const bigint_t* to_handle(const numeric::BigIntBase* p) {
    return reinterpret_cast<const bigint_t*>(p);
}

/* ------------------------------------------------------------------------- */
/* Lifecycle & Memory Management (Opaque Handle Mode)                        */
/* ------------------------------------------------------------------------- */

bigint_t* bigint_create(void) {
    try {
        return to_handle(new numeric::bigint());
    } catch (...) {
        return nullptr;
    }
}

bigint_t* bigint_create_i64(int64_t val) {
    try {
        return to_handle(new numeric::bigint(val));
    } catch (...) {
        return nullptr;
    }
}

bigint_t* bigint_create_u64(uint64_t val) {
    try {
        return to_handle(new numeric::bigint(val));
    } catch (...) {
        return nullptr;
    }
}

bigint_t* bigint_create_str(const char* str, int base) {
    bigint_t* b = bigint_create();
    if (!b) return nullptr;
    bigint_status_t status = bigint_set_str(b, str, base);
    if (status != BIGINT_OK) {
        bigint_destroy(b);
        return nullptr;
    }
    return b;
}

bigint_t* bigint_clone(const bigint_t* src) {
    if (!src) return nullptr;
    try {
        auto* cloned = new numeric::bigint();
        cloned->assign_from(to_base(src));
        return to_handle(cloned);
    } catch (...) {
        return nullptr;
    }
}

void bigint_destroy(bigint_t* b) {
    if (b) {
        delete reinterpret_cast<numeric::bigint*>(b);
    }
}

/* ------------------------------------------------------------------------- */
/* Zero-Heap Stack SBO Initializers & Destructors                            */
/* ------------------------------------------------------------------------- */

bigint_t* bigint_init_sbo256(bigint_sbo256_t* storage, int64_t val) {
    if (!storage) return nullptr;
    auto* obj = reinterpret_cast<numeric::BasicBigInt<4>*>(storage);
    new (obj) numeric::BasicBigInt<4>(val);
    return to_handle(obj);
}

bigint_t* bigint_init_sbo256_u64(bigint_sbo256_t* storage, uint64_t val) {
    if (!storage) return nullptr;
    auto* obj = reinterpret_cast<numeric::BasicBigInt<4>*>(storage);
    new (obj) numeric::BasicBigInt<4>(val);
    return to_handle(obj);
}

bigint_t* bigint_init_sbo256_str(bigint_sbo256_t* storage, const char* str, int base) {
    if (!storage || !str) return nullptr;
    bigint_t* b = bigint_init_sbo256(storage, 0);
    if (!b) return nullptr;
    bigint_status_t st = bigint_set_str(b, str, base);
    if (st != BIGINT_OK) {
        bigint_destroy_sbo256(storage);
        return nullptr;
    }
    return b;
}

void bigint_destroy_sbo256(bigint_sbo256_t* storage) {
    if (storage) {
        reinterpret_cast<numeric::BasicBigInt<4>*>(storage)->~BasicBigInt();
    }
}

bigint_t* bigint_init_sbo512(bigint_sbo512_t* storage, int64_t val) {
    if (!storage) return nullptr;
    auto* obj = reinterpret_cast<numeric::BasicBigInt<8>*>(storage);
    new (obj) numeric::BasicBigInt<8>(val);
    return to_handle(obj);
}

bigint_t* bigint_init_sbo512_u64(bigint_sbo512_t* storage, uint64_t val) {
    if (!storage) return nullptr;
    auto* obj = reinterpret_cast<numeric::BasicBigInt<8>*>(storage);
    new (obj) numeric::BasicBigInt<8>(val);
    return to_handle(obj);
}

bigint_t* bigint_init_sbo512_str(bigint_sbo512_t* storage, const char* str, int base) {
    if (!storage || !str) return nullptr;
    bigint_t* b = bigint_init_sbo512(storage, 0);
    if (!b) return nullptr;
    bigint_status_t st = bigint_set_str(b, str, base);
    if (st != BIGINT_OK) {
        bigint_destroy_sbo512(storage);
        return nullptr;
    }
    return b;
}

void bigint_destroy_sbo512(bigint_sbo512_t* storage) {
    if (storage) {
        reinterpret_cast<numeric::BasicBigInt<8>*>(storage)->~BasicBigInt();
    }
}

/* ------------------------------------------------------------------------- */
/* Assignment & Mutation                                                     */
/* ------------------------------------------------------------------------- */

bigint_status_t bigint_assign(bigint_t* dst, const bigint_t* src) {
    if (!dst || !src) return BIGINT_ERR_NULL_PTR;
    if (dst == src) return BIGINT_OK;
    try {
        to_base(dst).assign_from(to_base(src));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_set_i64(bigint_t* dst, int64_t val) {
    if (!dst) return BIGINT_ERR_NULL_PTR;
    try {
        int8_t sign = (val > 0) ? 1 : ((val < 0) ? -1 : 0);
        uint64_t mag = (val < 0) ? static_cast<uint64_t>(-(val + 1)) + 1ULL : static_cast<uint64_t>(val);
        to_base(dst).set_uint64(mag, sign);
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_set_u64(bigint_t* dst, uint64_t val) {
    if (!dst) return BIGINT_ERR_NULL_PTR;
    try {
        to_base(dst).set_uint64(val, val != 0 ? 1 : 0);
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_set_str(bigint_t* dst, const char* str, int base) {
    if (!dst || !str) return BIGINT_ERR_NULL_PTR;
    if (base != 0 && (base < 2 || base > 36)) return BIGINT_ERR_INVALID_ARG;

    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') ++str;
    if (*str == '\0') return BIGINT_ERR_INVALID_ARG;

    int8_t sign = 1;
    if (*str == '-') {
        sign = -1;
        ++str;
    } else if (*str == '+') {
        ++str;
    }
    if (*str == '\0') return BIGINT_ERR_INVALID_ARG;

    if (base == 0) {
        if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
            base = 16;
            str += 2;
        } else if (str[0] == '0' && (str[1] == 'b' || str[1] == 'B')) {
            base = 2;
            str += 2;
        } else if (str[0] == '0' && (str[1] == 'o' || str[1] == 'O')) {
            base = 8;
            str += 2;
        } else {
            base = 10;
        }
    }

    if (base == 10) {
        try {
            numeric::bigint tmp;
            std::string s_val = (sign < 0) ? ("-" + std::string(str)) : std::string(str);
            numeric::detail::BigIntCore::from_string(tmp, numeric::string_view(s_val.data(), s_val.size()));
            to_base(dst).assign_from(tmp);
            return BIGINT_OK;
        } catch (...) {
            return BIGINT_ERR_INVALID_ARG;
        }
    }

    // General radix 2..36 parsing
    try {
        numeric::bigint val(0);
        numeric::bigint b_base(base);
        bool any_digit = false;
        while (*str) {
            char c = *str++;
            int d = -1;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
            else if (c == '_') continue;
            else return BIGINT_ERR_INVALID_ARG;

            if (d >= base) return BIGINT_ERR_INVALID_ARG;
            val *= b_base;
            val += d;
            any_digit = true;
        }
        if (!any_digit) return BIGINT_ERR_INVALID_ARG;
        if (sign < 0 && val != 0) val.negate();
        to_base(dst).assign_from(val);
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

/* ------------------------------------------------------------------------- */
/* Basic Arithmetic (out = a OP b)                                           */
/* ------------------------------------------------------------------------- */

bigint_status_t bigint_add(bigint_t* out, const bigint_t* a, const bigint_t* b) {
    if (!out || !a || !b) return BIGINT_ERR_NULL_PTR;
    try {
        numeric::detail::BigIntCore::add_signed(to_base(out), to_base(a), to_base(b));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_sub(bigint_t* out, const bigint_t* a, const bigint_t* b) {
    if (!out || !a || !b) return BIGINT_ERR_NULL_PTR;
    try {
        numeric::detail::BigIntCore::sub_signed(to_base(out), to_base(a), to_base(b));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_mul(bigint_t* out, const bigint_t* a, const bigint_t* b) {
    if (!out || !a || !b) return BIGINT_ERR_NULL_PTR;
    try {
        if (out == a || out == b) {
            numeric::bigint tmp;
            numeric::detail::BigIntCore::mul_signed(tmp, to_base(a), to_base(b));
            to_base(out).move_from(std::move(tmp));
        } else {
            numeric::detail::BigIntCore::mul_signed(to_base(out), to_base(a), to_base(b));
        }
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_div(bigint_t* out, const bigint_t* a, const bigint_t* b) {
    if (!out || !a || !b) return BIGINT_ERR_NULL_PTR;
    if (to_base(b).m_size == 0 || to_base(b).m_sign == 0) return BIGINT_ERR_DIV_BY_ZERO;
    try {
        if (out == a || out == b) {
            numeric::bigint tmp;
            numeric::detail::BigIntCore::div_q_signed(tmp, to_base(a), to_base(b));
            to_base(out).move_from(std::move(tmp));
        } else {
            numeric::detail::BigIntCore::div_q_signed(to_base(out), to_base(a), to_base(b));
        }
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_mod(bigint_t* out, const bigint_t* a, const bigint_t* b) {
    if (!out || !a || !b) return BIGINT_ERR_NULL_PTR;
    if (to_base(b).m_size == 0 || to_base(b).m_sign == 0) return BIGINT_ERR_DIV_BY_ZERO;
    try {
        if (out == a || out == b) {
            numeric::bigint tmp;
            numeric::detail::BigIntCore::div_r_signed(tmp, to_base(a), to_base(b));
            to_base(out).move_from(std::move(tmp));
        } else {
            numeric::detail::BigIntCore::div_r_signed(to_base(out), to_base(a), to_base(b));
        }
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_div_rem(bigint_t* q, bigint_t* r, const bigint_t* a, const bigint_t* b) {
    if (!q || !r || !a || !b) return BIGINT_ERR_NULL_PTR;
    if (to_base(b).m_size == 0 || to_base(b).m_sign == 0) return BIGINT_ERR_DIV_BY_ZERO;
    try {
        numeric::bigint tmp_q, tmp_r;
        numeric::detail::BigIntCore::div_q_signed(tmp_q, to_base(a), to_base(b));
        numeric::detail::BigIntCore::div_r_signed(tmp_r, to_base(a), to_base(b));
        to_base(q).move_from(std::move(tmp_q));
        to_base(r).move_from(std::move(tmp_r));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_neg(bigint_t* out, const bigint_t* a) {
    if (!out || !a) return BIGINT_ERR_NULL_PTR;
    try {
        if (out != a) {
            to_base(out).assign_from(to_base(a));
        }
        to_base(out).negate();
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_abs(bigint_t* out, const bigint_t* a) {
    if (!out || !a) return BIGINT_ERR_NULL_PTR;
    try {
        if (out != a) {
            to_base(out).assign_from(to_base(a));
        }
        if (to_base(out).m_sign < 0) {
            to_base(out).negate();
        }
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

/* ------------------------------------------------------------------------- */
/* In-Place Arithmetic (a = a OP b)                                          */
/* ------------------------------------------------------------------------- */

bigint_status_t bigint_add_inplace(bigint_t* a, const bigint_t* b) {
    return bigint_add(a, a, b);
}

bigint_status_t bigint_sub_inplace(bigint_t* a, const bigint_t* b) {
    return bigint_sub(a, a, b);
}

bigint_status_t bigint_mul_inplace(bigint_t* a, const bigint_t* b) {
    return bigint_mul(a, a, b);
}

bigint_status_t bigint_div_inplace(bigint_t* a, const bigint_t* b) {
    return bigint_div(a, a, b);
}

bigint_status_t bigint_mod_inplace(bigint_t* a, const bigint_t* b) {
    return bigint_mod(a, a, b);
}

/* ------------------------------------------------------------------------- */
/* Bitwise & Shift Operations                                                */
/* ------------------------------------------------------------------------- */

bigint_status_t bigint_and(bigint_t* out, const bigint_t* a, const bigint_t* b) {
    if (!out || !a || !b) return BIGINT_ERR_NULL_PTR;
    try {
        numeric::bigint tmp;
        numeric::detail::BigIntCore::bitwise_and(tmp, to_base(a), to_base(b));
        to_base(out).move_from(std::move(tmp));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_or(bigint_t* out, const bigint_t* a, const bigint_t* b) {
    if (!out || !a || !b) return BIGINT_ERR_NULL_PTR;
    try {
        numeric::bigint tmp;
        numeric::detail::BigIntCore::bitwise_or(tmp, to_base(a), to_base(b));
        to_base(out).move_from(std::move(tmp));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_xor(bigint_t* out, const bigint_t* a, const bigint_t* b) {
    if (!out || !a || !b) return BIGINT_ERR_NULL_PTR;
    try {
        numeric::bigint tmp;
        numeric::detail::BigIntCore::bitwise_xor(tmp, to_base(a), to_base(b));
        to_base(out).move_from(std::move(tmp));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_not(bigint_t* out, const bigint_t* a) {
    if (!out || !a) return BIGINT_ERR_NULL_PTR;
    try {
        numeric::bigint one(1);
        numeric::bigint tmp;
        numeric::detail::BigIntCore::add_signed(tmp, to_base(a), one);
        tmp.negate();
        to_base(out).move_from(std::move(tmp));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_shl(bigint_t* out, const bigint_t* a, size_t shift) {
    if (!out || !a) return BIGINT_ERR_NULL_PTR;
    try {
        numeric::bigint tmp;
        numeric::detail::BigIntCore::shift_left(tmp, to_base(a), shift);
        to_base(out).move_from(std::move(tmp));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_shr(bigint_t* out, const bigint_t* a, size_t shift) {
    if (!out || !a) return BIGINT_ERR_NULL_PTR;
    try {
        numeric::bigint tmp;
        numeric::detail::BigIntCore::shift_right(tmp, to_base(a), shift);
        to_base(out).move_from(std::move(tmp));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

/* ------------------------------------------------------------------------- */
/* Comparisons                                                               */
/* ------------------------------------------------------------------------- */

int bigint_cmp(const bigint_t* a, const bigint_t* b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    const auto& lhs = to_base(a);
    const auto& rhs = to_base(b);
    if (lhs.m_sign < rhs.m_sign) return -1;
    if (lhs.m_sign > rhs.m_sign) return 1;
    if (lhs.m_sign == 0) return 0;
    int cmp = numeric::detail::BigIntCore::compare_unsigned(lhs.m_data, lhs.m_size, rhs.m_data, rhs.m_size);
    return (lhs.m_sign > 0) ? cmp : -cmp;
}

int bigint_cmp_i64(const bigint_t* a, int64_t b) {
    if (!a) return (b > 0) ? -1 : ((b < 0) ? 1 : 0);
    numeric::bigint b_val(b);
    return bigint_cmp(a, to_handle(&b_val));
}

int bigint_is_zero(const bigint_t* a) {
    if (!a) return 1;
    return (to_base(a).m_sign == 0 || to_base(a).m_size == 0) ? 1 : 0;
}

int bigint_is_negative(const bigint_t* a) {
    if (!a) return 0;
    return (to_base(a).m_sign < 0 && to_base(a).m_size > 0) ? 1 : 0;
}

int bigint_sign(const bigint_t* a) {
    if (!a) return 0;
    if (to_base(a).m_size == 0) return 0;
    return to_base(a).m_sign;
}

/* ------------------------------------------------------------------------- */
/* High-Precision Math Functions                                             */
/* ------------------------------------------------------------------------- */

bigint_status_t bigint_pow(bigint_t* out, const bigint_t* base, uint32_t exp) {
    if (!out || !base) return BIGINT_ERR_NULL_PTR;
    try {
        numeric::bigint b;
        b.assign_from(to_base(base));
        numeric::bigint res = numeric::pow(b, exp);
        to_base(out).move_from(std::move(res));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_gcd(bigint_t* out, const bigint_t* a, const bigint_t* b) {
    if (!out || !a || !b) return BIGINT_ERR_NULL_PTR;
    try {
        numeric::bigint ba, bb;
        ba.assign_from(to_base(a));
        bb.assign_from(to_base(b));
        numeric::bigint res = numeric::gcd(ba, bb);
        to_base(out).move_from(std::move(res));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_lcm(bigint_t* out, const bigint_t* a, const bigint_t* b) {
    if (!out || !a || !b) return BIGINT_ERR_NULL_PTR;
    try {
        numeric::bigint ba, bb;
        ba.assign_from(to_base(a));
        bb.assign_from(to_base(b));
        numeric::bigint res = numeric::lcm(ba, bb);
        to_base(out).move_from(std::move(res));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_isqrt(bigint_t* out, const bigint_t* a) {
    if (!out || !a) return BIGINT_ERR_NULL_PTR;
    if (to_base(a).m_sign < 0) return BIGINT_ERR_INVALID_ARG;
    try {
        numeric::bigint ba;
        ba.assign_from(to_base(a));
        numeric::bigint res = numeric::isqrt(ba);
        to_base(out).move_from(std::move(res));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

/* ------------------------------------------------------------------------- */
/* Conversions & String Formatting                                           */
/* ------------------------------------------------------------------------- */

int64_t bigint_to_i64(const bigint_t* a, bigint_status_t* status) {
    if (!a) {
        if (status) *status = BIGINT_ERR_NULL_PTR;
        return 0;
    }
    const auto& b = to_base(a);
    if (b.m_size == 0 || b.m_sign == 0) {
        if (status) *status = BIGINT_OK;
        return 0;
    }
    if (b.m_size > 1) {
        if (status) *status = BIGINT_ERR_OVERFLOW;
        return (b.m_sign < 0) ? std::numeric_limits<int64_t>::min() : std::numeric_limits<int64_t>::max();
    }
    uint64_t mag = b.m_data[0];
    if (b.m_sign > 0) {
        if (mag > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            if (status) *status = BIGINT_ERR_OVERFLOW;
            return std::numeric_limits<int64_t>::max();
        }
        if (status) *status = BIGINT_OK;
        return static_cast<int64_t>(mag);
    } else {
        if (mag > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1ULL) {
            if (status) *status = BIGINT_ERR_OVERFLOW;
            return std::numeric_limits<int64_t>::min();
        }
        if (status) *status = BIGINT_OK;
        return -static_cast<int64_t>(mag);
    }
}

uint64_t bigint_to_u64(const bigint_t* a, bigint_status_t* status) {
    if (!a) {
        if (status) *status = BIGINT_ERR_NULL_PTR;
        return 0;
    }
    const auto& b = to_base(a);
    if (b.m_size == 0 || b.m_sign == 0) {
        if (status) *status = BIGINT_OK;
        return 0;
    }
    if (b.m_sign < 0 || b.m_size > 1) {
        if (status) *status = BIGINT_ERR_OVERFLOW;
        return (b.m_sign < 0) ? 0 : std::numeric_limits<uint64_t>::max();
    }
    if (status) *status = BIGINT_OK;
    return b.m_data[0];
}

static std::string format_bigint_radix(const numeric::BigIntBase& a, int base) {
    if (a.m_size == 0 || a.m_sign == 0) return "0";
    if (base == 10) {
        return numeric::detail::BigIntCore::to_string(a);
    }
    numeric::bigint val;
    val.assign_from(a);
    bool neg = (val.sign() < 0);
    if (neg) val.negate();

    std::string digits;
    const char* charset = "0123456789abcdefghijklmnopqrstuvwxyz";
    numeric::bigint b_base(base);
    while (val > 0) {
        numeric::bigint q, r;
        numeric::detail::BigIntCore::div_q_signed(q, val, b_base);
        numeric::detail::BigIntCore::div_r_signed(r, val, b_base);
        int rem = static_cast<int>(r);
        digits.push_back(charset[rem]);
        val = std::move(q);
    }
    if (neg) digits.push_back('-');
    std::reverse(digits.begin(), digits.end());
    return digits;
}

size_t bigint_to_str_len(const bigint_t* a, int base) {
    if (!a || base < 2 || base > 36) return 0;
    try {
        std::string s = format_bigint_radix(to_base(a), base);
        return s.size() + 1;
    } catch (...) {
        return 0;
    }
}

bigint_status_t bigint_to_str(const bigint_t* a, char* buf, size_t buf_len, int base) {
    if (!a || !buf) return BIGINT_ERR_NULL_PTR;
    if (base < 2 || base > 36) return BIGINT_ERR_INVALID_ARG;
    try {
        std::string s = format_bigint_radix(to_base(a), base);
        if (buf_len <= s.size()) {
            return BIGINT_ERR_BUFFER_TOO_SMALL;
        }
        std::memcpy(buf, s.c_str(), s.size() + 1);
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

char* bigint_to_str_alloc(const bigint_t* a, int base) {
    if (!a || base < 2 || base > 36) return nullptr;
    try {
        std::string s = format_bigint_radix(to_base(a), base);
        char* mem = static_cast<char*>(std::malloc(s.size() + 1));
        if (mem) {
            std::memcpy(mem, s.c_str(), s.size() + 1);
        }
        return mem;
    } catch (...) {
        return nullptr;
    }
}

void bigint_free_str(char* str) {
    std::free(str);
}

/* ------------------------------------------------------------------------- */
/* Utilities & Introspection                                                 */
/* ------------------------------------------------------------------------- */

size_t bigint_bit_length(const bigint_t* a) {
    if (!a) return 0;
    const auto& b = to_base(a);
    if (b.m_size == 0 || b.m_sign == 0) return 0;
    uint64_t high = b.m_data[b.m_size - 1];
    int bits = 0;
    while (high > 0) {
        bits++;
        high >>= 1;
    }
    return (b.m_size - 1) * 64 + static_cast<size_t>(bits);
}

size_t bigint_limb_count(const bigint_t* a) {
    if (!a) return 0;
    return to_base(a).m_size;
}

int bigint_is_sbo(const bigint_t* a) {
    if (!a) return 0;
    return to_base(a).m_is_sbo ? 1 : 0;
}

const char* bigint_version(void) {
    return "1.0.0";
}

const char* bigint_status_message(bigint_status_t status) {
    switch (status) {
        case BIGINT_OK:                   return "Success";
        case BIGINT_ERR_INVALID_ARG:      return "Invalid argument";
        case BIGINT_ERR_DIV_BY_ZERO:      return "Division by zero";
        case BIGINT_ERR_BUFFER_TOO_SMALL: return "Buffer too small";
        case BIGINT_ERR_NULL_PTR:         return "Null pointer";
        case BIGINT_ERR_OUT_OF_MEMORY:    return "Out of memory";
        case BIGINT_ERR_OVERFLOW:         return "Overflow";
        default:                          return "Unknown status";
    }
}
