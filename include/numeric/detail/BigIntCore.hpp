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
#include <cassert>
#include <mutex>
#include <atomic>

#if defined(_MSC_VER)
#include <intrin.h>
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__)
#include <immintrin.h>
#endif

namespace numeric {
namespace detail {

/// <summary>
/// BigInt 內部儲存層，採用 256-bit Small Buffer Optimization (SBO) 架構。
/// 內建 4 個 64-bit limbs 緩衝區，當數值在 256 位元以內時達成 0 堆積記憶體配置。
/// 整體結構大小在 64 位元架構下剛好對齊單一 64-Byte CPU L1 Cache Line。
/// </summary>
class BigIntStorage {
public:
    static constexpr size_t SBO_CAPACITY = 4;

    uint64_t m_sbo[SBO_CAPACITY];
    uint64_t* m_heap;
    size_t m_size;
    size_t m_capacity;
    int8_t m_sign;

    /// <summary>
    /// 拷貝指定數量之 limbs，支援編譯期 constexpr 運算。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void copy_limbs(uint64_t* dst, const uint64_t* src, size_t count) noexcept {
        for (size_t i = 0; i < count; ++i) {
            dst[i] = src[i];
        }
    }

    /// <summary>
    /// 將指定數量之 limbs 清零，支援編譯期 constexpr 運算。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void zero_limbs(uint64_t* dst, size_t count) noexcept {
        for (size_t i = 0; i < count; ++i) {
            dst[i] = 0;
        }
    }

    /// <summary>
    /// 檢查當前是否使用 SBO 內建緩衝區儲存。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 bool is_sbo() const noexcept {
        return m_heap == nullptr;
    }

    /// <summary>
    /// 取得 limbs 資料指標（可修改）。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 uint64_t* data() noexcept {
        return (m_heap != nullptr) ? m_heap : m_sbo;
    }

    /// <summary>
    /// 取得 limbs 資料常數指標。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 const uint64_t* data() const noexcept {
        return (m_heap != nullptr) ? m_heap : m_sbo;
    }

    /// <summary>
    /// 下標運算子，直接存取指定索引之 limb。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 uint64_t& operator[](size_t idx) noexcept {
        return data()[idx];
    }

    /// <summary>
    /// 下標常數運算子，直接唯讀存取指定索引之 limb。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 const uint64_t& operator[](size_t idx) const noexcept {
        return data()[idx];
    }

    /// <summary>
    /// 釋放堆積緩衝區並將內部資料重置回 SBO。
    /// </summary>
    NUMERIC_CONSTEXPR_20 void reset_heap() noexcept {
        if (m_heap != nullptr) {
            delete[] m_heap;
            m_heap = nullptr;
        }
        m_capacity = SBO_CAPACITY;
    }

    /// <summary>
    /// 預設建構子：初始化為零值，使用 SBO 緩衝區。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntStorage() noexcept
        : m_sbo{0, 0, 0, 0}, m_heap(nullptr), m_size(0), m_capacity(SBO_CAPACITY), m_sign(0) {}

    /// <summary>
    /// 解構子：若已配置堆積記憶體則進行釋放。
    /// </summary>
    NUMERIC_CONSTEXPR_20 ~BigIntStorage() noexcept {
        reset_heap();
    }

    /// <summary>
    /// 複製建構子：深拷貝另一儲存物件之 limbs。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntStorage(const BigIntStorage& other)
        : m_sbo{other.m_sbo[0], other.m_sbo[1], other.m_sbo[2], other.m_sbo[3]},
          m_heap(nullptr), m_size(other.m_size), m_capacity(SBO_CAPACITY), m_sign(other.m_sign) {
        if (!other.is_sbo()) {
            m_capacity = other.m_capacity;
            m_heap = new uint64_t[m_capacity];
            copy_limbs(m_heap, other.m_heap, other.m_size);
        }
    }

    /// <summary>
    /// 移動建構子：轉移堆積緩衝區擁有權，或拷貝 SBO 內容，確保來源安全復位。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntStorage(BigIntStorage&& other) noexcept
        : m_sbo{other.m_sbo[0], other.m_sbo[1], other.m_sbo[2], other.m_sbo[3]},
          m_heap(other.m_heap), m_size(other.m_size), m_capacity(other.m_capacity), m_sign(other.m_sign) {
        other.m_heap = nullptr;
        other.m_size = 0;
        other.m_capacity = SBO_CAPACITY;
        other.m_sign = 0;
        other.m_sbo[0] = 0;
        other.m_sbo[1] = 0;
        other.m_sbo[2] = 0;
        other.m_sbo[3] = 0;
    }

    /// <summary>
    /// 複製賦值運算子：提供強例外安全保證 (Strong Exception Guarantee)。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntStorage& operator=(const BigIntStorage& other) {
        if (this != &other) {
            if (other.is_sbo()) {
                reset_heap();
                copy_limbs(m_sbo, other.m_sbo, SBO_CAPACITY);
            } else {
                if (m_capacity < other.m_size || is_sbo()) {
                    uint64_t* new_data = new uint64_t[other.m_capacity];
                    reset_heap();
                    m_heap = new_data;
                    m_capacity = other.m_capacity;
                }
                copy_limbs(m_heap, other.m_heap, other.m_size);
            }
            m_size = other.m_size;
            m_sign = other.m_sign;
        }
        return *this;
    }

    /// <summary>
    /// 移動賦值運算子：轉移緩衝區資源。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntStorage& operator=(BigIntStorage&& other) noexcept {
        if (this != &other) {
            reset_heap();
            if (other.is_sbo()) {
                copy_limbs(m_sbo, other.m_sbo, SBO_CAPACITY);
                m_capacity = SBO_CAPACITY;
            } else {
                m_heap = other.m_heap;
                m_capacity = other.m_capacity;
                other.m_heap = nullptr;
                other.m_capacity = SBO_CAPACITY;
            }
            m_size = other.m_size;
            m_sign = other.m_sign;
            other.m_size = 0;
            other.m_sign = 0;
            other.m_sbo[0] = 0;
            other.m_sbo[1] = 0;
            other.m_sbo[2] = 0;
            other.m_sbo[3] = 0;
        }
        return *this;
    }

    /// <summary>
    /// 預留緩衝區容量。
    /// </summary>
    NUMERIC_CONSTEXPR_20 void reserve(size_t new_cap) {
        if (new_cap <= m_capacity) return;
        uint64_t* new_heap = new uint64_t[new_cap];
        if (m_size > 0) {
            copy_limbs(new_heap, data(), m_size);
        }
        if (m_heap != nullptr) {
            delete[] m_heap;
        }
        m_heap = new_heap;
        m_capacity = new_cap;
    }

    /// <summary>
    /// 調整 limbs 數量大小並可選填預設值。
    /// </summary>
    NUMERIC_CONSTEXPR_20 void resize(size_t new_size, uint64_t init_val = 0) {
        if (new_size > m_capacity) {
            size_t next_cap = m_capacity * 2;
            if (next_cap < new_size) next_cap = new_size;
            reserve(next_cap);
        }
        uint64_t* d = data();
        if (new_size > m_size) {
            for (size_t i = m_size; i < new_size; ++i) {
                d[i] = init_val;
            }
        }
        m_size = new_size;
    }

    /// <summary>
    /// 規範化 limbs 陣列，移除高位無效之 0 limbs 並調整正負符號；若長度落回 SBO 則縮回 SBO。
    /// </summary>
    NUMERIC_CONSTEXPR_20 void normalize() noexcept {
        uint64_t* d = data();
        while (m_size > 0 && d[m_size - 1] == 0) {
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
            uint64_t* old_heap = m_heap;
            for (size_t i = 0; i < SBO_CAPACITY; ++i) {
                m_sbo[i] = (i < m_size) ? old_heap[i] : 0;
            }
            m_heap = nullptr;
            m_capacity = SBO_CAPACITY;
            delete[] old_heap;
        }
    }

    /// <summary>
    /// 清空數值為 0。
    /// </summary>
    NUMERIC_CONSTEXPR_20 void clear() noexcept {
        m_size = 0;
        m_sign = 0;
    }

    /// <summary>
    /// 設定為 64 位元無符號整數與指定正負號。
    /// </summary>
    NUMERIC_CONSTEXPR_20 void set_uint64(uint64_t val, int8_t sign) noexcept {
        reset_heap();
        for (size_t i = 0; i < SBO_CAPACITY; ++i) {
            m_sbo[i] = 0;
        }
        if (val == 0) {
            m_size = 0;
            m_sign = 0;
        } else {
            m_size = 1;
            m_sign = sign;
            m_sbo[0] = val;
        }
    }
};

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
static_assert(sizeof(BigIntStorage) == 64, "BigIntStorage must be exactly 64 bytes on 64-bit platforms!");
#endif

/// <summary>
/// BigInt 演算法核心類別，提供無符號與有符號之任意精度運算。
/// </summary>
class BigIntCore {
public:
    static constexpr size_t KARATSUBA_THRESHOLD = 16;
    static constexpr size_t FROM_STRING_DC_THRESHOLD = 10;

    /// <summary>
    /// 64-bit ADC 原語：out = a + b + carry_in，回傳 carry_out (0 或 1)。
    /// 消除 C++ 純量條件判斷分支，並支援 C++20 constexpr 常數求值。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE uint8_t adc64(
        uint8_t carry_in, uint64_t a, uint64_t b, uint64_t* out) noexcept
    {
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            uint64_t sum = a + carry_in;
            uint8_t c1 = static_cast<uint8_t>(sum < a);
            sum += b;
            uint8_t c2 = static_cast<uint8_t>(sum < b);
            *out = sum;
            return c1 | c2;
        }
#endif

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        unsigned __int64 temp = 0;
        uint8_t c = _addcarry_u64(carry_in, a, b, &temp);
        *out = static_cast<uint64_t>(temp);
        return c;
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__)
        unsigned long long temp = 0;
        uint8_t c = _addcarry_u64(carry_in, a, b, &temp);
        *out = static_cast<uint64_t>(temp);
        return c;
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__SIZEOF_INT128__)
        unsigned __int128 res = static_cast<unsigned __int128>(a) + b + carry_in;
        *out = static_cast<uint64_t>(res);
        return static_cast<uint8_t>(res >> 64);
#else
        uint64_t sum = a + carry_in;
        uint8_t c1 = static_cast<uint8_t>(sum < a);
        sum += b;
        uint8_t c2 = static_cast<uint8_t>(sum < b);
        *out = sum;
        return c1 | c2;
#endif
    }

    /// <summary>
    /// 64-bit SBB 原語：out = a - b - borrow_in，回傳 borrow_out (0 或 1)。
    /// 消除 C++ 純量條件判斷分支，並支援 C++20 constexpr 常數求值。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE uint8_t sbb64(
        uint8_t borrow_in, uint64_t a, uint64_t b, uint64_t* out) noexcept
    {
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            uint64_t diff = a - borrow_in;
            uint8_t b1 = static_cast<uint8_t>(a < borrow_in);
            uint64_t diff2 = diff - b;
            uint8_t b2 = static_cast<uint8_t>(diff < b);
            *out = diff2;
            return b1 | b2;
        }
#endif

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
        unsigned __int64 temp = 0;
        uint8_t b_out = _subborrow_u64(borrow_in, a, b, &temp);
        *out = static_cast<uint64_t>(temp);
        return b_out;
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__)
        unsigned long long temp = 0;
        uint8_t b_out = _subborrow_u64(borrow_in, a, b, &temp);
        *out = static_cast<uint64_t>(temp);
        return b_out;
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__SIZEOF_INT128__)
        unsigned __int128 res = static_cast<unsigned __int128>(a) - b - borrow_in;
        *out = static_cast<uint64_t>(res);
        return static_cast<uint8_t>((res >> 127) & 1);
#else
        uint64_t diff = a - borrow_in;
        uint8_t b1 = static_cast<uint8_t>(a < borrow_in);
        uint64_t diff2 = diff - b;
        uint8_t b2 = static_cast<uint8_t>(diff < b);
        *out = diff2;
        return b1 | b2;
#endif
    }

    /// <summary>
    /// 2-Digit 快速十進位查詢表（200 位元組，長駐 L1 Cache）。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE const char* get_digit_pairs() noexcept {
        return
            "00010203040506070809"
            "10111213141516171819"
            "20212223242526272829"
            "30313233343536373839"
            "40414243444546474849"
            "50515253545556575859"
            "60616263646566676869"
            "70717273747576777879"
            "80818283848586878889"
            "90919293949596979899";
    }

    /// <summary>
    /// 支援完整 64 位元無符號整數（最大 18446744073709551615，共 20 位）之精準十進位位數判定。
    /// 補齊 10^16 二分階梯，杜絕任何緩衝區溢位。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE size_t digits10_u64(uint64_t v) noexcept {
        size_t d = 1;
        if (v >= 10000000000000000ULL) { v /= 10000000000000000ULL; d += 16; } // 10^16
        if (v >= 100000000ULL)         { v /= 100000000ULL;         d += 8;  } // 10^8
        if (v >= 10000ULL)             { v /= 10000ULL;             d += 4;  } // 10^4
        if (v >= 100ULL)               { v /= 100ULL;               d += 2;  } // 10^2
        if (v >= 10ULL)                { d += 1; }
        return d;
    }

    /// <summary>
    /// 格式化最高位 chunk：直接利用已知的 top_digits 由尾向頭倒序填寫，免除重複除法與二次長度搜尋。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE void format_highest_chunk(
        char* dst, uint64_t val, size_t digits) noexcept
    {
        const char* pairs = get_digit_pairs();
        int pos = static_cast<int>(digits);
        while (val >= 100) {
            uint32_t rem = static_cast<uint32_t>(val % 100);
            val /= 100;
            pos -= 2;
            dst[pos]     = pairs[rem * 2];
            dst[pos + 1] = pairs[rem * 2 + 1];
        }
        if (val < 10) {
            dst[--pos] = static_cast<char>('0' + val);
        } else {
            pos -= 2;
            dst[pos]     = pairs[val * 2];
            dst[pos + 1] = pairs[val * 2 + 1];
        }
        assert(pos == 0 && "format_highest_chunk failed to match exact digit count");
    }

    /// <summary>
    /// 格式化中間 19 位 fixed-width chunk：倒序逆向填充 9 組 LUT 雙字元加上 1 個最高位單字元。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE void format_chunk_19_digits(
        char* dst, uint64_t val) noexcept
    {
        const char* pairs = get_digit_pairs();
        for (int p = 8; p >= 0; --p) {
            uint32_t rem = static_cast<uint32_t>(val % 100);
            val /= 100;
            dst[1 + p * 2]     = pairs[rem * 2];
            dst[1 + p * 2 + 1] = pairs[rem * 2 + 1];
        }
        dst[0] = static_cast<char>('0' + val);
    }

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
#elif defined(_MSC_VER) && defined(_M_ARM64)
        hi = __umulh(a, b);
        return a * b;
#elif defined(_MSC_VER) && defined(_M_X64)
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
    /// 128 位元除以 64 位元整數之除法（前置合約：d != 0 且 hi 小於 d，防止觸發硬體 #DE 異常中斷）。
    /// </summary>
    /// <param name="hi">被除數高 64 位元</param>
    /// <param name="lo">被除數低 64 位元</param>
    /// <param name="d">除數</param>
    /// <param name="rem">輸出餘數參考</param>
    /// <returns>商之 64 位元數值</returns>
    static NUMERIC_CONSTEXPR_20 uint64_t div128_64(uint64_t hi, uint64_t lo, uint64_t d, uint64_t& rem) noexcept {
        assert(d != 0 && "Division by zero!");
        assert(hi < d && "Hardware divide error: quotient exceeds 64 bits!");
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
#elif defined(_MSC_VER) && defined(_M_X64)
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
    /// 128 位元除以 10^19 (10000000000000000000) 之乘法求逆除法。
    /// 預算逆元常數 v = floor((2^128 - 1) / 10^19) - 2^64 = 0xd83c94fb6d2ac34a。
    /// 消除 _udiv128 硬體除法指令，將除法延遲自 ~40 cycles 降低至 ~6 cycles。
    /// </summary>
    static NUMERIC_CONSTEXPR_20_FORCEINLINE uint64_t div_recip_radix10_19(
        uint64_t nh, uint64_t nl, uint64_t& rem) noexcept
    {
        constexpr uint64_t d = 10000000000000000000ULL;
        constexpr uint64_t v = 0xd83c94fb6d2ac34aULL;

#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            return div128_64(nh, nl, d, rem);
        }
#endif

        uint64_t nh_v_hi = 0;
        uint64_t nh_v_lo = mul64_wide(nh, v, nh_v_hi);
        uint64_t nl_v_hi = 0;
        mul64_wide(nl, v, nl_v_hi);

        uint64_t mid1 = nh_v_lo + nl;
        uint64_t carry1 = (mid1 < nl) ? 1 : 0;
        uint64_t mid2 = mid1 + nl_v_hi;
        uint64_t carry2 = (mid2 < mid1) ? 1 : 0;

        uint64_t q_est = nh + nh_v_hi + carry1 + carry2;
        uint64_t r = nl - q_est * d;
        if (r >= d) {
            ++q_est;
            r -= d;
        }
        rem = r;
        return q_est;
    }

    /// <summary>
    /// 將 64 位元無符號整數格式化為十進位字元陣列寫入 buf（不含 null 結尾），回傳字元長度。
    /// 棧上無配置零開銷輔助函式。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 size_t format_uint64_to_buf(char* buf, uint64_t val) noexcept {
        if (val == 0) {
            buf[0] = '0';
            return 1;
        }
        char tmp[24];
        size_t pos = 0;
        while (val > 0) {
            tmp[pos++] = static_cast<char>('0' + (val % 10));
            val /= 10;
        }
        for (size_t i = 0; i < pos; ++i) {
            buf[i] = tmp[pos - 1 - i];
        }
        return pos;
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
    /// 256-bit SBO 靜態展開快路徑加法 (1 ~ 4 Limbs)。
    /// 安全零填充提取，純暫存器 ADC 流水線，延遲進位擴容，保證自我別名與全零規格化安全。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void add_unsigned_sbo4(
        BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) noexcept
    {
        const uint64_t* a_data = a.data();
        const uint64_t* b_data = b.data();
        size_t a_size = a.m_size;
        size_t b_size = b.m_size;

        const uint64_t a0 = (0 < a_size) ? a_data[0] : 0;
        const uint64_t a1 = (1 < a_size) ? a_data[1] : 0;
        const uint64_t a2 = (2 < a_size) ? a_data[2] : 0;
        const uint64_t a3 = (3 < a_size) ? a_data[3] : 0;

        const uint64_t b0 = (0 < b_size) ? b_data[0] : 0;
        const uint64_t b1 = (1 < b_size) ? b_data[1] : 0;
        const uint64_t b2 = (2 < b_size) ? b_data[2] : 0;
        const uint64_t b3 = (3 < b_size) ? b_data[3] : 0;

        uint64_t r0 = 0, r1 = 0, r2 = 0, r3 = 0;
        uint8_t c = 0;
        c = adc64(c, a0, b0, &r0);
        c = adc64(c, a1, b1, &r1);
        c = adc64(c, a2, b2, &r2);
        c = adc64(c, a3, b3, &r3);

        if (c == 0) {
            res.reset_heap();
            res.m_sbo[0] = r0;
            res.m_sbo[1] = r1;
            res.m_sbo[2] = r2;
            res.m_sbo[3] = r3;
            size_t s = 4;
            while (s > 0 && res.m_sbo[s - 1] == 0) --s;
            res.m_size = s;
            if (s == 0) {
                res.m_sign = 0;
            }
        } else {
            res.resize(5, 0);
            uint64_t* d = res.data();
            d[0] = r0; d[1] = r1; d[2] = r2; d[3] = r3;
            d[4] = 1;
            res.m_size = 5;
        }
    }

    /// <summary>
    /// 多肢段通用加法，含別名指針備份防護、空間預留與長邊 Early-Exit 進位鏈。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void add_unsigned_general(
        BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) noexcept
    {
        size_t a_len = a.m_size;
        size_t b_len = b.m_size;
        size_t min_len = std::min(a_len, b_len);
        size_t max_len = std::max(a_len, b_len);

        uint64_t a_stack[128], b_stack[128];
        const uint64_t* a_ptr = a.data();
        const uint64_t* b_ptr = b.data();
        std::vector<uint64_t> a_heap, b_heap;

        if (&res == &a && a_len < b_len) {
            if (a_len <= 128) {
                std::copy_n(a.data(), a_len, a_stack);
                a_ptr = a_stack;
            } else {
                a_heap.resize(a_len);
                std::copy_n(a.data(), a_len, a_heap.data());
                a_ptr = a_heap.data();
            }
        } else if (&res == &b && b_len < a_len) {
            if (b_len <= 128) {
                std::copy_n(b.data(), b_len, b_stack);
                b_ptr = b_stack;
            } else {
                b_heap.resize(b_len);
                std::copy_n(b.data(), b_len, b_heap.data());
                b_ptr = b_heap.data();
            }
        }

        if (&res != &a && &res != &b) {
            res.resize(max_len + 1, 0);
        } else if ((&res == &a && a_len < b_len) || (&res == &b && b_len < a_len)) {
            res.resize(max_len + 1, 0);
        }

        const uint64_t* longer_ptr = (a_len >= b_len) ? a_ptr : b_ptr;

        uint8_t carry = 0;
        for (size_t i = 0; i < min_len; ++i) {
            carry = adc64(carry, a_ptr[i], b_ptr[i], &res.data()[i]);
        }

        size_t i = min_len;
        for (; i < max_len && carry != 0; ++i) {
            carry = adc64(carry, longer_ptr[i], 0, &res.data()[i]);
        }

        if (carry == 0) {
            if (res.data() != longer_ptr && i < max_len) {
                std::copy_n(longer_ptr + i, max_len - i, res.data() + i);
            }
            res.m_size = max_len;
        } else {
            if (res.m_size < max_len + 1) {
                res.resize(max_len + 1, 0);
            }
            res.data()[max_len] = 1;
            res.m_size = max_len + 1;
        }
    }

    /// <summary>
    /// 無符號 limbs 加法：res = a + b。純數值 (Magnitude) 運算。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void add_unsigned(
        BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) noexcept
    {
        if (a.m_size <= BigIntStorage::SBO_CAPACITY && b.m_size <= BigIntStorage::SBO_CAPACITY) {
            add_unsigned_sbo4(res, a, b);
        } else {
            add_unsigned_general(res, a, b);
        }
    }

    /// <summary>
    /// 絕對值減法核心演算法：res = a - b (前置合約：|a| >= |b|)。
    /// 支援小運算元別名備份 (&res == &b)、前置擴容與由高位向下快速規格化。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void sub_magnitude_core(
        BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) noexcept
    {
        size_t a_len = a.m_size;
        size_t b_len = b.m_size;

        uint64_t b_stack[128];
        const uint64_t* b_data = b.data();
        std::vector<uint64_t> b_heap;
        if (&res == &b) {
            if (b_len <= 128) {
                std::copy_n(b.data(), b_len, b_stack);
                b_data = b_stack;
            } else {
                b_heap.resize(b_len);
                std::copy_n(b.data(), b_len, b_heap.data());
                b_data = b_heap.data();
            }
        }

        if (&res != &a) {
            res.resize(a_len, 0);
        }

        uint8_t borrow = 0;
        for (size_t i = 0; i < b_len; ++i) {
            borrow = sbb64(borrow, a.data()[i], b_data[i], &res.data()[i]);
        }
        size_t i = b_len;
        for (; i < a_len && borrow != 0; ++i) {
            borrow = sbb64(borrow, a.data()[i], 0, &res.data()[i]);
        }
        if (borrow == 0 && &res != &a && i < a_len) {
            std::copy_n(a.data() + i, a_len - i, res.data() + i);
        }

        size_t sz = a_len;
        while (sz > 0 && res.data()[sz - 1] == 0) --sz;
        res.m_size = sz;
        if (sz == 0) {
            res.m_sign = 0;
        }
        if (sz <= BigIntStorage::SBO_CAPACITY) {
            res.shrink_to_sbo_if_possible();
        }
    }

    /// <summary>
    /// 無符號 limbs 減法：res = a - b（前置條件：a >= b）。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void sub_unsigned(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) noexcept {
        sub_magnitude_core(res, a, b);
        res.m_sign = (res.m_size > 0) ? 1 : 0;
    }

    /// <summary>
    /// 帶符號加法：res = a + b。
    /// </summary>
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
            res.m_sign = (res.m_size > 0) ? a.m_sign : 0;
        } else {
            int cmp = compare_unsigned(a.data(), a.m_size, b.data(), b.m_size);
            if (cmp == 0) {
                res.reset_heap();
                res.m_size = 0;
                res.m_sign = 0;
            } else if (cmp > 0) {
                sub_magnitude_core(res, a, b);
                res.m_sign = (res.m_size > 0) ? a.m_sign : 0;
            } else {
                sub_magnitude_core(res, b, a);
                res.m_sign = (res.m_size > 0) ? b.m_sign : 0;
            }
        }
    }

    /// <summary>
    /// 帶符號減法：res = a - b。
    /// </summary>
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
        if (a.m_sign != b.m_sign) {
            add_unsigned(res, a, b);
            res.m_sign = (res.m_size > 0) ? a.m_sign : 0;
        } else {
            int cmp = compare_unsigned(a.data(), a.m_size, b.data(), b.m_size);
            if (cmp == 0) {
                res.reset_heap();
                res.m_size = 0;
                res.m_sign = 0;
            } else if (cmp > 0) {
                sub_magnitude_core(res, a, b);
                res.m_sign = (res.m_size > 0) ? a.m_sign : 0;
            } else {
                sub_magnitude_core(res, b, a);
                res.m_sign = (res.m_size > 0) ? -b.m_sign : 0;
            }
        }
    }

    /// <summary>
    /// 原生 Schoolbook 乘法：out = a * b。out 長度至少為 a_len + b_len。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void mul_schoolbook_raw(uint64_t* NUMERIC_RESTRICT out,
                                                        const uint64_t* a, size_t a_len,
                                                        const uint64_t* b, size_t b_len) noexcept {
        BigIntStorage::zero_limbs(out, a_len + b_len);
        for (size_t i = 0; i < a_len; ++i) {
            if (a[i] == 0) continue;
            uint64_t carry = 0;
            for (size_t j = 0; j < b_len; ++j) {
                uint64_t hi = 0;
                uint64_t lo = mul64_wide(a[i], b[j], hi);
                uint64_t cur = out[i + j];
                uint64_t sum = cur + lo;
                uint64_t c1 = (sum < cur) ? 1 : 0;
                sum += carry;
                uint64_t c2 = (sum < carry) ? 1 : 0;
                out[i + j] = sum;
                carry = hi + c1 + c2;
            }
            out[i + b_len] += carry;
        }
    }

    /// <summary>
    /// 原生原地加法：dst += src。回傳溢出進位。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 uint64_t add_to_raw(uint64_t* dst, size_t dst_len, const uint64_t* src, size_t src_len) noexcept {
        uint8_t carry = 0;
        size_t min_len = (dst_len < src_len) ? dst_len : src_len;
        for (size_t i = 0; i < min_len; ++i) {
            carry = adc64(carry, dst[i], src[i], &dst[i]);
        }
        for (size_t i = min_len; i < dst_len && carry != 0; ++i) {
            carry = adc64(carry, dst[i], 0, &dst[i]);
        }
        return carry;
    }

    /// <summary>
    /// 原生原地減法：dst -= src。回傳借位。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 uint64_t sub_from_raw(uint64_t* dst, size_t dst_len, const uint64_t* src, size_t src_len) noexcept {
        uint8_t borrow = 0;
        size_t min_len = (dst_len < src_len) ? dst_len : src_len;
        for (size_t i = 0; i < min_len; ++i) {
            borrow = sbb64(borrow, dst[i], src[i], &dst[i]);
        }
        for (size_t i = min_len; i < dst_len && borrow != 0; ++i) {
            borrow = sbb64(borrow, dst[i], 0, &dst[i]);
        }
        return borrow;
    }

    /// <summary>
    /// 原生相減差值絕對值：out = a - b (要求 a >= b)。回傳正規化長度。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 size_t sub_limbs_diff_raw(uint64_t* out, const uint64_t* a, size_t a_len, const uint64_t* b, size_t b_len) noexcept {
        uint8_t borrow = 0;
        for (size_t i = 0; i < a_len; ++i) {
            uint64_t bv = (i < b_len) ? b[i] : 0;
            borrow = sbb64(borrow, a[i], bv, &out[i]);
        }
        size_t len = a_len;
        while (len > 0 && out[len - 1] == 0) {
            --len;
        }
        return len;
    }

    /// <summary>
    /// 線程局部暫存記憶體池（Bump Allocator Scratch Arena）。
    /// 提供 Karatsuba 乘法與長整數運算無鎖、0 堆積重配置之 scratch 空間。
    /// 支援 RAII Scope 機制，遞迴或巢狀調用時自動安全回滾偏移指標。
    /// </summary>
    class ScratchArena {
    public:
        static ScratchArena& instance() noexcept {
            static thread_local ScratchArena arena;
            return arena;
        }

        class Scope {
        public:
            explicit NUMERIC_CONSTEXPR_20 Scope(ScratchArena& arena, bool active = true) noexcept
                : m_arena(active ? &arena : nullptr),
                  m_saved_offset(active ? arena.m_offset : 0) {}

            NUMERIC_CONSTEXPR_20 ~Scope() noexcept {
                if (m_arena) {
                    m_arena->m_offset = m_saved_offset;
                }
            }

            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;
            Scope(Scope&&) = delete;
            Scope& operator=(Scope&&) = delete;

        private:
            ScratchArena* m_arena;
            size_t m_saved_offset;
        };

        uint64_t* allocate(size_t count) {
            if (m_offset + count > m_buffer.size()) {
                size_t new_cap = (std::max)(m_buffer.size() * 2, m_offset + count);
                new_cap = (std::max)(new_cap, static_cast<size_t>(4096));
                m_buffer.resize(new_cap);
            }
            uint64_t* ptr = m_buffer.data() + m_offset;
            m_offset += count;
            return ptr;
        }

    private:
        ScratchArena() = default;
        std::vector<uint64_t> m_buffer;
        size_t m_offset = 0;
    };

    /// <summary>
    /// 減法變體 Scratchpad Karatsuba 核心演算法：out = a * b。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void mul_karatsuba_raw(uint64_t* NUMERIC_RESTRICT out,
                                                       const uint64_t* a, size_t a_len,
                                                       const uint64_t* b, size_t b_len,
                                                       uint64_t* scratch) noexcept {
        if (a_len < KARATSUBA_THRESHOLD || b_len < KARATSUBA_THRESHOLD) {
            mul_schoolbook_raw(out, a, a_len, b, b_len);
            return;
        }

        size_t n = (a_len > b_len) ? a_len : b_len;
        size_t m = (n + 1) / 2;

        const uint64_t* a0 = a;
        size_t a0_len = (a_len < m) ? a_len : m;
        while (a0_len > 0 && a0[a0_len - 1] == 0) --a0_len;

        const uint64_t* a1 = a + m;
        size_t a1_len = (a_len > m) ? (a_len - m) : 0;
        while (a1_len > 0 && a1[a1_len - 1] == 0) --a1_len;

        const uint64_t* b0 = b;
        size_t b0_len = (b_len < m) ? b_len : m;
        while (b0_len > 0 && b0[b0_len - 1] == 0) --b0_len;

        const uint64_t* b1 = b + m;
        size_t b1_len = (b_len > m) ? (b_len - m) : 0;
        while (b1_len > 0 && b1[b1_len - 1] == 0) --b1_len;

        // Scratchpad 分配：
        // da: m
        // db: m
        // p:  2*m
        // t:  2*m + 2
        // next_scratch: scratch + 6*m + 2
        uint64_t* da = scratch;
        uint64_t* db = da + m;
        uint64_t* p = db + m;
        uint64_t* t = p + (2 * m);
        uint64_t* next_scratch = t + (2 * m + 2);

        // 1. z0 = a0 * b0 寫入 out[0 .. 2*m - 1]
        BigIntStorage::zero_limbs(out, a_len + b_len);
        size_t z0_len = 0;
        if (a0_len > 0 && b0_len > 0) {
            z0_len = a0_len + b0_len;
            mul_karatsuba_raw(out, a0, a0_len, b0, b0_len, next_scratch);
        }

        // 2. z2 = a1 * b1 寫入 out[2*m .. a_len + b_len - 1]
        size_t z2_len = 0;
        if (a1_len > 0 && b1_len > 0) {
            z2_len = a1_len + b1_len;
            mul_karatsuba_raw(out + 2 * m, a1, a1_len, b1, b1_len, next_scratch);
        }

        // 3. 計算 Da = |a0 - a1|, Db = |b0 - b1|
        int cmp_a = compare_unsigned(a0, a0_len, a1, a1_len);
        int8_t s_a = 0;
        size_t da_len = 0;
        if (cmp_a > 0) {
            s_a = 1;
            da_len = sub_limbs_diff_raw(da, a0, a0_len, a1, a1_len);
        } else if (cmp_a < 0) {
            s_a = -1;
            da_len = sub_limbs_diff_raw(da, a1, a1_len, a0, a0_len);
        }

        int cmp_b = compare_unsigned(b0, b0_len, b1, b1_len);
        int8_t s_b = 0;
        size_t db_len = 0;
        if (cmp_b > 0) {
            s_b = 1;
            db_len = sub_limbs_diff_raw(db, b0, b0_len, b1, b1_len);
        } else if (cmp_b < 0) {
            s_b = -1;
            db_len = sub_limbs_diff_raw(db, b1, b1_len, b0, b0_len);
        }

        int8_t s_p = static_cast<int8_t>(s_a * s_b);
        size_t p_len = 0;
        if (s_p != 0 && da_len > 0 && db_len > 0) {
            p_len = da_len + db_len;
            mul_karatsuba_raw(p, da, da_len, db, db_len, next_scratch);
        } else {
            s_p = 0;
        }

        // 4. 在 t 緩衝區中合成 z1 = z0 + z2 - s_p * p
        if (z0_len > 2 * m) z0_len = 2 * m;
        if (z0_len > 0) {
            BigIntStorage::copy_limbs(t, out, z0_len);
        }
        BigIntStorage::zero_limbs(t + z0_len, (2 * m + 2) - z0_len);

        // 累加 z2
        if (z2_len > 0) {
            add_to_raw(t, 2 * m + 2, out + 2 * m, z2_len);
        }

        // 減法變體符號修正：
        if (s_p > 0 && p_len > 0) {
            sub_from_raw(t, 2 * m + 2, p, p_len);
        } else if (s_p < 0 && p_len > 0) {
            add_to_raw(t, 2 * m + 2, p, p_len);
        }

        // 5. 將 z1 累加至 out + m
        add_to_raw(out + m, (a_len + b_len) - m, t, 2 * m + 2);
    }

    /// <summary>
    /// 傳統 Schoolbook 長整數乘法演算法。
    /// </summary>
    /// <param name="res">輸出乘積儲存物件</param>
    /// <param name="a">乘數 a</param>
    /// <param name="b">乘數 b</param>
    static NUMERIC_CONSTEXPR_20 void mul_schoolbook(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (a.m_size == 0 || b.m_size == 0) {
            res.reset_heap();
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        if (&res == &a || &res == &b) {
            BigIntStorage tmp;
            tmp.resize(a.m_size + b.m_size, 0);
            mul_schoolbook_raw(tmp.data(), a.data(), a.m_size, b.data(), b.m_size);
            tmp.m_sign = 1;
            tmp.normalize();
            res = std::move(tmp);
        } else {
            res.resize(a.m_size + b.m_size, 0);
            mul_schoolbook_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size);
            res.m_sign = 1;
            res.normalize();
        }
    }

    /// <summary>
    /// 單肢段非平衡乘法：res = a * b_val（含自我別名 &res == &a 防護）。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void mul_single_limb(BigIntStorage& res, const BigIntStorage& a, uint64_t b_val, int8_t sign) {
        if (b_val == 0 || a.m_size == 0) {
            res.reset_heap();
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        if (b_val == 1) {
            if (&res != &a) {
                res = a;
            }
            res.m_sign = sign;
            return;
        }

        size_t n = a.m_size;
        // 自我別名防護：先一次性確認容量足夠，絕不允許在掃描迴圈中途擴容導致指針懸置
        if (res.m_capacity < n + 1) {
            res.reserve(n + 1);
        }

        uint64_t carry = 0;
        for (size_t i = 0; i < n; ++i) {
            uint64_t high = 0;
            uint64_t low = mul64_wide(a.data()[i], b_val, high);
            low += carry;
            if (low < carry) ++high;
            res.data()[i] = low;
            carry = high;
        }
        if (carry > 0) {
            res.data()[n] = carry;
            res.m_size = n + 1;
        } else {
            res.m_size = n;
        }
        res.m_sign = sign;
    }

    /// <summary>
    /// Karatsuba 快速乘法演算法（減法變體 + Scratchpad 記憶體規劃）。
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

        // 自我別名防護
        if (&res == &a || &res == &b) {
            BigIntStorage tmp;
            mul_karatsuba(tmp, a, b);
            res = std::move(tmp);
            return;
        }

        size_t total_len = a.m_size + b.m_size;
        res.resize(total_len, 0);

        // Scratchpad 預估大小：8 * n + 128 limbs
        size_t scratch_size = 8 * n + 128;
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            if (scratch_size <= 1024) {
                uint64_t stack_scratch[1024];
                mul_karatsuba_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size, stack_scratch);
            } else {
                std::vector<uint64_t> heap_scratch(scratch_size);
                mul_karatsuba_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size, heap_scratch.data());
            }
            res.m_sign = 1;
            res.normalize();
            return;
        }
#endif
        if (scratch_size <= 1024) {
            uint64_t stack_scratch[1024];
            mul_karatsuba_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size, stack_scratch);
        } else {
            auto& arena = ScratchArena::instance();
            ScratchArena::Scope scope(arena);
            uint64_t* scratch = arena.allocate(scratch_size);
            mul_karatsuba_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size, scratch);
        }

        res.m_sign = 1;
        res.normalize();
    }

    /// <summary>
    /// 內部乘法派發函式，依據位數自動切換 Schoolbook 與 Karatsuba。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">乘數 a</param>
    /// <param name="b">乘數 b</param>
    static NUMERIC_CONSTEXPR_20 void mul_core(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        if (a.m_size < KARATSUBA_THRESHOLD || b.m_size < KARATSUBA_THRESHOLD ||
            a.m_size >= 2 * b.m_size || b.m_size >= 2 * a.m_size) {
            mul_schoolbook(res, a, b);
        } else {
            mul_karatsuba(res, a, b);
        }
    }

    /// <summary>
    /// 帶符號乘法：res = a * b。
    /// 包含 M0 (零值快路徑與非平衡乘法)、M1 (單肢段原生直通)、M2/M3 (Schoolbook/Karatsuba)。
    /// </summary>
    /// <param name="res">輸出結果</param>
    /// <param name="a">乘數 a</param>
    /// <param name="b">乘數 b</param>
    static NUMERIC_CONSTEXPR_20 void mul_signed(BigIntStorage& res, const BigIntStorage& a, const BigIntStorage& b) {
        // M0: 零值快路徑
        if (a.m_sign == 0 || b.m_sign == 0 || a.m_size == 0 || b.m_size == 0) {
            res.reset_heap();
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }
        int8_t res_sign = static_cast<int8_t>(a.m_sign * b.m_sign);

        // M1: 1-Limb 原生直通
        if (a.m_size == 1 && b.m_size == 1) {
            uint64_t hi = 0;
            uint64_t lo = mul64_wide(a.data()[0], b.data()[0], hi);
            res.reset_heap();
            res.data()[0] = lo;
            if (hi > 0) {
                res.data()[1] = hi;
                res.m_size = 2;
            } else {
                res.m_size = 1;
            }
            res.m_sign = res_sign;
            return;
        }

        // 單肢段非平衡乘法
        if (a.m_size == 1) {
            mul_single_limb(res, b, a.data()[0], res_sign);
            return;
        }
        if (b.m_size == 1) {
            mul_single_limb(res, a, b.data()[0], res_sign);
            return;
        }

        // M2 & M3 派發
        mul_core(res, a, b);
        res.m_sign = res_sign;
    }

    static constexpr size_t BZ_THRESHOLD = 128; // Tuned threshold for Burnikel-Ziegler division

    /// <summary>
    /// Knuth Algorithm D 原地長除法（採用 Scratch Buffer 達成 16,384 位元內零 Heap 配置）。
    /// 依據 q_out 與 r_out 是否為空，完全消除不必要的商或餘數陣列存取與正規化開銷。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_mod_core_knuth(
        BigIntStorage* q_out, BigIntStorage* r_out,
        const BigIntStorage& u, const BigIntStorage& v)
    {
        if (v.m_size == 0) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("division by zero"));
        }
        if (u.m_size == 0 || u.m_size < v.m_size) {
            if (q_out) { q_out->m_size = 0; q_out->m_sign = 0; }
            if (r_out) { *r_out = u; r_out->m_sign = (u.m_size > 0 ? 1 : 0); }
            return;
        }
        int cmp = compare_unsigned(u.data(), u.m_size, v.data(), v.m_size);
        if (cmp < 0) {
            if (q_out) { q_out->m_size = 0; q_out->m_sign = 0; }
            if (r_out) { *r_out = u; r_out->m_sign = 1; }
            return;
        }
        if (cmp == 0) {
            if (q_out) { q_out->set_uint64(1, 1); }
            if (r_out) { r_out->m_size = 0; r_out->m_sign = 0; }
            return;
        }

        size_t n = v.m_size;
        size_t m = u.m_size - n;

        // D1: 正規化 (shift left by s bits)
        int s = clz64(v.data()[n - 1]);
        size_t total_scratch = (n + 1) + (u.m_size + 2);
        uint64_t stack_scratch[512];
        uint64_t* scratch_ptr = stack_scratch;
        std::vector<uint64_t> heap_scratch;
        if (total_scratch > 512) {
            heap_scratch.resize(total_scratch);
            scratch_ptr = heap_scratch.data();
        }

        uint64_t* vn = scratch_ptr;
        uint64_t* un = scratch_ptr + (n + 1);

        if (s == 0) {
            BigIntStorage::copy_limbs(vn, v.data(), n);
            BigIntStorage::copy_limbs(un, u.data(), u.m_size);
            un[u.m_size] = 0;
            un[u.m_size + 1] = 0;
        } else {
            uint64_t carry = 0;
            for (size_t i = 0; i < n; ++i) {
                uint64_t cur = v.data()[i];
                vn[i] = (cur << s) | carry;
                carry = cur >> (64 - s);
            }
            vn[n] = carry;

            carry = 0;
            for (size_t i = 0; i < u.m_size; ++i) {
                uint64_t cur = u.data()[i];
                un[i] = (cur << s) | carry;
                carry = cur >> (64 - s);
            }
            un[u.m_size] = carry;
            un[u.m_size + 1] = 0;
        }

        if (q_out) {
            q_out->resize(m + 1, 0);
        }

        uint64_t v_hi = vn[n - 1];
        uint64_t v_lo = vn[n - 2];

        // D2~D7: 主迴圈
        for (size_t k = m + 1; k > 0; --k) {
            size_t j = k - 1;
            uint64_t u_hi = un[j + n];
            uint64_t u_mid = un[j + n - 1];
            uint64_t u_lo = (j + n >= 2) ? un[j + n - 2] : 0;

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
                uint64_t p_lo = mul64_wide(q_hat, vn[i], p_hi);
                uint64_t p_full = p_lo + carry;
                uint64_t c1 = (p_full < p_lo) ? 1 : 0;
                carry = p_hi + c1;

                uint64_t cur = un[j + i];
                uint64_t diff = cur - borrow;
                uint64_t b1 = (cur < borrow) ? 1 : 0;
                uint64_t diff2 = diff - p_full;
                uint64_t b2 = (diff < p_full) ? 1 : 0;
                borrow = b1 + b2;
                un[j + i] = diff2;
            }

            uint64_t cur = un[j + n];
            uint64_t diff = cur - borrow;
            uint64_t b1 = (cur < borrow) ? 1 : 0;
            uint64_t diff2 = diff - carry;
            uint64_t b2 = (diff < carry) ? 1 : 0;
            un[j + n] = diff2;

            // D5: 判斷是否需要回加
            if (b1 + b2 > 0) {
                --q_hat;
                uint64_t add_carry = 0;
                for (size_t i = 0; i < n; ++i) {
                    uint64_t val = un[j + i];
                    uint64_t sum = val + vn[i] + add_carry;
                    add_carry = (sum < val || (add_carry && sum == val)) ? 1 : 0;
                    un[j + i] = sum;
                }
                un[j + n] += add_carry;
            }

            if (q_out) {
                q_out->data()[j] = q_hat;
            }
        }

        if (q_out) {
            q_out->m_sign = 1;
            q_out->normalize();
        }

        // D8: 去正規化餘數
        if (r_out) {
            r_out->resize(n, 0);
            if (s > 0) {
                for (size_t i = 0; i < n; ++i) {
                    uint64_t c = un[i];
                    uint64_t next = (i + 1 <= n) ? un[i + 1] : 0;
                    r_out->data()[i] = (c >> s) | (next << (64 - s));
                }
            } else {
                BigIntStorage::copy_limbs(r_out->data(), un, n);
            }
            r_out->m_sign = 1;
            r_out->normalize();
        }
    }

    /// <summary>
    /// Burnikel–Ziegler div3by2: 以 2k-limb 除數 b 除 3k-limb 被除數 a。
    /// 產出 k-limb 商 q 與 2k-limb 餘數 r。要求 a < b * beta^k。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void bz_div3by2(
        BigIntStorage* q, BigIntStorage* r,
        const BigIntStorage& a, const BigIntStorage& b, size_t k)
    {
        BigIntStorage b1, b0;
        b1.resize(k, 0);
        BigIntStorage::copy_limbs(b1.data(), b.data() + k, k);
        b1.m_sign = 1;
        b1.normalize();

        b0.resize(k, 0);
        BigIntStorage::copy_limbs(b0.data(), b.data(), k);
        b0.m_sign = 1;
        b0.normalize();

        BigIntStorage a_hi, a_lo;
        if (a.m_size > k) {
            size_t hi_len = a.m_size - k;
            a_hi.resize(hi_len, 0);
            BigIntStorage::copy_limbs(a_hi.data(), a.data() + k, hi_len);
            a_hi.m_sign = 1;
            a_hi.normalize();

            a_lo.resize(k, 0);
            BigIntStorage::copy_limbs(a_lo.data(), a.data(), k);
            a_lo.m_sign = 1;
            a_lo.normalize();
        } else {
            a_lo = a;
        }

        BigIntStorage q_hat, r_hat;
        if (a_hi.m_size == 0) {
            q_hat.m_size = 0;
            q_hat.m_sign = 0;
            r_hat.m_size = 0;
            r_hat.m_sign = 0;
        } else {
            bool a_hi_ge_b1_beta_k = false;
            if (a_hi.m_size > k + b1.m_size) {
                a_hi_ge_b1_beta_k = true;
            } else if (a_hi.m_size == k + b1.m_size) {
                a_hi_ge_b1_beta_k = (compare_unsigned(a_hi.data() + k, b1.m_size, b1.data(), b1.m_size) >= 0);
            }

            if (a_hi_ge_b1_beta_k) {
                q_hat.resize(k, 0xFFFFFFFFFFFFFFFFULL);
                q_hat.m_sign = 1;

                BigIntStorage b1_shifted;
                shift_left_limbs(b1_shifted, b1, k);
                BigIntStorage diff;
                sub_magnitude_core(diff, a_hi, b1_shifted);
                add_unsigned(r_hat, diff, b1);
                r_hat.m_sign = 1;
            } else {
                bz_div2by1(&q_hat, &r_hat, a_hi, b1);
            }
        }

        BigIntStorage d;
        mul_core(d, q_hat, b0);

        BigIntStorage r_prime;
        shift_left_limbs(r_prime, r_hat, k);
        if (a_lo.m_size > 0) {
            BigIntStorage tmp;
            add_unsigned(tmp, r_prime, a_lo);
            tmp.m_sign = 1;
            r_prime = std::move(tmp);
        }

        while (compare_unsigned(r_prime.data(), r_prime.m_size, d.data(), d.m_size) < 0) {
            BigIntStorage one; one.set_uint64(1, 1);
            BigIntStorage q_next;
            sub_magnitude_core(q_next, q_hat, one);
            q_next.m_sign = (q_next.m_size > 0) ? 1 : 0;
            q_hat = std::move(q_next);

            BigIntStorage r_next;
            add_unsigned(r_next, r_prime, b);
            r_next.m_sign = 1;
            r_prime = std::move(r_next);
        }

        BigIntStorage r_final;
        sub_magnitude_core(r_final, r_prime, d);
        r_final.m_sign = (r_final.m_size > 0) ? 1 : 0;

        if (q) *q = std::move(q_hat);
        if (r) *r = std::move(r_final);
    }

    /// <summary>
    /// Burnikel–Ziegler div2by1: 以 n-limb 除數 b 除 2n-limb 被除數 a。
    /// 要求 a < b * beta^n，n 為偶數。
    /// 遞迴調用 div3by2，以 Karatsuba 乘法降低計算複雜度至 O(M(N) log N)。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void bz_div2by1(
        BigIntStorage* q, BigIntStorage* r,
        const BigIntStorage& a, const BigIntStorage& b)
    {
        size_t n = b.m_size;
        if (n < BZ_THRESHOLD || (n % 2 != 0)) {
            div_mod_core_knuth(q, r, a, b);
            return;
        }

        size_t k = n / 2;
        BigIntStorage a_hi, a_lo;
        if (a.m_size > k) {
            size_t hi_len = a.m_size - k;
            a_hi.resize(hi_len, 0);
            BigIntStorage::copy_limbs(a_hi.data(), a.data() + k, hi_len);
            a_hi.m_sign = 1;
            a_hi.normalize();

            a_lo.resize(k, 0);
            BigIntStorage::copy_limbs(a_lo.data(), a.data(), k);
            a_lo.m_sign = 1;
            a_lo.normalize();
        } else {
            a_lo = a;
        }

        BigIntStorage q1, r1;
        bz_div3by2(&q1, &r1, a_hi, b, k);

        BigIntStorage a_step2;
        shift_left_limbs(a_step2, r1, k);
        if (a_lo.m_size > 0) {
            BigIntStorage tmp;
            add_unsigned(tmp, a_step2, a_lo);
            tmp.m_sign = 1;
            a_step2 = std::move(tmp);
        }

        BigIntStorage q0, r0;
        bz_div3by2(&q0, &r0, a_step2, b, k);

        if (q) {
            shift_left_limbs(*q, q1, k);
            if (q0.m_size > 0) {
                BigIntStorage tmp;
                add_unsigned(tmp, *q, q0);
                tmp.m_sign = 1;
                *q = std::move(tmp);
            }
        }
        if (r) {
            *r = std::move(r0);
        }
    }

    /// <summary>
    /// 核心長除法與取模派發入口。
    /// 支援單 limb 快速路徑、Knuth Algorithm D 與 Burnikel–Ziegler 分治除法。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_mod_core(
        BigIntStorage* q_out, BigIntStorage* r_out,
        const BigIntStorage& u, const BigIntStorage& v)
    {
        if (v.m_size == 0) {
            NUMERIC_THROW_OR_ABORT(std::invalid_argument("division by zero"));
        }
        if (u.m_size == 0) {
            if (q_out) { q_out->m_size = 0; q_out->m_sign = 0; }
            if (r_out) { r_out->m_size = 0; r_out->m_sign = 0; }
            return;
        }
        int cmp = compare_unsigned(u.data(), u.m_size, v.data(), v.m_size);
        if (cmp < 0) {
            if (q_out) { q_out->m_size = 0; q_out->m_sign = 0; }
            if (r_out) { *r_out = u; r_out->m_sign = 1; }
            return;
        }
        if (cmp == 0) {
            if (q_out) { q_out->set_uint64(1, 1); }
            if (r_out) { r_out->m_size = 0; r_out->m_sign = 0; }
            return;
        }

        // 單 limb 快速除法路徑
        if (v.m_size == 1) {
            uint64_t divisor = v.data()[0];
            if (q_out) q_out->resize(u.m_size, 0);
            uint64_t rem = 0;
            for (size_t i = u.m_size; i > 0; --i) {
                uint64_t next_rem = 0;
                uint64_t q_limb = div128_64(rem, u.data()[i - 1], divisor, next_rem);
                if (q_out) q_out->data()[i - 1] = q_limb;
                rem = next_rem;
            }
            if (q_out) {
                q_out->m_sign = 1;
                q_out->normalize();
            }
            if (r_out) {
                if (rem != 0) {
                    r_out->set_uint64(rem, 1);
                } else {
                    r_out->m_size = 0;
                    r_out->m_sign = 0;
                }
            }
            return;
        }

        // 小於 BZ_THRESHOLD (64 limbs, 4096-bit) 採 Knuth Algorithm D 長除法
        if (v.m_size < BZ_THRESHOLD) {
            div_mod_core_knuth(q_out, r_out, u, v);
            return;
        }

        // Burnikel–Ziegler 大數分治除法路徑
        size_t s = v.m_size;
        size_t m_pow2 = 2;
        while (m_pow2 * BZ_THRESHOLD <= s) {
            m_pow2 <<= 1;
        }
        size_t j = (s + m_pow2 - 1) / m_pow2;
        size_t n = j * m_pow2; // n is always even and >= s

        size_t n_bits = n * 64;
        size_t v_bits = (v.m_size - 1) * 64 + (64 - clz64(v.data()[v.m_size - 1]));
        size_t sigma = (n_bits >= v_bits) ? (n_bits - v_bits) : 0;

        BigIntStorage v_shifted, u_shifted;
        if (sigma > 0) {
            shift_left(v_shifted, v, sigma);
            shift_left(u_shifted, u, sigma);
        } else {
            v_shifted = v;
            u_shifted = u;
        }

        size_t u_bits = (u_shifted.m_size > 0) 
            ? ((u_shifted.m_size - 1) * 64 + (64 - clz64(u_shifted.data()[u_shifted.m_size - 1])))
            : 0;
        size_t t = (u_bits + n_bits) / n_bits;
        if (t < 2) t = 2;

        auto get_block = [&](BigIntStorage& out, size_t block_idx) {
            size_t start = block_idx * n;
            if (start >= u_shifted.m_size) {
                out.m_size = 0;
                out.m_sign = 0;
                return;
            }
            size_t len = std::min(n, u_shifted.m_size - start);
            out.resize(len, 0);
            BigIntStorage::copy_limbs(out.data(), u_shifted.data() + start, len);
            out.m_sign = 1;
            out.normalize();
        };

        BigIntStorage r_cur;
        get_block(r_cur, t - 1);

        BigIntStorage q_total;
        for (size_t i = t - 1; i > 0; --i) {
            size_t block_idx = i - 1;
            BigIntStorage a_block;
            get_block(a_block, block_idx);

            BigIntStorage z;
            shift_left_limbs(z, r_cur, n);
            if (a_block.m_size > 0) {
                BigIntStorage tmp;
                add_unsigned(tmp, z, a_block);
                tmp.m_sign = 1;
                z = std::move(tmp);
            }

            BigIntStorage q_step, r_step;
            bz_div2by1(&q_step, &r_step, z, v_shifted);
            r_cur = std::move(r_step);

            if (q_out) {
                shift_left_limbs(q_total, q_total, n);
                if (q_step.m_size > 0) {
                    BigIntStorage tmp;
                    add_unsigned(tmp, q_total, q_step);
                    tmp.m_sign = 1;
                    q_total = std::move(tmp);
                }
            }
        }

        if (q_out) {
            *q_out = std::move(q_total);
            q_out->m_sign = (q_out->m_size > 0) ? 1 : 0;
            q_out->normalize();
        }
        if (r_out) {
            if (sigma > 0) {
                shift_right(*r_out, r_cur, sigma);
            } else {
                *r_out = std::move(r_cur);
            }
            r_out->m_sign = (r_out->m_size > 0) ? 1 : 0;
            r_out->normalize();
        }
    }

    /// <summary>
    /// 商餘同求長除法介面。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_qr(BigIntStorage& q, BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) {
        div_mod_core(&q, &r, u, v);
    }

    /// <summary>
    /// 僅求商長除法介面（省略餘數處理）。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_q(BigIntStorage& q, const BigIntStorage& u, const BigIntStorage& v) {
        div_mod_core(&q, nullptr, u, v);
    }

    /// <summary>
    /// 僅求餘數取模介面（省略商輸出處理）。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_r(BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) {
        div_mod_core(nullptr, &r, u, v);
    }

    /// <summary>
    /// 帶符號商餘同求除法運算。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_qr_signed(BigIntStorage& q, BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) {
        div_mod_core(&q, &r, u, v);
        if (q.m_size > 0) {
            q.m_sign = static_cast<int8_t>(u.m_sign * v.m_sign);
        }
        if (r.m_size > 0) {
            r.m_sign = u.m_sign;
        }
    }

    /// <summary>
    /// 帶符號僅求商除法運算。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_q_signed(BigIntStorage& q, const BigIntStorage& u, const BigIntStorage& v) {
        div_mod_core(&q, nullptr, u, v);
        if (q.m_size > 0) {
            q.m_sign = static_cast<int8_t>(u.m_sign * v.m_sign);
        }
    }

    /// <summary>
    /// 帶符號僅求餘數取模運算。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_r_signed(BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) {
        div_mod_core(nullptr, &r, u, v);
        if (r.m_size > 0) {
            r.m_sign = u.m_sign;
        }
    }

    /// <summary>
    /// 傳統相容介面：帶符號除法與模運算。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_mod_signed(BigIntStorage& q, BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) {
        div_qr_signed(q, r, u, v);
    }

    /// <summary>
    /// 傳統相容介面：無符號除法與模運算。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void div_mod_core(BigIntStorage& q, BigIntStorage& r, const BigIntStorage& u, const BigIntStorage& v) {
        div_mod_core(&q, &r, u, v);
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
        if (&res == &a) {
            BigIntStorage tmp;
            shift_left_limbs(tmp, a, limbs);
            res = std::move(tmp);
            return;
        }
        res.resize(a.m_size + limbs, 0);
        BigIntStorage::copy_limbs(res.data() + limbs, a.data(), a.m_size);
        BigIntStorage::zero_limbs(res.data(), limbs);
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
        if (&res == &a) {
            BigIntStorage tmp;
            shift_left(tmp, a, shift);
            res = std::move(tmp);
            return;
        }
        size_t limb_shift = shift / 64;
        size_t bit_shift = shift % 64;
        size_t new_size = a.m_size + limb_shift + 1;
        res.resize(new_size, 0);

        if (bit_shift == 0) {
            BigIntStorage::copy_limbs(res.data() + limb_shift, a.data(), a.m_size);
            if (limb_shift > 0) {
                BigIntStorage::zero_limbs(res.data(), limb_shift);
            }
        } else {
            if (limb_shift > 0) {
                BigIntStorage::zero_limbs(res.data(), limb_shift);
            }
            uint64_t carry = 0;
            for (size_t i = 0; i < a.m_size; ++i) {
                uint64_t cur = a.data()[i];
                res.data()[i + limb_shift] = (cur << bit_shift) | carry;
                carry = cur >> (64 - bit_shift);
            }
            res.data()[a.m_size + limb_shift] = carry;
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
        if (&res == &a) {
            BigIntStorage tmp;
            shift_right_positive(tmp, a, shift);
            res = std::move(tmp);
            return;
        }
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
            BigIntStorage::copy_limbs(res.data(), a.data() + limb_shift, new_size);
        } else {
            for (size_t i = 0; i < new_size; ++i) {
                uint64_t cur = a.data()[i + limb_shift];
                uint64_t next = (i + limb_shift + 1 < a.m_size) ? a.data()[i + limb_shift + 1] : 0;
                res.data()[i] = (cur >> bit_shift) | (next << (64 - bit_shift));
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
                res.data()[i] = a.data()[i] & b.data()[i];
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
                uint64_t nu = (i < not_b.m_size) ? not_b.data()[i] : 0;
                res.data()[i] = a.data()[i] & (~nu);
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
                uint64_t av = (i < a.m_size) ? a.data()[i] : 0;
                uint64_t bv = (i < b.m_size) ? b.data()[i] : 0;
                res.data()[i] = av | bv;
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
                uint64_t av = (i < a.m_size) ? a.data()[i] : 0;
                uint64_t bv = (i < b.m_size) ? b.data()[i] : 0;
                res.data()[i] = av ^ bv;
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
    /// 線程安全且帶 [[unlikely]] 冷路徑優化之 10^(19 * 2^k) 冪次快取表。
    /// 預先計算前 7 階 (k = 0..6，涵蓋至 10^1216)，冷啟動小於 1 微秒。
    /// </summary>
    class Pow10Cache {
    public:
        static constexpr size_t PRECOMPUTED_LEVELS = 7; // k = 0..6 (涵蓋至 10^1216)
        static constexpr size_t MAX_LEVELS = 13;        // 支援至 77,824 位
        BigIntStorage table[MAX_LEVELS];
        std::atomic<size_t> computed_levels{PRECOMPUTED_LEVELS};
        std::mutex mtx;

        static Pow10Cache& instance() {
            static Pow10Cache cache;
            return cache;
        }

        const BigIntStorage& get_pow(size_t k) {
            assert(k < MAX_LEVELS);
            if (k >= computed_levels.load(std::memory_order_acquire))
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
                [[unlikely]]
#endif
            {
                std::lock_guard<std::mutex> lock(mtx);
                size_t cur = computed_levels.load(std::memory_order_relaxed);
                for (size_t i = cur; i <= k && i < MAX_LEVELS; ++i) {
                    BigIntCore::mul_core(table[i], table[i - 1], table[i - 1]);
                    computed_levels.store(i + 1, std::memory_order_release);
                }
            }
            return table[k];
        }

    private:
        Pow10Cache() {
            table[0].set_uint64(10000000000000000000ULL, 1);
            for (size_t k = 1; k < PRECOMPUTED_LEVELS; ++k) {
                BigIntCore::mul_core(table[k], table[k - 1], table[k - 1]);
            }
        }
    };

    /// <summary>
    /// 當 k >= Pow10Cache::MAX_LEVELS 時的局部動態冪次求值。
    /// </summary>
    static BigIntStorage compute_pow10_dynamic(size_t k) {
        BigIntStorage cur = Pow10Cache::instance().get_pow(Pow10Cache::MAX_LEVELS - 1);
        for (size_t i = Pow10Cache::MAX_LEVELS - 1; i < k; ++i) {
            BigIntStorage nxt;
            mul_core(nxt, cur, cur);
            cur = std::move(nxt);
        }
        return cur;
    }

    /// <summary>
    /// 線性累積解析 19-digit chunks 陣列，採用原地單 limb 乘加運算，零臨時配置。
    /// </summary>
    static BigIntStorage parse_chunks_linear(const uint64_t* chunks, size_t count) {
        BigIntStorage cur;
        cur.set_uint64(chunks[count - 1], 1);
        constexpr uint64_t RADIX10_19 = 10000000000000000000ULL;
        for (size_t i = count - 1; i > 0; --i) {
            uint64_t chunk_val = chunks[i - 1];
            if (cur.m_capacity < cur.m_size + 1) {
                cur.reserve(cur.m_size + 2);
            }
            uint64_t carry = chunk_val;
            for (size_t j = 0; j < cur.m_size; ++j) {
                uint64_t hi = 0;
                uint64_t lo = mul64_wide(cur.data()[j], RADIX10_19, hi);
                lo += carry;
                if (lo < carry) ++hi;
                cur.data()[j] = lo;
                carry = hi;
            }
            if (carry > 0) {
                cur.data()[cur.m_size] = carry;
                cur.m_size += 1;
            }
        }
        cur.m_sign = 1;
        cur.normalize();
        return cur;
    }

    /// <summary>
    /// 分治解析 19-digit chunks 陣列 [start, end)。
    /// </summary>
    static BigIntStorage parse_chunks_dc(const uint64_t* chunks, size_t start, size_t end) {
        size_t count = end - start;
        if (count <= FROM_STRING_DC_THRESHOLD) {
            return parse_chunks_linear(chunks + start, count);
        }

        // 尋找切分點：最大的 2^k 使得 2^k < count
        size_t k = 0;
        while ((static_cast<size_t>(1) << (k + 1)) < count) {
            ++k;
        }
        size_t split = static_cast<size_t>(1) << k;

        BigIntStorage low = parse_chunks_dc(chunks, start, start + split);
        BigIntStorage high = parse_chunks_dc(chunks, start + split, end);

        BigIntStorage dynamic_pow;
        const BigIntStorage* pow_ptr = nullptr;
        if (k < Pow10Cache::MAX_LEVELS) {
            pow_ptr = &Pow10Cache::instance().get_pow(k);
        } else {
            dynamic_pow = compute_pow10_dynamic(k);
            pow_ptr = &dynamic_pow;
        }

        BigIntStorage high_scaled;
        mul_core(high_scaled, high, *pow_ptr);

        BigIntStorage res;
        add_signed(res, high_scaled, low);
        return res;
    }

    /// <summary>
    /// C++20 constexpr 編譯期常數求值專用字串解析函式。
    /// 採用無靜態局部變數之 Horner 演算法，保證常數求值完全相容。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void from_string_constexpr(BigIntStorage& res, numeric::string_view sv) {
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
        for (size_t i = idx; i < sv.size(); ++i) {
            if (sv[i] < '0' || sv[i] > '9') {
                NUMERIC_THROW_OR_ABORT(std::invalid_argument("invalid character in bigint string"));
            }
        }
        while (idx < sv.size() && sv[idx] == '0') {
            ++idx;
        }
        if (idx == sv.size()) {
            res.reset_heap();
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }

        size_t total_digits = sv.size() - idx;
        if (total_digits <= 19) {
            uint64_t val = 0;
            for (size_t i = idx; i < sv.size(); ++i) {
                val = val * 10 + static_cast<uint64_t>(sv[i] - '0');
            }
            res.set_uint64(val, sign);
            return;
        }

        BigIntStorage cur;
        cur.reset_heap();
        cur.m_size = 0;
        cur.m_sign = 0;

        while (idx < sv.size()) {
            size_t take = (sv.size() - idx >= 9) ? 9 : (sv.size() - idx);
            uint64_t chunk_val = 0;
            uint64_t multiplier = 1;
            for (size_t i = 0; i < take; ++i) {
                chunk_val = chunk_val * 10 + static_cast<uint64_t>(sv[idx + i] - '0');
                multiplier *= 10;
            }
            idx += take;

            if (cur.m_size > 0) {
                BigIntStorage next_cur;
                mul_single_limb(next_cur, cur, multiplier, 1);
                BigIntStorage chunk_st;
                chunk_st.set_uint64(chunk_val, 1);
                add_signed(cur, next_cur, chunk_st);
            } else {
                cur.set_uint64(chunk_val, 1);
            }
        }
        cur.m_sign = sign;
        cur.normalize();
        res = std::move(cur);
    }

    /// <summary>
    /// 執行期高效十進位字串解析（分治法 + Pow10Cache 冪次表快取）。
    /// </summary>
    static void from_string_runtime(BigIntStorage& res, numeric::string_view sv) {
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

        for (size_t i = idx; i < sv.size(); ++i) {
            if (sv[i] < '0' || sv[i] > '9') {
                NUMERIC_THROW_OR_ABORT(std::invalid_argument("invalid character in bigint string"));
            }
        }

        while (idx < sv.size() && sv[idx] == '0') {
            ++idx;
        }
        if (idx == sv.size()) {
            res.reset_heap();
            res.m_size = 0;
            res.m_sign = 0;
            return;
        }

        size_t total_digits = sv.size() - idx;
        if (total_digits <= 19) {
            uint64_t val = 0;
            for (size_t i = idx; i < sv.size(); ++i) {
                val = val * 10 + static_cast<uint64_t>(sv[i] - '0');
            }
            res.set_uint64(val, sign);
            return;
        }

        std::vector<uint64_t> chunks;
        chunks.reserve((total_digits + 18) / 19);

        size_t curr_end = sv.size();
        while (curr_end > idx) {
            size_t chunk_start = (curr_end - idx >= 19) ? (curr_end - 19) : idx;
            uint64_t val = 0;
            for (size_t i = chunk_start; i < curr_end; ++i) {
                val = val * 10 + static_cast<uint64_t>(sv[i] - '0');
            }
            chunks.push_back(val);
            curr_end = chunk_start;
        }

        if (chunks.size() <= FROM_STRING_DC_THRESHOLD) {
            res = parse_chunks_linear(chunks.data(), chunks.size());
            res.m_sign = sign;
            res.normalize();
            return;
        }

        res = parse_chunks_dc(chunks.data(), 0, chunks.size());
        res.m_sign = sign;
        res.normalize();
    }

    /// <summary>
    /// 字串解析統一入口：編譯期常數求值時派發至 from_string_constexpr，執行期派發至 from_string_runtime。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 void from_string(BigIntStorage& res, numeric::string_view sv) {
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
        if (std::is_constant_evaluated()) {
            from_string_constexpr(res, sv);
            return;
        }
#endif
        from_string_runtime(res, sv);
    }

    /// <summary>
    /// 將 BigIntStorage 轉換為 Radix-10 十進位字串。
    /// 8192-bit 內全棧上工作緩衝區（stack_limbs[128], stack_chunks[136]），達成真 0-Heap 分配。
    /// 搭配 100% 精準 digits10_u64 預留與 2-Digit LUT 倒序無分支格式化。
    /// </summary>
    /// <summary>
    /// 基底情況：將任意小於 10^(19 * count) 之大數透過 10^19 乘法求逆除法提取為 count 個 chunks。
    /// 不足 count 個 chunk 者高位自動補 0。
    /// </summary>
    static void extract_chunks_basecase(const BigIntStorage& val, uint64_t* out_chunks, size_t count) {
        for (size_t i = 0; i < count; ++i) {
            out_chunks[i] = 0;
        }
        if (val.m_size == 0 || val.m_sign == 0) {
            return;
        }

        uint64_t stack_limbs[128];
        std::vector<uint64_t> heap_limbs;
        uint64_t* work_limbs = stack_limbs;
        if (val.m_size > 128) {
            heap_limbs.resize(val.m_size);
            work_limbs = heap_limbs.data();
        }
        std::copy_n(val.data(), val.m_size, work_limbs);
        size_t work_size = val.m_size;
        size_t idx = 0;

        while (work_size > 0 && idx < count) {
            uint64_t rem = 0;
            for (size_t i = work_size; i > 0; --i) {
                uint64_t next_rem = 0;
                work_limbs[i - 1] = div_recip_radix10_19(rem, work_limbs[i - 1], next_rem);
                rem = next_rem;
            }
            while (work_size > 0 && work_limbs[work_size - 1] == 0) {
                --work_size;
            }
            out_chunks[idx++] = rem;
        }
    }

    /// <summary>
    /// 分治固定位元格式化：保證剛好寫入 num_chunks * 19 個字元（高位以 '0' 補足）。
    /// </summary>
    static void format_padded_dc(char* dst, const BigIntStorage& val, size_t num_chunks) {
        if (num_chunks <= 16) {
            uint64_t chunks[16];
            extract_chunks_basecase(val, chunks, num_chunks);
            for (size_t i = num_chunks; i > 0; --i) {
                format_chunk_19_digits(dst, chunks[i - 1]);
                dst += 19;
            }
            return;
        }

        size_t k = 0;
        while ((static_cast<size_t>(1) << (k + 1)) < num_chunks) {
            ++k;
        }
        size_t split = static_cast<size_t>(1) << k;

        BigIntStorage dynamic_pow;
        const BigIntStorage* pow_ptr = nullptr;
        if (k < Pow10Cache::MAX_LEVELS) {
            pow_ptr = &Pow10Cache::instance().get_pow(k);
        } else {
            dynamic_pow = compute_pow10_dynamic(k);
            pow_ptr = &dynamic_pow;
        }

        BigIntStorage q, r;
        div_qr(q, r, val, *pow_ptr);

        size_t high_chunks = num_chunks - split;
        format_padded_dc(dst, q, high_chunks);
        format_padded_dc(dst + high_chunks * 19, r, split);
    }

    /// <summary>
    /// 分治非固定位元格式化：最頂部數值無前導 0 寫入，回傳實際寫入字元總長。
    /// </summary>
    static size_t format_unpadded_dc(char* dst, const BigIntStorage& val) {
        if (val.m_size == 0 || val.m_sign == 0) {
            dst[0] = '0';
            return 1;
        }

        // 當數值小於等於 16 chunks (~304 十進位位元，約 1010 bits) 時採用 basecase 展開
        if (val.m_size <= 16) {
            uint64_t stack_limbs[32];
            std::copy_n(val.data(), val.m_size, stack_limbs);
            size_t work_size = val.m_size;

            uint64_t chunks[32];
            size_t chunk_count = 0;

            while (work_size > 0) {
                uint64_t rem = 0;
                for (size_t i = work_size; i > 0; --i) {
                    uint64_t next_rem = 0;
                    stack_limbs[i - 1] = div_recip_radix10_19(rem, stack_limbs[i - 1], next_rem);
                    rem = next_rem;
                }
                while (work_size > 0 && stack_limbs[work_size - 1] == 0) {
                    --work_size;
                }
                chunks[chunk_count++] = rem;
            }

            if (chunk_count == 0) {
                dst[0] = '0';
                return 1;
            }

            size_t top_digits = digits10_u64(chunks[chunk_count - 1]);
            format_highest_chunk(dst, chunks[chunk_count - 1], top_digits);
            char* out_ptr = dst + top_digits;

            for (size_t i = chunk_count - 1; i > 0; --i) {
                format_chunk_19_digits(out_ptr, chunks[i - 1]);
                out_ptr += 19;
            }
            return (chunk_count - 1) * 19 + top_digits;
        }

        // 大數分治：依據 val 之 bit 長度估計所需總 chunks，尋找最佳 split = 2^k
        // 1 chunk = 10^19 ≈ 2^63.11 bits => chunks ≈ val.m_size * 64 / 63
        size_t est_chunks = (val.m_size * 64 + 62) / 63;
        size_t k = 0;
        while ((static_cast<size_t>(1) << (k + 1)) < est_chunks) {
            ++k;
        }

        BigIntStorage dynamic_pow;
        const BigIntStorage* pow_ptr = nullptr;
        if (k < Pow10Cache::MAX_LEVELS) {
            pow_ptr = &Pow10Cache::instance().get_pow(k);
        } else {
            dynamic_pow = compute_pow10_dynamic(k);
            pow_ptr = &dynamic_pow;
        }

        // 若估計的 2^k 超過或等於 val，則降一級
        while (k > 0 && compare_unsigned(val.data(), val.m_size, pow_ptr->data(), pow_ptr->m_size) < 0) {
            --k;
            if (k < Pow10Cache::MAX_LEVELS) {
                pow_ptr = &Pow10Cache::instance().get_pow(k);
            } else {
                dynamic_pow = compute_pow10_dynamic(k);
                pow_ptr = &dynamic_pow;
            }
        }

        size_t split = static_cast<size_t>(1) << k;
        BigIntStorage q, r;
        div_qr(q, r, val, *pow_ptr);

        if (q.m_size == 0 || q.m_sign == 0) {
            return format_unpadded_dc(dst, r);
        }

        size_t q_len = format_unpadded_dc(dst, q);
        format_padded_dc(dst + q_len, r, split);
        return q_len + split * 19;
    }

    /// <summary>
    /// 將 BigIntStorage 轉換為 Radix-10 十進位字串。
    /// 小於 1000 位元採高效 10^19 Reciprocal Division；大數自動切換至 Divide-and-Conquer Radix Conversion。
    /// </summary>
    static std::string to_string(const BigIntStorage& a) {
        if (a.m_size == 0 || a.m_sign == 0) {
            return "0";
        }

        // 單 limb 快速超短格式化路徑
        if (a.m_size == 1) {
            uint64_t val = a.data()[0];
            if (val == 0) return "0";
            size_t digits = digits10_u64(val);
            char stack_buf[32];
            char* ptr = stack_buf;
            if (a.m_sign < 0) *ptr++ = '-';
            format_highest_chunk(ptr, val, digits);
            size_t total_len = (a.m_sign < 0 ? 1 : 0) + digits;
            return std::string(stack_buf, total_len);
        }

        // 小中型大數 (<= 16 limbs, 約 1024 bits)：棧上緩衝區零初步堆積配置
        if (a.m_size <= 16) {
            char stack_buf[512];
            char* out_ptr = stack_buf;
            if (a.m_sign < 0) {
                *out_ptr++ = '-';
            }
            size_t digits_written = format_unpadded_dc(out_ptr, a);
            size_t total_len = (a.m_sign < 0 ? 1 : 0) + digits_written;
            return std::string(stack_buf, total_len);
        }

        // 大型大數 (> 16 limbs)：預估容量並以分治法格式化
        size_t max_digits = a.m_size * 20 + 24;
        std::string result;
        result.resize(max_digits);

        char* out_ptr = &result[0];
        if (a.m_sign < 0) {
            *out_ptr++ = '-';
        }

        size_t digits_written = format_unpadded_dc(out_ptr, a);
        size_t total_len = (a.m_sign < 0 ? 1 : 0) + digits_written;
        result.resize(total_len);
        return result;
    }
};

} // namespace detail
} // namespace numeric
