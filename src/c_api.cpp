// filepath: src/c_api.cpp
/**
 * @file c_api.cpp
 * @brief Implementation of the C-compatible ABI (`extern "C"`) for CPP-BigInt.
 *        CPP-BigInt 的 C 相容 ABI (`extern "C"`) 實作檔。
 */

#include "numeric/c_api.h"
#include "numeric/BigInt.hpp"
#include "numeric/BigIntMath.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

/* ------------------------------------------------------------------------- */
/* Static Assertions: Verify C-ABI Struct Alignment and Sizes                */
/* ------------------------------------------------------------------------- */

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || \
    (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
static_assert(sizeof(bigint_sbo256_t) == sizeof(numeric::BasicBigInt<4>),
              "bigint_sbo256_t size must match BasicBigInt<4> exactly (64 bytes)");
static_assert(alignof(bigint_sbo256_t) == alignof(numeric::BasicBigInt<4>),
              "bigint_sbo256_t alignment must match BasicBigInt<4> (8 bytes)");

static_assert(sizeof(bigint_sbo512_t) == sizeof(numeric::BasicBigInt<8>),
              "bigint_sbo512_t size must match BasicBigInt<8> exactly (96 bytes)");
static_assert(alignof(bigint_sbo512_t) == alignof(numeric::BasicBigInt<8>),
              "bigint_sbo512_t alignment must match BasicBigInt<8> (8 bytes)");
#endif

namespace {

/* ------------------------------------------------------------------------- */
/* Internal Pointer Shims (Zero-Cost ABI Boundary Conversions)               */
/* ------------------------------------------------------------------------- */

[[nodiscard]] inline auto to_base(bigint_t* p) noexcept -> numeric::BigIntBase& {
    return *static_cast<numeric::BigIntBase*>(static_cast<void*>(p));
}

[[nodiscard]] inline auto to_base(const bigint_t* p) noexcept -> const numeric::BigIntBase& {
    return *static_cast<const numeric::BigIntBase*>(static_cast<const void*>(p));
}

[[nodiscard]] inline auto to_handle(numeric::BigIntBase* p) noexcept -> bigint_t* {
    return static_cast<bigint_t*>(static_cast<void*>(p));
}

[[nodiscard]] inline auto to_handle(const numeric::BigIntBase* p) noexcept -> const bigint_t* {
    return static_cast<const bigint_t*>(static_cast<const void*>(p));
}

[[nodiscard]] inline auto to_sbo256_obj(bigint_sbo256_t* storage) noexcept -> numeric::BasicBigInt<4>* {
    return static_cast<numeric::BasicBigInt<4>*>(static_cast<void*>(storage));
}

[[nodiscard]] inline auto to_sbo512_obj(bigint_sbo512_t* storage) noexcept -> numeric::BasicBigInt<8>* {
    return static_cast<numeric::BasicBigInt<8>*>(static_cast<void*>(storage));
}

[[nodiscard]] auto format_bigint_radix(const numeric::BigIntBase& a,
                                       std::int32_t base) -> std::string {
    if (a.m_size == 0U || a.m_sign == 0) {
        return "0";
    }
    if (base == 10) {
        return numeric::detail::BigIntCore::to_string(a);
    }

    numeric::bigint val;
    val.assign_from(a);
    const bool neg{val.sign() < 0};
    if (neg) {
        val.negate();
    }

    std::string digits;
    constexpr std::string_view kCharset{"0123456789abcdefghijklmnopqrstuvwxyz"};
    const numeric::bigint b_base{base};
    while (val > 0) {
        numeric::bigint q;
        numeric::bigint r;
        numeric::detail::BigIntCore::div_q_signed(q, val, b_base);
        numeric::detail::BigIntCore::div_r_signed(r, val, b_base);
        const std::size_t rem{static_cast<std::size_t>(static_cast<std::int32_t>(r))};
        digits.push_back(kCharset[rem]);
        val = std::move(q);
    }
    if (neg) {
        digits.push_back('-');
    }
    std::ranges::reverse(digits);
    return digits;
}

} // namespace

/* ------------------------------------------------------------------------- */
/* Lifecycle & Memory Management (Opaque Handle Mode)                        */
/* ------------------------------------------------------------------------- */

bigint_t* bigint_create(void) {
    try {
        auto obj = std::make_unique<numeric::bigint>();
        return to_handle(obj.release());
    } catch (...) {
        return nullptr;
    }
}

bigint_t* bigint_create_i64(std::int64_t val) {
    try {
        auto obj = std::make_unique<numeric::bigint>(val);
        return to_handle(obj.release());
    } catch (...) {
        return nullptr;
    }
}

bigint_t* bigint_create_u64(std::uint64_t val) {
    try {
        auto obj = std::make_unique<numeric::bigint>(val);
        return to_handle(obj.release());
    } catch (...) {
        return nullptr;
    }
}

bigint_t* bigint_create_str(const char* str, int base) {
    bigint_t* b{bigint_create()};
    if (b == nullptr) {
        return nullptr;
    }
    const bigint_status_t status{bigint_set_str(b, str, base)};
    if (status != BIGINT_OK) {
        bigint_destroy(b);
        return nullptr;
    }
    return b;
}

bigint_t* bigint_clone(const bigint_t* src) {
    if (src == nullptr) {
        return nullptr;
    }
    try {
        auto cloned = std::make_unique<numeric::bigint>();
        cloned->assign_from(to_base(src));
        return to_handle(cloned.release());
    } catch (...) {
        return nullptr;
    }
}

void bigint_destroy(bigint_t* b) {
    if (b != nullptr) {
        auto* base_ptr = static_cast<numeric::BigIntBase*>(static_cast<void*>(b));
        const std::unique_ptr<numeric::bigint> guard{static_cast<numeric::bigint*>(base_ptr)};
    }
}

/* ------------------------------------------------------------------------- */
/* Zero-Heap Stack SBO Initializers & Destructors                            */
/* ------------------------------------------------------------------------- */

bigint_t* bigint_init_sbo256(bigint_sbo256_t* storage, std::int64_t val) {
    if (storage == nullptr) {
        return nullptr;
    }
    auto* const obj = std::construct_at(to_sbo256_obj(storage), val);
    return to_handle(obj);
}

bigint_t* bigint_init_sbo256_u64(bigint_sbo256_t* storage, std::uint64_t val) {
    if (storage == nullptr) {
        return nullptr;
    }
    auto* const obj = std::construct_at(to_sbo256_obj(storage), val);
    return to_handle(obj);
}

bigint_t* bigint_init_sbo256_str(bigint_sbo256_t* storage, const char* str, int base) {
    if (storage == nullptr || str == nullptr) {
        return nullptr;
    }
    bigint_t* b{bigint_init_sbo256(storage, 0)};
    if (b == nullptr) {
        return nullptr;
    }
    const bigint_status_t st{bigint_set_str(b, str, base)};
    if (st != BIGINT_OK) {
        bigint_destroy_sbo256(storage);
        return nullptr;
    }
    return b;
}

void bigint_destroy_sbo256(bigint_sbo256_t* storage) {
    if (storage != nullptr) {
        std::destroy_at(std::launder(to_sbo256_obj(storage)));
    }
}

bigint_t* bigint_init_sbo512(bigint_sbo512_t* storage, std::int64_t val) {
    if (storage == nullptr) {
        return nullptr;
    }
    auto* const obj = std::construct_at(to_sbo512_obj(storage), val);
    return to_handle(obj);
}

bigint_t* bigint_init_sbo512_u64(bigint_sbo512_t* storage, std::uint64_t val) {
    if (storage == nullptr) {
        return nullptr;
    }
    auto* const obj = std::construct_at(to_sbo512_obj(storage), val);
    return to_handle(obj);
}

bigint_t* bigint_init_sbo512_str(bigint_sbo512_t* storage, const char* str, int base) {
    if (storage == nullptr || str == nullptr) {
        return nullptr;
    }
    bigint_t* b{bigint_init_sbo512(storage, 0)};
    if (b == nullptr) {
        return nullptr;
    }
    const bigint_status_t st{bigint_set_str(b, str, base)};
    if (st != BIGINT_OK) {
        bigint_destroy_sbo512(storage);
        return nullptr;
    }
    return b;
}

void bigint_destroy_sbo512(bigint_sbo512_t* storage) {
    if (storage != nullptr) {
        std::destroy_at(std::launder(to_sbo512_obj(storage)));
    }
}

/* ------------------------------------------------------------------------- */
/* Assignment & Mutation                                                     */
/* ------------------------------------------------------------------------- */

bigint_status_t bigint_assign(bigint_t* dst, const bigint_t* src) {
    if (dst == nullptr || src == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    if (dst == src) {
        return BIGINT_OK;
    }
    try {
        to_base(dst).assign_from(to_base(src));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_set_i64(bigint_t* dst, std::int64_t val) {
    if (dst == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    try {
        const std::int8_t sign{
            (val > 0) ? std::int8_t{1} : ((val < 0) ? std::int8_t{-1} : std::int8_t{0})};
        const std::uint64_t mag{(val < 0)
                                    ? static_cast<std::uint64_t>(-(val + 1)) + 1ULL
                                    : static_cast<std::uint64_t>(val)};
        to_base(dst).set_uint64(mag, sign);
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_set_u64(bigint_t* dst, std::uint64_t val) {
    if (dst == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    try {
        to_base(dst).set_uint64(val, (val != 0ULL) ? std::int8_t{1} : std::int8_t{0});
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_set_str(bigint_t* dst, const char* str, int base) {
    if (dst == nullptr || str == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    std::int32_t eff_base{static_cast<std::int32_t>(base)};
    if (eff_base != 0 && (eff_base < 2 || eff_base > 36)) {
        return BIGINT_ERR_INVALID_ARG;
    }

    std::string_view sv{str};
    while (!sv.empty() &&
           (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r' || sv.front() == '\n')) {
        sv.remove_prefix(1U);
    }
    if (sv.empty()) {
        return BIGINT_ERR_INVALID_ARG;
    }

    std::int8_t sign{1};
    if (sv.front() == '-') {
        sign = -1;
        sv.remove_prefix(1U);
    } else if (sv.front() == '+') {
        sv.remove_prefix(1U);
    }
    if (sv.empty()) {
        return BIGINT_ERR_INVALID_ARG;
    }

    if (eff_base == 0) {
        if (sv.size() >= 2U && sv[0] == '0' && (sv[1] == 'x' || sv[1] == 'X')) {
            eff_base = 16;
            sv.remove_prefix(2U);
        } else if (sv.size() >= 2U && sv[0] == '0' && (sv[1] == 'b' || sv[1] == 'B')) {
            eff_base = 2;
            sv.remove_prefix(2U);
        } else if (sv.size() >= 2U && sv[0] == '0' && (sv[1] == 'o' || sv[1] == 'O')) {
            eff_base = 8;
            sv.remove_prefix(2U);
        } else {
            eff_base = 10;
        }
    }

    if (eff_base == 10) {
        try {
            numeric::bigint tmp;
            const std::string s_val{(sign < 0) ? ("-" + std::string{sv}) : std::string{sv}};
            numeric::detail::BigIntCore::from_string(tmp, std::string_view{s_val});
            to_base(dst).assign_from(tmp);
            return BIGINT_OK;
        } catch (...) {
            return BIGINT_ERR_INVALID_ARG;
        }
    }

    // General radix 2..36 parsing / 通用進位 2..36 解析
    try {
        numeric::bigint val{0};
        const numeric::bigint b_base{eff_base};
        bool any_digit{false};
        for (const char c : sv) {
            std::int32_t d{-1};
            if (c >= '0' && c <= '9') {
                d = c - '0';
            } else if (c >= 'a' && c <= 'z') {
                d = (c - 'a') + 10;
            } else if (c >= 'A' && c <= 'Z') {
                d = (c - 'A') + 10;
            } else if (c == '_') {
                continue;
            } else {
                return BIGINT_ERR_INVALID_ARG;
            }

            if (d >= eff_base) {
                return BIGINT_ERR_INVALID_ARG;
            }
            val *= b_base;
            val += d;
            any_digit = true;
        }
        if (!any_digit) {
            return BIGINT_ERR_INVALID_ARG;
        }
        if (sign < 0 && val != 0) {
            val.negate();
        }
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
    if (out == nullptr || a == nullptr || b == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    try {
        numeric::detail::BigIntCore::add_signed(to_base(out), to_base(a), to_base(b));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_sub(bigint_t* out, const bigint_t* a, const bigint_t* b) {
    if (out == nullptr || a == nullptr || b == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    try {
        numeric::detail::BigIntCore::sub_signed(to_base(out), to_base(a), to_base(b));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_mul(bigint_t* out, const bigint_t* a, const bigint_t* b) {
    if (out == nullptr || a == nullptr || b == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
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
    if (out == nullptr || a == nullptr || b == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    if (to_base(b).m_size == 0U || to_base(b).m_sign == 0) {
        return BIGINT_ERR_DIV_BY_ZERO;
    }
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
    if (out == nullptr || a == nullptr || b == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    if (to_base(b).m_size == 0U || to_base(b).m_sign == 0) {
        return BIGINT_ERR_DIV_BY_ZERO;
    }
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
    if (q == nullptr || r == nullptr || a == nullptr || b == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    if (to_base(b).m_size == 0U || to_base(b).m_sign == 0) {
        return BIGINT_ERR_DIV_BY_ZERO;
    }
    try {
        numeric::bigint tmp_q;
        numeric::bigint tmp_r;
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
    if (out == nullptr || a == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    try {
        if (out != a) {
            to_base(out).assign_negated(to_base(a));
        } else {
            to_base(out).negate();
        }
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_abs(bigint_t* out, const bigint_t* a) {
    if (out == nullptr || a == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
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
    if (out == nullptr || a == nullptr || b == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
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
    if (out == nullptr || a == nullptr || b == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
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
    if (out == nullptr || a == nullptr || b == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
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
    if (out == nullptr || a == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    try {
        numeric::detail::BigIntCore::bitwise_not(to_base(out), to_base(a));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_shl(bigint_t* out, const bigint_t* a, std::size_t shift) {
    if (out == nullptr || a == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    try {
        numeric::bigint tmp;
        numeric::detail::BigIntCore::shift_left(tmp, to_base(a), shift);
        to_base(out).move_from(std::move(tmp));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_shr(bigint_t* out, const bigint_t* a, std::size_t shift) {
    if (out == nullptr || a == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
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
    if (a == nullptr && b == nullptr) {
        return 0;
    }
    if (a == nullptr) {
        return -1;
    }
    if (b == nullptr) {
        return 1;
    }
    const auto& lhs = to_base(a);
    const auto& rhs = to_base(b);
    if (lhs.m_sign < rhs.m_sign) {
        return -1;
    }
    if (lhs.m_sign > rhs.m_sign) {
        return 1;
    }
    if (lhs.m_sign == 0) {
        return 0;
    }
    const std::int32_t cmp{
        numeric::detail::BigIntCore::compare_unsigned(lhs.as_span(), rhs.as_span())};
    return (lhs.m_sign > 0) ? static_cast<int>(cmp) : static_cast<int>(-cmp);
}

int bigint_cmp_i64(const bigint_t* a, std::int64_t b) {
    if (a == nullptr) {
        return (b > 0) ? -1 : ((b < 0) ? 1 : 0);
    }
    const numeric::bigint b_val{b};
    return bigint_cmp(a, to_handle(&b_val));
}

int bigint_is_zero(const bigint_t* a) {
    if (a == nullptr) {
        return 1;
    }
    return (to_base(a).m_sign == 0 || to_base(a).m_size == 0U) ? 1 : 0;
}

int bigint_is_negative(const bigint_t* a) {
    if (a == nullptr) {
        return 0;
    }
    return (to_base(a).m_sign < 0 && to_base(a).m_size > 0U) ? 1 : 0;
}

int bigint_sign(const bigint_t* a) {
    if (a == nullptr || to_base(a).m_size == 0U) {
        return 0;
    }
    return static_cast<int>(to_base(a).m_sign);
}

/* ------------------------------------------------------------------------- */
/* High-Precision Math Functions                                             */
/* ------------------------------------------------------------------------- */

bigint_status_t bigint_pow(bigint_t* out, const bigint_t* base, std::uint32_t exp) {
    if (out == nullptr || base == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    try {
        numeric::bigint b;
        b.assign_from(to_base(base));
        numeric::bigint res{numeric::pow(b, exp)};
        to_base(out).move_from(std::move(res));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_gcd(bigint_t* out, const bigint_t* a, const bigint_t* b) {
    if (out == nullptr || a == nullptr || b == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    try {
        numeric::bigint ba;
        numeric::bigint bb;
        ba.assign_from(to_base(a));
        bb.assign_from(to_base(b));
        numeric::bigint res{numeric::gcd(ba, bb)};
        to_base(out).move_from(std::move(res));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_lcm(bigint_t* out, const bigint_t* a, const bigint_t* b) {
    if (out == nullptr || a == nullptr || b == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    try {
        numeric::bigint ba;
        numeric::bigint bb;
        ba.assign_from(to_base(a));
        bb.assign_from(to_base(b));
        numeric::bigint res{numeric::lcm(ba, bb)};
        to_base(out).move_from(std::move(res));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

bigint_status_t bigint_isqrt(bigint_t* out, const bigint_t* a) {
    if (out == nullptr || a == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    if (to_base(a).m_sign < 0) {
        return BIGINT_ERR_INVALID_ARG;
    }
    try {
        numeric::bigint ba;
        ba.assign_from(to_base(a));
        numeric::bigint res{numeric::isqrt(ba)};
        to_base(out).move_from(std::move(res));
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

/* ------------------------------------------------------------------------- */
/* Conversions & String Formatting                                           */
/* ------------------------------------------------------------------------- */

std::int64_t bigint_to_i64(const bigint_t* a, bigint_status_t* status) {
    if (a == nullptr) {
        if (status != nullptr) {
            *status = BIGINT_ERR_NULL_PTR;
        }
        return 0;
    }
    const auto& b = to_base(a);
    if (b.m_size == 0U || b.m_sign == 0) {
        if (status != nullptr) {
            *status = BIGINT_OK;
        }
        return 0;
    }
    if (b.m_size > 1U) {
        if (status != nullptr) {
            *status = BIGINT_ERR_OVERFLOW;
        }
        return (b.m_sign < 0) ? std::numeric_limits<std::int64_t>::min()
                              : std::numeric_limits<std::int64_t>::max();
    }
    const std::uint64_t mag{b.m_data[0]};
    if (b.m_sign > 0) {
        if (mag > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            if (status != nullptr) {
                *status = BIGINT_ERR_OVERFLOW;
            }
            return std::numeric_limits<std::int64_t>::max();
        }
        if (status != nullptr) {
            *status = BIGINT_OK;
        }
        return static_cast<std::int64_t>(mag);
    }

    if (mag > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1ULL) {
        if (status != nullptr) {
            *status = BIGINT_ERR_OVERFLOW;
        }
        return std::numeric_limits<std::int64_t>::min();
    }
    if (status != nullptr) {
        *status = BIGINT_OK;
    }
    return -static_cast<std::int64_t>(mag);
}

std::uint64_t bigint_to_u64(const bigint_t* a, bigint_status_t* status) {
    if (a == nullptr) {
        if (status != nullptr) {
            *status = BIGINT_ERR_NULL_PTR;
        }
        return 0ULL;
    }
    const auto& b = to_base(a);
    if (b.m_size == 0U || b.m_sign == 0) {
        if (status != nullptr) {
            *status = BIGINT_OK;
        }
        return 0ULL;
    }
    if (b.m_sign < 0 || b.m_size > 1U) {
        if (status != nullptr) {
            *status = BIGINT_ERR_OVERFLOW;
        }
        return (b.m_sign < 0) ? 0ULL : std::numeric_limits<std::uint64_t>::max();
    }
    if (status != nullptr) {
        *status = BIGINT_OK;
    }
    return b.m_data[0];
}

std::size_t bigint_to_str_len(const bigint_t* a, int base) {
    if (a == nullptr || base < 2 || base > 36) {
        return 0U;
    }
    try {
        const std::string s{format_bigint_radix(to_base(a), static_cast<std::int32_t>(base))};
        return s.size() + 1U;
    } catch (...) {
        return 0U;
    }
}

bigint_status_t bigint_to_str(const bigint_t* a, char* buf, std::size_t buf_len, int base) {
    if (a == nullptr || buf == nullptr) {
        return BIGINT_ERR_NULL_PTR;
    }
    if (base < 2 || base > 36) {
        return BIGINT_ERR_INVALID_ARG;
    }
    try {
        const std::string s{format_bigint_radix(to_base(a), static_cast<std::int32_t>(base))};
        if (buf_len <= s.size()) {
            return BIGINT_ERR_BUFFER_TOO_SMALL;
        }
        std::memcpy(buf, s.c_str(), s.size() + 1U);
        return BIGINT_OK;
    } catch (...) {
        return BIGINT_ERR_OUT_OF_MEMORY;
    }
}

char* bigint_to_str_alloc(const bigint_t* a, int base) {
    if (a == nullptr || base < 2 || base > 36) {
        return nullptr;
    }
    try {
        const std::string s{format_bigint_radix(to_base(a), static_cast<std::int32_t>(base))};
        void* const raw_mem{std::malloc(s.size() + 1U)};
        if (raw_mem != nullptr) {
            std::memcpy(raw_mem, s.c_str(), s.size() + 1U);
        }
        return static_cast<char*>(raw_mem);
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

std::size_t bigint_bit_length(const bigint_t* a) {
    if (a == nullptr) {
        return 0U;
    }
    const auto& b = to_base(a);
    if (b.m_size == 0U || b.m_sign == 0) {
        return 0U;
    }
    const std::uint64_t high{b.m_data[b.m_size - 1U]};
    const std::size_t high_bits{static_cast<std::size_t>(std::bit_width(high))};
    return ((b.m_size - 1U) * 64U) + high_bits;
}

std::size_t bigint_limb_count(const bigint_t* a) {
    if (a == nullptr) {
        return 0U;
    }
    return to_base(a).m_size;
}

int bigint_is_sbo(const bigint_t* a) {
    if (a == nullptr) {
        return 0;
    }
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
