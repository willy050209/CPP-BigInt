#pragma once

// BigIntCore.hpp
// Core arbitrary-precision integer algorithms and SBO storage layer for CPP-BigInt.
// Zero external dependencies, downward compatible from C++23 to C++11.

#include "../Config.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <type_traits>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
#include <intrin.h>
#include <immintrin.h>
#endif

namespace numeric {
namespace detail {

/// <summary>
/// BigInt 內部儲存層，採用 128-bit Small Buffer Optimization (SBO) 架構。
/// 內建 2 個 64-bit limbs 緩衝區，當數值在 128 位元以內時達成 0 堆積記憶體配置。
/// </summary>
class BigIntStorage {
public:
    static constexpr size_t SBO_CAPACITY = 2;

    uint64_t m_sbo[SBO_CAPACITY];
    uint64_t* m_data;
    size_t m_size;
    size_t m_capacity;
    int8_t m_sign;

    /// <summary>
    /// 拷貝指定數量之 limbs，支援編譯期 constexpr 運算。
    /// </summary>
    /// <param name="dst">目的端記憶體指標</param>
    /// <param name="src">來源端記憶體指標</param>
    /// <param name="count">拷貝 limbs 數量</param>
    static NUMERIC_CONSTEXPR_20 void copy_limbs(uint64_t* dst, const uint64_t* src, size_t count) noexcept {
        for (size_t i = 0; i < count; ++i) {
            dst[i] = src[i];
        }
    }

    /// <summary>
    /// 將指定數量之 limbs 清零，支援編譯期 constexpr 運算。
    /// </summary>
    /// <param name="dst">目的端記憶體指標</param>
    /// <param name="count">清零 limbs 數量</param>
    static NUMERIC_CONSTEXPR_20 void zero_limbs(uint64_t* dst, size_t count) noexcept {
        for (size_t i = 0; i < count; ++i) {
            dst[i] = 0;
        }
    }

    /// <summary>
    /// 預設建構子：初始化為零值，使用 SBO 緩衝區。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntStorage() noexcept
        : m_sbo{0, 0}, m_data(m_sbo), m_size(0), m_capacity(SBO_CAPACITY), m_sign(0) {}

    /// <summary>
    /// 解構子：若已配置堆積記憶體則進行釋放。
    /// </summary>
    NUMERIC_CONSTEXPR_20 ~BigIntStorage() noexcept {
        if (!is_sbo() && m_data != nullptr) {
            delete[] m_data;
            m_data = nullptr;
        }
    }

    /// <summary>
    /// 複製建構子：深拷貝另一儲存物件之 limbs。
    /// </summary>
    /// <param name="other">來源儲存物件</param>
    NUMERIC_CONSTEXPR_20 BigIntStorage(const BigIntStorage& other)
        : m_sbo{0, 0}, m_size(other.m_size), m_capacity(SBO_CAPACITY), m_sign(other.m_sign) {
        if (other.is_sbo()) {
            m_sbo[0] = other.m_sbo[0];
            m_sbo[1] = other.m_sbo[1];
            m_data = m_sbo;
            m_capacity = SBO_CAPACITY;
        } else {
            m_capacity = other.m_capacity;
            m_data = new uint64_t[m_capacity];
            copy_limbs(m_data, other.m_data, m_size);
        }
    }

    /// <summary>
    /// 移動建構子：轉移堆積緩衝區擁有權，或拷貝 SBO 內容。
    /// </summary>
    /// <param name="other">來源儲存物件（右值）</param>
    NUMERIC_CONSTEXPR_20 BigIntStorage(BigIntStorage&& other) noexcept
        : m_sbo{0, 0}, m_size(other.m_size), m_capacity(SBO_CAPACITY), m_sign(other.m_sign) {
        if (other.is_sbo()) {
            m_sbo[0] = other.m_sbo[0];
            m_sbo[1] = other.m_sbo[1];
            m_data = m_sbo;
            m_capacity = SBO_CAPACITY;
        } else {
            m_data = other.m_data;
            m_capacity = other.m_capacity;
            other.m_data = other.m_sbo;
            other.m_capacity = SBO_CAPACITY;
            other.m_sbo[0] = 0;
            other.m_sbo[1] = 0;
        }
        other.m_size = 0;
        other.m_sign = 0;
    }

    /// <summary>
    /// 複製賦值運算子：確保強例外安全與正確記憶體管理。
    /// </summary>
    /// <param name="other">來源儲存物件</param>
    /// <returns>自身參考</returns>
    NUMERIC_CONSTEXPR_20 BigIntStorage& operator=(const BigIntStorage& other) {
        if (this != &other) {
            if (other.is_sbo()) {
                if (!is_sbo()) {
                    delete[] m_data;
                }
                m_sbo[0] = other.m_sbo[0];
                m_sbo[1] = other.m_sbo[1];
                m_data = m_sbo;
                m_capacity = SBO_CAPACITY;
            } else {
                if (m_capacity < other.m_size) {
                    if (!is_sbo()) {
                        delete[] m_data;
                    }
                    m_capacity = other.m_capacity;
                    m_data = new uint64_t[m_capacity];
                }
                copy_limbs(m_data, other.m_data, other.m_size);
            }
            m_size = other.m_size;
            m_sign = other.m_sign;
        }
        return *this;
    }

    /// <summary>
    /// 移動賦值運算子：轉移緩衝區資源。
    /// </summary>
    /// <param name="other">來源儲存物件（右值）</param>
    /// <returns>自身參考</returns>
    NUMERIC_CONSTEXPR_20 BigIntStorage& operator=(BigIntStorage&& other) noexcept {
        if (this != &other) {
            if (!is_sbo()) {
                delete[] m_data;
            }
            m_size = other.m_size;
            m_sign = other.m_sign;
            if (other.is_sbo()) {
                m_sbo[0] = other.m_sbo[0];
                m_sbo[1] = other.m_sbo[1];
                m_data = m_sbo;
                m_capacity = SBO_CAPACITY;
            } else {
                m_data = other.m_data;
                m_capacity = other.m_capacity;
                other.m_data = other.m_sbo;
                other.m_capacity = SBO_CAPACITY;
                other.m_sbo[0] = 0;
                other.m_sbo[1] = 0;
            }
            other.m_size = 0;
            other.m_sign = 0;
        }
        return *this;
    }

    /// <summary>
    /// 檢查當前是否使用 SBO 內建緩衝區儲存。
    /// </summary>
    /// <returns>若使用 SBO 則回傳 true，堆積配置則回傳 false</returns>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 bool is_sbo() const noexcept {
        return m_data == m_sbo;
    }

    /// <summary>
    /// 預留緩衝區容量。
    /// </summary>
    /// <param name="new_cap">目標容量大小（limbs）</param>
    NUMERIC_CONSTEXPR_20 void reserve(size_t new_cap) {
        if (new_cap <= m_capacity) return;
        uint64_t* new_data = new uint64_t[new_cap];
        if (m_size > 0) {
            copy_limbs(new_data, m_data, m_size);
        }
        if (!is_sbo()) {
            delete[] m_data;
        }
        m_data = new_data;
        m_capacity = new_cap;
    }

    /// <summary>
    /// 調整 limbs 數量大小並可選填預設值。
    /// </summary>
    /// <param name="new_size">目標大小</param>
    /// <param name="init_val">新擴充元素之初始值</param>
    NUMERIC_CONSTEXPR_20 void resize(size_t new_size, uint64_t init_val = 0) {
        if (new_size > m_capacity) {
            size_t next_cap = m_capacity * 2;
            if (next_cap < new_size) next_cap = new_size;
            reserve(next_cap);
        }
        if (new_size > m_size) {
            for (size_t i = m_size; i < new_size; ++i) {
                m_data[i] = init_val;
            }
        }
        m_size = new_size;
    }

    /// <summary>
    /// 規範化 limbs 陣列，移除高位無效之 0 limbs 並調整正負符號；若長度落回 SBO 則縮回 SBO。
    /// </summary>
    NUMERIC_CONSTEXPR_20 void normalize() noexcept {
        while (m_size > 0 && m_data[m_size - 1] == 0) {
            --m_size;
        }
        if (m_size == 0) {
            m_sign = 0;
        }
        shrink_to_sbo_if_possible();
    }

    /// <summary>
    /// 當 limbs 數量小於等於 SBO 容量且當前為堆積配置時，縮回 SBO。
    /// </summary>
    NUMERIC_CONSTEXPR_20 void shrink_to_sbo_if_possible() noexcept {
        if (!is_sbo() && m_size <= SBO_CAPACITY) {
            uint64_t* old_data = m_data;
            m_sbo[0] = (m_size > 0) ? old_data[0] : 0;
            m_sbo[1] = (m_size > 1) ? old_data[1] : 0;
            m_data = m_sbo;
            m_capacity = SBO_CAPACITY;
            delete[] old_data;
        }
    }

    /// <summary>
    /// 設定為 64 位元無符號整數與指定正負號。
    /// </summary>
    /// <param name="val">數值</param>
    /// <param name="sign">符號 (-1, 0, 1)</param>
    NUMERIC_CONSTEXPR_20 void set_uint64(uint64_t val, int8_t sign) noexcept {
        if (!is_sbo()) {
            delete[] m_data;
            m_data = m_sbo;
            m_capacity = SBO_CAPACITY;
        }
        if (val == 0) {
            m_size = 0;
            m_sign = 0;
            m_sbo[0] = 0;
            m_sbo[1] = 0;
        } else {
            m_size = 1;
            m_sign = sign;
            m_sbo[0] = val;
            m_sbo[1] = 0;
        }
    }
};

/// <summary>
/// BigInt 演算法核心類別，提供無符號與有符號之任意精度運算。
/// </summary>
class BigIntCore {
public:
    static constexpr size_t KARATSUBA_THRESHOLD = 16;

    /// <summary>
    /// 計算 64 位元整數之前導 0 數量 (Count Leading Zeros)。
    /// </summary>
    /// <param name="x">目標 64 位元整數</param>
    /// <returns>前導 0 數量 (0~64)</returns>
    static NUMERIC_CONSTEXPR_20 int clz64(uint64_t x) noexcept {
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            if (x == 0) return 64;
            int n = 0;
            if ((x >> 32) == 0) { n += 32; x <<= 32; }
            if ((x >> 48) == 0) { n += 16; x <<= 16; }
            if ((x >> 56) == 0) { n += 8;  x <<= 8;  }
            if ((x >> 60) == 0) { n += 4;  x <<= 4;  }
            if ((x >> 62) == 0) { n += 2;  x <<= 2;  }
            if ((x >> 63) == 0) { n += 1; }
            return n;
        }
#endif
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        unsigned long idx;
        if (_BitScanReverse64(&idx, x)) {
            return static_cast<int>(63 - idx);
        }
        return 64;
#elif defined(__GNUC__) || defined(__clang__)
        return (x == 0) ? 64 : __builtin_clzll(x);
#else
        if (x == 0) return 64;
        int n = 0;
        if ((x >> 32) == 0) { n += 32; x <<= 32; }
        if ((x >> 48) == 0) { n += 16; x <<= 16; }
        if ((x >> 56) == 0) { n += 8;  x <<= 8;  }
        if ((x >> 60) == 0) { n += 4;  x <<= 4;  }
        if ((x >> 62) == 0) { n += 2;  x <<= 2;  }
        if ((x >> 63) == 0) { n += 1; }
        return n;
#endif
    }

    /// <summary>
    /// 64 位元乘法運算，輸出低 64 位並回傳高 64 位進位。
    /// </summary>
    /// <param name="a">乘數 a</param>
    /// <param name="b">乘數 b</param>
    /// <param name="hi">輸出高 64 位進位參考</param>
    /// <returns>乘積之低 64 位元</returns>
    static NUMERIC_CONSTEXPR_20 uint64_t mul64_wide(uint64_t a, uint64_t b, uint64_t& hi) noexcept {
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            uint64_t a_lo = static_cast<uint32_t>(a);
            uint64_t a_hi = a >> 32;
            uint64_t b_lo = static_cast<uint32_t>(b);
            uint64_t b_hi = b >> 32;
            uint64_t p0 = a_lo * b_lo;
            uint64_t p1 = a_lo * b_hi;
            uint64_t p2 = a_hi * b_lo;
            uint64_t p3 = a_hi * b_hi;
            uint64_t mid = p1 + static_cast<uint32_t>(p0 >> 32) + static_cast<uint32_t>(p2);
            hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
            return (mid << 32) | static_cast<uint32_t>(p0);
        }
#endif
#if defined(__SIZEOF_INT128__)
        unsigned __int128 prod = static_cast<unsigned __int128>(a) * b;
        hi = static_cast<uint64_t>(prod >> 64);
        return static_cast<uint64_t>(prod);
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        return _umul128(a, b, &hi);
#else
        uint64_t a_lo = static_cast<uint32_t>(a);
        uint64_t a_hi = a >> 32;
        uint64_t b_lo = static_cast<uint32_t>(b);
        uint64_t b_hi = b >> 32;
        uint64_t p0 = a_lo * b_lo;
        uint64_t p1 = a_lo * b_hi;
        uint64_t p2 = a_hi * b_lo;
        uint64_t p3 = a_hi * b_hi;
        uint64_t mid = p1 + static_cast<uint32_t>(p0 >> 32) + static_cast<uint32_t>(p2);
        hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
        return (mid << 32) | static_cast<uint32_t>(p0);
#endif
    }

    /// <summary>
    /// 128 位元除以 64 位元整數之除法（前置條件：hi 小於 d）。
    /// </summary>
    /// <param name="hi">被除數高 64 位元</param>
    /// <param name="lo">被除數低 64 位元</param>
    /// <param name="d">除數</param>
    /// <param name="rem">輸出餘數參考</param>
    /// <returns>商之 64 位元數值</returns>
    static NUMERIC_CONSTEXPR_20 uint64_t div128_64(uint64_t hi, uint64_t lo, uint64_t d, uint64_t& rem) noexcept {
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            uint64_t q = 0;
            uint64_t r = hi;
            for (int i = 63; i >= 0; --i) {
                r = (r << 1) | ((lo >> i) & 1);
                if (r >= d) {
                    r -= d;
                    q |= (1ULL << i);
                }
            }
            rem = r;
            return q;
        }
#endif
#if defined(__SIZEOF_INT128__)
        unsigned __int128 n = (static_cast<unsigned __int128>(hi) << 64) | lo;
        rem = static_cast<uint64_t>(n % d);
        return static_cast<uint64_t>(n / d);
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        return _udiv128(hi, lo, d, &rem);
#else
        uint64_t q = 0;
        uint64_t r = hi;
        for (int i = 63; i >= 0; --i) {
            r = (r << 1) | ((lo >> i) & 1);
            if (r >= d) {
                r -= d;
                q |= (1ULL << i);
            }
        }
        rem = r;
        return q;
#endif
    }

    /// <summary>
    /// 比較兩無符號 limbs 陣列之大小。
    /// </summary>
    /// <param name="a">陣列 a</param>
    /// <param name="a_len">陣列 a 長度</param>
    /// <param name="b">陣列 b</param>
    /// <param name="b_len">陣列 b 長度</param>
    /// <returns>若 a &gt; b 回傳 1，a &lt; b 回傳 -1，相等回傳 0</returns>
    static NUMERIC_CONSTEXPR_20 int compare_unsigned(const uint64_t* a, size_t a_len, const uint64_t* b, size_t b_len) noexcept {
        if (a_len > b_len) return 1;
        if (a_len < b_len) return -1;
        for (size_t i = a_len; i > 0; --i) {
            if (a[i - 1] > b[i - 1]) return 1;
            if (a[i - 1] < b[i - 1]) return -1;
        }
        return 0;
    }

    /// <summary>
    /// 無符號 limbs 加法：res = a + b。
    /// </summary>
    /// <param name="res">輸出結果儲存物件</param>
    /// <param name="a">加數 a</param>
    /// <param name="b">加數 b</param>
    static NUMERIC_CONSTEXPR_20 void add_unsigned(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        size_t max_len = (a.m_size > b.m_size) ? a.m_size : b.m_size;
        res.resize(max_len + 1, 0);
        uint64_t carry = 0;
        for (size_t i = 0; i < max_len; ++i) {
            uint64_t av = (i < a.m_size) ? a.m_data[i] : 0;
            uint64_t bv = (i < b.m_size) ? b.m_data[i] : 0;
            uint64_t sum = av + carry;
            uint64_t c1 = (sum < av) ? 1 : 0;
            sum += bv;
            uint64_t c2 = (sum < bv) ? 1 : 0;
            carry = c1 + c2;
            res.m_data[i] = sum;
        }
        res.m_data[max_len] = carry;
        res.m_sign = 1;
        res.normalize();
    }

    /// <summary>
    /// 無符號 limbs 減法：res = a - b（前置條件：a &gt;= b）。
    /// </summary>
    /// <param name="res">輸出結果儲存物件</param>
    /// <param name="a">被減數 a</param>
    /// <param name="b">減數 b</param>
    static NUMERIC_CONSTEXPR_20 void sub_unsigned(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        res.resize(a.m_size, 0);
        uint64_t borrow = 0;
        for (size_t i = 0; i < a.m_size; ++i) {
            uint64_t av = a.m_data[i];
            uint64_t bv = (i < b.m_size) ? b.m_data[i] : 0;
            uint64_t diff = av - borrow;
            uint64_t b1 = (av < borrow) ? 1 : 0;
            uint64_t diff2 = diff - bv;
            uint64_t b2 = (diff < bv) ? 1 : 0;
            borrow = b1 + b2;
            res.m_data[i] = diff2;
        }
        res.m_sign = 1;
        res.normalize();
    }

    /// <summary>
    /// 帶符號加法：res = a + b。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元 a</param>
    /// <param name="b">運算元 b</param>
    static NUMERIC_CONSTEXPR_20 void add_signed(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (a.m_sign == 0) {
            res = b;
            return;
        }
        if (b.m_sign == 0) {
            res = a;
            return;
        }
        if (a.m_sign == b.m_sign) {
            add_unsigned(res, a, b);
            res.m_sign = a.m_sign;
        } else {
            int cmp = compare_unsigned(a.m_data, a.m_size, b.m_data, b.m_size);
            if (cmp == 0) {
                res.m_size = 0;
                res.m_sign = 0;
            } else if (cmp > 0) {
                sub_unsigned(res, a, b);
                res.m_sign = a.m_sign;
            } else {
                sub_unsigned(res, b, a);
                res.m_sign = b.m_sign;
            }
        }
    }

    /// <summary>
    /// 帶符號減法：res = a - b。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">被減數 a</param>
    /// <param name="b">減數 b</param>
    static NUMERIC_CONSTEXPR_20 void sub_signed(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (b.m_sign == 0) {
            res = a;
            return;
        }
        if (a.m_sign == 0) {
            res = b;
            res.m_sign = -res.m_sign;
            return;
        }
        BigIntStorage neg_b = b;
        neg_b.m_sign = -neg_b.m_sign;
        add_signed(res, a, neg_b);
    }

    /// <summary>
    /// 傳統 Schoolbook 長整數乘法演算法。
    /// </summary>
    /// <param name="res">輸出乘積儲存物件</param>
    /// <param name="a">乘數 a</param>
    /// <param name="b">乘數 b</param>
    static NUMERIC_CONSTEXPR_20 void mul_schoolbook(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (a.m_size == 0 || b.m_size == 0) {
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        BigIntStorage tmp;
        tmp.resize(a.m_size + b.m_size, 0);
        for (size_t i = 0; i < a.m_size; ++i) {
            if (a.m_data[i] == 0) continue;
            uint64_t carry = 0;
            for (size_t j = 0; j < b.m_size; ++j) {
                uint64_t hi = 0;
                uint64_t lo = mul64_wide(a.m_data[i], b.m_data[j], hi);
                uint64_t cur = tmp.m_data[i + j];
                uint64_t sum = cur + lo;
                uint64_t c1 = (sum < cur) ? 1 : 0;
                sum += carry;
                uint64_t c2 = (sum < carry) ? 1 : 0;
                tmp.m_data[i + j] = sum;
                carry = hi + c1 + c2;
            }
            tmp.m_data[i + b.m_size] += carry;
        }
        tmp.m_sign = 1;
        tmp.normalize();
        res = std::move(tmp);
    }

    /// <summary>
    /// Karatsuba 快速乘法演算法。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">乘數 a</param>
    /// <param name="b">乘數 b</param>
    static NUMERIC_CONSTEXPR_20 void mul_karatsuba(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        size_t n = (a.m_size > b.m_size) ? a.m_size : b.m_size;
        if (n < KARATSUBA_THRESHOLD || a.m_size == 0 || b.m_size == 0) {
            mul_schoolbook(res, a, b);
            return;
        }
        size_t k = n / 2;

        BigIntStorage a0, a1, b0, b1;
        size_t a0_len = (a.m_size < k) ? a.m_size : k;
        size_t b0_len = (b.m_size < k) ? b.m_size : k;

        a0.resize(a0_len);
        if (a0_len > 0) BigIntStorage::copy_limbs(a0.m_data, a.m_data, a0_len);
        a0.m_sign = 1;
        a0.normalize();

        if (a.m_size > k) {
            size_t a1_len = a.m_size - k;
            a1.resize(a1_len);
            BigIntStorage::copy_limbs(a1.m_data, a.m_data + k, a1_len);
            a1.m_sign = 1;
            a1.normalize();
        }

        b0.resize(b0_len);
        if (b0_len > 0) BigIntStorage::copy_limbs(b0.m_data, b.m_data, b0_len);
        b0.m_sign = 1;
        b0.normalize();

        if (b.m_size > k) {
            size_t b1_len = b.m_size - k;
            b1.resize(b1_len);
            BigIntStorage::copy_limbs(b1.m_data, b.m_data + k, b1_len);
            b1.m_sign = 1;
            b1.normalize();
        }

        BigIntStorage z0, z2;
        mul_core(z0, a0, b0);
        mul_core(z2, a1, b1);

        BigIntStorage ta, tb;
        add_signed(ta, a0, a1);
        add_signed(tb, b0, b1);

        BigIntStorage p;
        mul_core(p, ta, tb);

        BigIntStorage z1;
        sub_signed(z1, p, z0);
        sub_signed(z1, z1, z2);

        // res = (z2 << (128*k)) + (z1 << (64*k)) + z0
        BigIntStorage z1_shifted, z2_shifted;
        shift_left_limbs(z1_shifted, z1, k);
        shift_left_limbs(z2_shifted, z2, 2 * k);

        BigIntStorage final_res;
        add_signed(final_res, z0, z1_shifted);
        add_signed(final_res, final_res, z2_shifted);
        res = std::move(final_res);
    }

    /// <summary>
    /// 內部乘法派發函式，依據位數自動切換 Schoolbook 與 Karatsuba。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">乘數 a</param>
    /// <param name="b">乘數 b</param>
    static NUMERIC_CONSTEXPR_20 void mul_core(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (a.m_size < KARATSUBA_THRESHOLD || b.m_size < KARATSUBA_THRESHOLD) {
            mul_schoolbook(res, a, b);
        } else {
            mul_karatsuba(res, a, b);
        }
    }

    /// <summary>
    /// 帶符號乘法：res = a * b。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">乘數 a</param>
    /// <param name="b">乘數 b</param>
    static NUMERIC_CONSTEXPR_20 void mul_signed(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (a.m_sign == 0 || b.m_sign == 0) {
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        int8_t res_sign = static_cast<int8_t>(a.m_sign * b.m_sign);
        mul_core(res, a, b);
        res.m_sign = res_sign;
    }

    /// <summary>
    /// Knuth Algorithm D 與單 limb 快速長除法演算法。
    /// </summary>
    /// <param name="q">輸出商</param>
    /// <param name="r">輸出餘數</param>
    /// <param name="u">被除數</param>
    /// <param name="v">除數</param>
    /// <exception cref="std::invalid_argument">當除數為 0 時拋出</exception>
    static NUMERIC_CONSTEXPR_20 void div_mod_core(BigIntStorage& q, BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) {
        if (v.m_size == 0) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("division by zero"));
        }
        if (u.m_size == 0) {
            q.m_size = 0; q.m_sign = 0;
            r.m_size = 0; r.m_sign = 0;
            return;
        }
        int cmp = compare_unsigned(u.m_data, u.m_size, v.m_data, v.m_size);
        if (cmp < 0) {
            q.m_size = 0; q.m_sign = 0;
            r = u;
            r.m_sign = 1;
            return;
        }
        if (cmp == 0) {
            q.set_uint64(1, 1);
            r.m_size = 0; r.m_sign = 0;
            return;
        }

        // 單 limb 快速除法路徑
        if (v.m_size == 1) {
            uint64_t divisor = v.m_data[0];
            q.resize(u.m_size, 0);
            uint64_t rem = 0;
            for (size_t i = u.m_size; i > 0; --i) {
                uint64_t next_rem = 0;
                q.m_data[i - 1] = div128_64(rem, u.m_data[i - 1], divisor, next_rem);
                rem = next_rem;
            }
            q.m_sign = 1;
            q.normalize();
            if (rem != 0) {
                r.set_uint64(rem, 1);
            } else {
                r.m_size = 0;
                r.m_sign = 0;
            }
            return;
        }

        // Knuth Algorithm D (多 limb 長除法)
        size_t n = v.m_size;
        size_t m = u.m_size - n;

        // D1: 正規化 (shift left by s bits)
        int s = clz64(v.m_data[n - 1]);
        BigIntStorage vn, un;
        shift_left(vn, v, static_cast<size_t>(s));
        shift_left(un, u, static_cast<size_t>(s));
        if (un.m_size < u.m_size + 1) {
            un.resize(u.m_size + 1, 0);
        }

        q.resize(m + 1, 0);

        uint64_t v_hi = vn.m_data[n - 1];
        uint64_t v_lo = vn.m_data[n - 2];

        // D2~D7: 主迴圈
        for (size_t k = m + 1; k > 0; --k) {
            size_t j = k - 1;
            uint64_t u_hi = un.m_data[j + n];
            uint64_t u_mid = un.m_data[j + n - 1];
            uint64_t u_lo = (j + n >= 2) ? un.m_data[j + n - 2] : 0;

            uint64_t q_hat = 0;
            uint64_t r_hat = 0;

            if (u_hi == v_hi) {
                q_hat = 0xFFFFFFFFFFFFFFFFULL;
                r_hat = u_mid + v_hi;
                if (r_hat >= v_hi) {
                    while (true) {
                        uint64_t p_hi = 0;
                        uint64_t p_lo = mul64_wide(q_hat, v_lo, p_hi);
                        if (p_hi > r_hat || (p_hi == r_hat && p_lo > u_lo)) {
                            --q_hat;
                            r_hat += v_hi;
                            if (r_hat < v_hi) break;
                        } else {
                            break;
                        }
                    }
                }
            } else {
                q_hat = div128_64(u_hi, u_mid, v_hi, r_hat);
                while (true) {
                    uint64_t p_hi = 0;
                    uint64_t p_lo = mul64_wide(q_hat, v_lo, p_hi);
                    if (p_hi > r_hat || (p_hi == r_hat && p_lo > u_lo)) {
                        --q_hat;
                        r_hat += v_hi;
                        if (r_hat < v_hi) break;
                    } else {
                        break;
                    }
                }
            }

            // D4: 乘並減
            uint64_t carry = 0;
            uint64_t borrow = 0;
            for (size_t i = 0; i < n; ++i) {
                uint64_t p_hi = 0;
                uint64_t p_lo = mul64_wide(q_hat, vn.m_data[i], p_hi);
                uint64_t p_full = p_lo + carry;
                uint64_t c1 = (p_full < p_lo) ? 1 : 0;
                carry = p_hi + c1;

                uint64_t cur = un.m_data[j + i];
                uint64_t diff = cur - borrow;
                uint64_t b1 = (cur < borrow) ? 1 : 0;
                uint64_t diff2 = diff - p_full;
                uint64_t b2 = (diff < p_full) ? 1 : 0;
                borrow = b1 + b2;
                un.m_data[j + i] = diff2;
            }

            uint64_t cur = un.m_data[j + n];
            uint64_t diff = cur - borrow;
            uint64_t b1 = (cur < borrow) ? 1 : 0;
            uint64_t diff2 = diff - carry;
            uint64_t b2 = (diff < carry) ? 1 : 0;
            un.m_data[j + n] = diff2;

            // D5: 判斷是否需要回加
            if (b1 + b2 > 0) {
                --q_hat;
                uint64_t add_carry = 0;
                for (size_t i = 0; i < n; ++i) {
                    uint64_t val = un.m_data[j + i];
                    uint64_t sum = val + vn.m_data[i] + add_carry;
                    add_carry = (sum < val || (add_carry && sum == val)) ? 1 : 0;
                    un.m_data[j + i] = sum;
                }
                un.m_data[j + n] += add_carry;
            }

            q.m_data[j] = q_hat;
        }

        q.m_sign = 1;
        q.normalize();

        // D8: 去正規化餘數
        un.m_size = n;
        un.normalize();
        if (s > 0) {
            shift_right(r, un, static_cast<size_t>(s));
        } else {
            r = un;
        }
        r.m_sign = (r.m_size > 0) ? 1 : 0;
    }

    /// <summary>
    /// 帶符號除法與模運算（符合 C++ 截斷除法規格）。
    /// </summary>
    /// <param name="q">輸出商</param>
    /// <param name="r">輸出餘數</param>
    /// <param name="u">被除數</param>
    /// <param name="v">除數</param>
    static NUMERIC_CONSTEXPR_20 void div_mod_signed(BigIntStorage& q, BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) {
        div_mod_core(q, r, u, v);
        if (q.m_size > 0) {
            q.m_sign = static_cast<int8_t>(u.m_sign * v.m_sign);
        }
        if (r.m_size > 0) {
            r.m_sign = u.m_sign;
        }
    }

    /// <summary>
    /// 將 limbs 整體左移指定 limbs 數量。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">輸入數值</param>
    /// <param name="limbs">移動 limbs 數量</param>
    static NUMERIC_CONSTEXPR_20 void shift_left_limbs(BigIntStorage& res, const BigIntStorage& a, size_t limbs) {
        if (a.m_size == 0) {
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        res.resize(a.m_size + limbs, 0);
        BigIntStorage::copy_limbs(res.m_data + limbs, a.m_data, a.m_size);
        BigIntStorage::zero_limbs(res.m_data, limbs);
        res.m_sign = a.m_sign;
        res.normalize();
    }

    /// <summary>
    /// 位元左移運算：res = a &lt;&lt; shift。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元</param>
    /// <param name="shift">位移位元數</param>
    static NUMERIC_CONSTEXPR_20 void shift_left(BigIntStorage& res, const BigIntStorage& a, size_t shift) {
        if (shift == 0 || a.m_size == 0) {
            res = a;
            return;
        }
        size_t limb_shift = shift / 64;
        size_t bit_shift = shift % 64;
        size_t new_size = a.m_size + limb_shift + 1;
        res.resize(new_size, 0);

        if (bit_shift == 0) {
            BigIntStorage::copy_limbs(res.m_data + limb_shift, a.m_data, a.m_size);
            if (limb_shift > 0) {
                BigIntStorage::zero_limbs(res.m_data, limb_shift);
            }
        } else {
            if (limb_shift > 0) {
                BigIntStorage::zero_limbs(res.m_data, limb_shift);
            }
            uint64_t carry = 0;
            for (size_t i = 0; i < a.m_size; ++i) {
                uint64_t cur = a.m_data[i];
                res.m_data[i + limb_shift] = (cur << bit_shift) | carry;
                carry = cur >> (64 - bit_shift);
            }
            res.m_data[a.m_size + limb_shift] = carry;
        }
        res.m_sign = a.m_sign;
        res.normalize();
    }

    /// <summary>
    /// 位元右移運算：res = a &gt;&gt; shift（支援負數算術右移語意）。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元</param>
    /// <param name="shift">位移位元數</param>
    static NUMERIC_CONSTEXPR_20 void shift_right(BigIntStorage& res, const BigIntStorage& a, size_t shift) {
        if (shift == 0 || a.m_size == 0) {
            res = a;
            return;
        }
        if (a.m_sign < 0) {
            // 負數算術右移：a >> shift = ~((~a) >> shift) = - ((-a - 1) >> shift) - 1
            BigIntStorage u;
            BigIntStorage one_st; one_st.set_uint64(1, 1);
            BigIntStorage abs_a = a; abs_a.m_sign = 1;
            sub_signed(u, abs_a, one_st);

            BigIntStorage shifted_u;
            shift_right_positive(shifted_u, u, shift);

            BigIntStorage final_res;
            add_signed(final_res, shifted_u, one_st);
            final_res.m_sign = -1;
            final_res.normalize();
            res = std::move(final_res);
            return;
        }
        shift_right_positive(res, a, shift);
    }

    /// <summary>
    /// 正整數無符號位元右移運算。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元</param>
    /// <param name="shift">位移位元數</param>
    static NUMERIC_CONSTEXPR_20 void shift_right_positive(BigIntStorage& res, const BigIntStorage& a, size_t shift) {
        size_t limb_shift = shift / 64;
        size_t bit_shift = shift % 64;
        if (limb_shift >= a.m_size) {
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        size_t new_size = a.m_size - limb_shift;
        res.resize(new_size, 0);

        if (bit_shift == 0) {
            BigIntStorage::copy_limbs(res.m_data, a.m_data + limb_shift, new_size);
        } else {
            for (size_t i = 0; i < new_size; ++i) {
                uint64_t cur = a.m_data[i + limb_shift];
                uint64_t next = (i + limb_shift + 1 < a.m_size) ? a.m_data[i + limb_shift + 1] : 0;
                res.m_data[i] = (cur >> bit_shift) | (next << (64 - bit_shift));
            }
        }
        res.m_sign = (res.m_size > 0) ? a.m_sign : 0;
        res.normalize();
    }

    /// <summary>
    /// 位元非運算：~a = -a - 1。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">輸入數值</param>
    static NUMERIC_CONSTEXPR_20 void bitwise_not(BigIntStorage& res, const BigIntStorage& a) {
        BigIntStorage one_st; one_st.set_uint64(1, 1);
        BigIntStorage tmp;
        add_signed(tmp, a, one_st);
        tmp.m_sign = -tmp.m_sign;
        tmp.normalize();
        res = std::move(tmp);
    }

    /// <summary>
    /// 位元及運算：res = a &amp; b。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元 a</param>
    /// <param name="b">運算元 b</param>
    static NUMERIC_CONSTEXPR_20 void bitwise_and(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (a.m_sign == 0 || b.m_sign == 0) {
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        if (a.m_sign > 0 && b.m_sign > 0) {
            size_t min_len = (a.m_size < b.m_size) ? a.m_size : b.m_size;
            res.resize(min_len, 0);
            for (size_t i = 0; i < min_len; ++i) {
                res.m_data[i] = a.m_data[i] & b.m_data[i];
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
        if (a.m_sign > 0 && b.m_sign < 0) {
            // a & b = a & ~(~b) = a & ~u
            BigIntStorage not_b;
            bitwise_not(not_b, b);
            res.resize(a.m_size, 0);
            for (size_t i = 0; i < a.m_size; ++i) {
                uint64_t nu = (i < not_b.m_size) ? not_b.m_data[i] : 0;
                res.m_data[i] = a.m_data[i] & (~nu);
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
        if (a.m_sign < 0 && b.m_sign > 0) {
            bitwise_and(res, b, a);
            return;
        }
        // a < 0 && b < 0: ~(a & b) = (~a) | (~b)
        BigIntStorage not_a, not_b, or_res;
        bitwise_not(not_a, a);
        bitwise_not(not_b, b);
        bitwise_or(or_res, not_a, not_b);
        bitwise_not(res, or_res);
    }

    /// <summary>
    /// 位元或運算：res = a | b。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元 a</param>
    /// <param name="b">運算元 b</param>
    static NUMERIC_CONSTEXPR_20 void bitwise_or(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (a.m_sign == 0) { res = b; return; }
        if (b.m_sign == 0) { res = a; return; }
        if (a.m_sign > 0 && b.m_sign > 0) {
            size_t max_len = (a.m_size > b.m_size) ? a.m_size : b.m_size;
            res.resize(max_len, 0);
            for (size_t i = 0; i < max_len; ++i) {
                uint64_t av = (i < a.m_size) ? a.m_data[i] : 0;
                uint64_t bv = (i < b.m_size) ? b.m_data[i] : 0;
                res.m_data[i] = av | bv;
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
        // De Morgan: ~(a | b) = (~a) & (~b)
        BigIntStorage not_a, not_b, and_res;
        bitwise_not(not_a, a);
        bitwise_not(not_b, b);
        bitwise_and(and_res, not_a, not_b);
        bitwise_not(res, and_res);
    }

    /// <summary>
    /// 位元互斥或運算：res = a ^ b。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">運算元 a</param>
    /// <param name="b">運算元 b</param>
    static NUMERIC_CONSTEXPR_20 void bitwise_xor(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (a.m_sign == 0) { res = b; return; }
        if (b.m_sign == 0) { res = a; return; }
        if (a.m_sign > 0 && b.m_sign > 0) {
            size_t max_len = (a.m_size > b.m_size) ? a.m_size : b.m_size;
            res.resize(max_len, 0);
            for (size_t i = 0; i < max_len; ++i) {
                uint64_t av = (i < a.m_size) ? a.m_data[i] : 0;
                uint64_t bv = (i < b.m_size) ? b.m_data[i] : 0;
                res.m_data[i] = av ^ bv;
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
        if (a.m_sign > 0 && b.m_sign < 0) {
            // a ^ b = ~(a ^ ~b)
            BigIntStorage not_b, xor_res;
            bitwise_not(not_b, b);
            bitwise_xor(xor_res, a, not_b);
            bitwise_not(res, xor_res);
            return;
        }
        if (a.m_sign < 0 && b.m_sign > 0) {
            bitwise_xor(res, b, a);
            return;
        }
        // a < 0 && b < 0: a ^ b = (~a) ^ (~b)
        BigIntStorage not_a, not_b;
        bitwise_not(not_a, a);
        bitwise_not(not_b, b);
        bitwise_xor(res, not_a, not_b);
    }

    /// <summary>
    /// 字串解析，將十進位字串轉換為 BigIntStorage。
    /// </summary>
    /// <param name="res">輸出儲存物件</param>
    /// <param name="sv">輸入字串視圖</param>
    /// <exception cref="std::invalid_argument">當字串為空或包含非數字字元時拋出</exception>
    static NUMERIC_CONSTEXPR_20 void from_string(BigIntStorage& res, numeric::string_view sv) {
        if (sv.empty()) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("empty bigint string"));
        }
        size_t idx = 0;
        int8_t sign = 1;
        if (sv[0] == '-') {
            sign = -1;
            ++idx;
        } else if (sv[0] == '+') {
            ++idx;
        }
        if (idx == sv.size()) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("no digits in bigint string"));
        }

        // 驗證所有字元皆為十進位數字
        for (size_t i = idx; i < sv.size(); ++i) {
            if (sv[i] < '0' || sv[i] > '9') {
                NUMERIC_THROW_OR_ABORT(std::invalid_argument("invalid character in bigint string"));
            }
        }

        // 跳過前導 0
        while (idx < sv.size() && sv[idx] == '0') {
            ++idx;
        }
        if (idx == sv.size()) {
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }

        // 分塊解析：每次最多讀取 19 位數字 (10^19 < 2^64)
        BigIntStorage cur;
        while (idx < sv.size()) {
            size_t chunk_len = std::min<size_t>(19, sv.size() - idx);
            uint64_t chunk_val = 0;
            uint64_t mult = 1;
            for (size_t i = 0; i < chunk_len; ++i) {
                chunk_val = chunk_val * 10 + static_cast<uint64_t>(sv[idx + i] - '0');
                mult *= 10;
            }
            idx += chunk_len;

            BigIntStorage mult_st, chunk_st, prod;
            mult_st.set_uint64(mult, 1);
            chunk_st.set_uint64(chunk_val, 1);

            mul_signed(prod, cur, mult_st);
            add_signed(cur, prod, chunk_st);
        }
        cur.m_sign = sign;
        cur.normalize();
        res = std::move(cur);
    }

    /// <summary>
    /// 將 BigIntStorage 轉換為 Radix-10 十進位字串。
    /// </summary>
    /// <param name="a">輸入儲存物件</param>
    /// <returns>十進位字串表示</returns>
    static std::string to_string(const BigIntStorage& a) {
        if (a.m_sign == 0) {
            return "0";
        }
        BigIntStorage cur = a;
        cur.m_sign = 1;

        BigIntStorage radix_st;
        radix_st.set_uint64(10000000000000000000ULL, 1); // 10^19

        std::vector<uint64_t> chunks;
        while (cur.m_sign != 0) {
            BigIntStorage q, r;
            div_mod_core(q, r, cur, radix_st);
            uint64_t rem = (r.m_size > 0) ? r.m_data[0] : 0;
            chunks.push_back(rem);
            cur = std::move(q);
        }

        std::string result;
        if (a.m_sign < 0) {
            result.push_back('-');
        }
        // 最高位 chunk 不補前導 0
        result += std::to_string(chunks.back());
        // 其餘 chunk 補齊 19 位
        for (size_t i = chunks.size() - 1; i > 0; --i) {
            std::string s = std::to_string(chunks[i - 1]);
            if (s.size() < 19) {
                result.append(19 - s.size(), '0');
            }
            result += s;
        }
        return result;
    }
};

} // namespace detail
} // namespace numeric
