#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <numeric/detail/BigIntCore.hpp>

#if __has_include(<gmp.h>)
#  include <gmp.h>
#  define BENCH_HAVE_GMP 1
#elif __has_include("../deps/mpir/include/gmp.h")
#  include "../deps/mpir/include/gmp.h"
#  define BENCH_HAVE_GMP 1
#else
#  define BENCH_HAVE_GMP 0
#endif

namespace bench {
namespace kernels {

// =========================================================================
// 1. CPP-BigInt Core Raw Kernels
// =========================================================================
namespace cpp_bigint {

inline uint64_t raw_add_n(uint64_t* NUMERIC_RESTRICT out,
                          const uint64_t* NUMERIC_RESTRICT a,
                          const uint64_t* NUMERIC_RESTRICT b,
                          size_t n) noexcept
{
    uint8_t carry = 0;
    for (size_t i = 0; i < n; ++i) {
        carry = numeric::detail::BigIntCore::adc64(carry, a[i], b[i], &out[i]);
    }
    return carry;
}

inline uint64_t raw_sub_n(uint64_t* NUMERIC_RESTRICT out,
                          const uint64_t* NUMERIC_RESTRICT a,
                          const uint64_t* NUMERIC_RESTRICT b,
                          size_t n) noexcept
{
    uint8_t borrow = 0;
    for (size_t i = 0; i < n; ++i) {
        borrow = numeric::detail::BigIntCore::sbb64(borrow, a[i], b[i], &out[i]);
    }
    return borrow;
}

inline void raw_mul_schoolbook(uint64_t* NUMERIC_RESTRICT out,
                               const uint64_t* NUMERIC_RESTRICT a, size_t a_len,
                               const uint64_t* NUMERIC_RESTRICT b, size_t b_len) noexcept
{
    numeric::detail::BigIntCore::mul_schoolbook_raw(out, a, a_len, b, b_len);
}

inline void raw_mul_karatsuba(uint64_t* NUMERIC_RESTRICT out,
                              const uint64_t* NUMERIC_RESTRICT a, size_t a_len,
                              const uint64_t* NUMERIC_RESTRICT b, size_t b_len,
                              uint64_t* NUMERIC_RESTRICT scratch) noexcept
{
    numeric::detail::BigIntCore::mul_karatsuba_raw(out, a, a_len, b, b_len, scratch);
}

inline void raw_div_knuth(uint64_t* NUMERIC_RESTRICT q,
                          uint64_t* NUMERIC_RESTRICT r,
                          const uint64_t* NUMERIC_RESTRICT u, size_t u_len,
                          const uint64_t* NUMERIC_RESTRICT v, size_t v_len) noexcept
{
    numeric::detail::BigIntStorage u_st, v_st, q_st, r_st;
    u_st.resize(u_len, 0);
    numeric::detail::BigIntStorage::copy_limbs(u_st.data(), u, u_len);
    u_st.m_size = u_len;
    u_st.m_sign = 1;
    u_st.normalize();

    v_st.resize(v_len, 0);
    numeric::detail::BigIntStorage::copy_limbs(v_st.data(), v, v_len);
    v_st.m_size = v_len;
    v_st.m_sign = 1;
    v_st.normalize();

    numeric::detail::BigIntCore::div_mod_core_knuth(q ? &q_st : nullptr, r ? &r_st : nullptr, u_st, v_st);
    if (q && q_st.m_size > 0) {
        numeric::detail::BigIntStorage::copy_limbs(q, q_st.data(), q_st.m_size);
    }
    if (r && r_st.m_size > 0) {
        numeric::detail::BigIntStorage::copy_limbs(r, r_st.data(), r_st.m_size);
    }
}

inline void raw_div_bz(uint64_t* NUMERIC_RESTRICT q,
                       uint64_t* NUMERIC_RESTRICT r,
                       const uint64_t* NUMERIC_RESTRICT u, size_t u_len,
                       const uint64_t* NUMERIC_RESTRICT v, size_t v_len) noexcept
{
    numeric::detail::BigIntStorage u_st, v_st, q_st, r_st;
    u_st.resize(u_len, 0);
    numeric::detail::BigIntStorage::copy_limbs(u_st.data(), u, u_len);
    u_st.m_size = u_len;
    u_st.m_sign = 1;
    u_st.normalize();

    v_st.resize(v_len, 0);
    numeric::detail::BigIntStorage::copy_limbs(v_st.data(), v, v_len);
    v_st.m_size = v_len;
    v_st.m_sign = 1;
    v_st.normalize();

    numeric::detail::BigIntCore::div_mod_core(q ? &q_st : nullptr, r ? &r_st : nullptr, u_st, v_st);
    if (q && q_st.m_size > 0) {
        numeric::detail::BigIntStorage::copy_limbs(q, q_st.data(), q_st.m_size);
    }
    if (r && r_st.m_size > 0) {
        numeric::detail::BigIntStorage::copy_limbs(r, r_st.data(), r_st.m_size);
    }
}

} // namespace cpp_bigint

// =========================================================================
// 2. GMP (MPIR) Raw mpn_* Kernels
// =========================================================================
#if BENCH_HAVE_GMP
namespace gmp {

inline uint64_t raw_add_n(uint64_t* NUMERIC_RESTRICT out,
                          const uint64_t* NUMERIC_RESTRICT a,
                          const uint64_t* NUMERIC_RESTRICT b,
                          size_t n) noexcept
{
    return mpn_add_n(reinterpret_cast<mp_ptr>(out),
                     reinterpret_cast<mp_srcptr>(a),
                     reinterpret_cast<mp_srcptr>(b),
                     static_cast<mp_size_t>(n));
}

inline uint64_t raw_sub_n(uint64_t* NUMERIC_RESTRICT out,
                          const uint64_t* NUMERIC_RESTRICT a,
                          const uint64_t* NUMERIC_RESTRICT b,
                          size_t n) noexcept
{
    return mpn_sub_n(reinterpret_cast<mp_ptr>(out),
                     reinterpret_cast<mp_srcptr>(a),
                     reinterpret_cast<mp_srcptr>(b),
                     static_cast<mp_size_t>(n));
}

inline void raw_mul_n(uint64_t* NUMERIC_RESTRICT out,
                      const uint64_t* NUMERIC_RESTRICT a,
                      const uint64_t* NUMERIC_RESTRICT b,
                      size_t n) noexcept
{
    mpn_mul_n(reinterpret_cast<mp_ptr>(out),
              reinterpret_cast<mp_srcptr>(a),
              reinterpret_cast<mp_srcptr>(b),
              static_cast<mp_size_t>(n));
}

inline void raw_div_qr(uint64_t* NUMERIC_RESTRICT q,
                       uint64_t* NUMERIC_RESTRICT r,
                       const uint64_t* NUMERIC_RESTRICT u, size_t u_len,
                       const uint64_t* NUMERIC_RESTRICT v, size_t v_len) noexcept
{
    while (v_len > 0 && v[v_len - 1] == 0) --v_len;
    while (u_len > 0 && u[u_len - 1] == 0) --u_len;

    if (v_len == 0) return;
    if (u_len < v_len) {
        if (q) q[0] = 0;
        if (r && u_len > 0) std::memcpy(r, u, u_len * sizeof(uint64_t));
        return;
    }

    mpn_tdiv_qr(reinterpret_cast<mp_ptr>(q),
                reinterpret_cast<mp_ptr>(r),
                0,
                reinterpret_cast<mp_srcptr>(u),
                static_cast<mp_size_t>(u_len),
                reinterpret_cast<mp_srcptr>(v),
                static_cast<mp_size_t>(v_len));
}

} // namespace gmp
#endif

// =========================================================================
// 3. Scalar Baseline Kernels (Naive C++ loop for compiler codegen reference)
// =========================================================================
namespace scalar {

inline uint64_t raw_add_n(uint64_t* NUMERIC_RESTRICT out,
                          const uint64_t* NUMERIC_RESTRICT a,
                          const uint64_t* NUMERIC_RESTRICT b,
                          size_t n) noexcept
{
    uint64_t carry = 0;
    for (size_t i = 0; i < n; ++i) {
        uint64_t sum = a[i] + carry;
        uint64_t c1 = (sum < a[i]) ? 1 : 0;
        sum += b[i];
        uint64_t c2 = (sum < b[i]) ? 1 : 0;
        out[i] = sum;
        carry = c1 | c2;
    }
    return carry;
}

inline uint64_t raw_sub_n(uint64_t* NUMERIC_RESTRICT out,
                          const uint64_t* NUMERIC_RESTRICT a,
                          const uint64_t* NUMERIC_RESTRICT b,
                          size_t n) noexcept
{
    uint64_t borrow = 0;
    for (size_t i = 0; i < n; ++i) {
        uint64_t diff = a[i] - borrow;
        uint64_t b1 = (diff > a[i]) ? 1 : 0;
        uint64_t final_diff = diff - b[i];
        uint64_t b2 = (final_diff > diff) ? 1 : 0;
        out[i] = final_diff;
        borrow = b1 | b2;
    }
    return borrow;
}

} // namespace scalar

} // namespace kernels
} // namespace bench
