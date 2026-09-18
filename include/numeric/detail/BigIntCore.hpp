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

    /// <summary>
    /// 64-bit ADC 原語：out = a + b + carry_in，回傳 carry_out (0 或 1)。
    /// 消除 C++ 純量條件判斷分支，並支援 C++20 constexpr 常數求值。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 NUMERIC_ALWAYS_INLINE uint8_t adc64(
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
    static NUMERIC_CONSTEXPR_20 NUMERIC_ALWAYS_INLINE uint8_t sbb64(
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
    static constexpr char DIGIT_PAIRS[201] =
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

    /// <summary>
    /// 支援完整 64 位元無符號整數（最大 18446744073709551615，共 20 位）之精準十進位位數判定。
    /// 補齊 10^16 二分階梯，杜絕任何緩衝區溢位。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 NUMERIC_ALWAYS_INLINE size_t digits10_u64(uint64_t v) noexcept {
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
    static NUMERIC_CONSTEXPR_20 NUMERIC_ALWAYS_INLINE void format_highest_chunk(
        char* dst, uint64_t val, size_t digits) noexcept
    {
        int pos = static_cast<int>(digits);
        while (val >= 100) {
            uint32_t rem = static_cast<uint32_t>(val % 100);
            val /= 100;
            pos -= 2;
            dst[pos]     = DIGIT_PAIRS[rem * 2];
            dst[pos + 1] = DIGIT_PAIRS[rem * 2 + 1];
        }
        if (val < 10) {
            dst[--pos] = static_cast<char>('0' + val);
        } else {
            pos -= 2;
            dst[pos]     = DIGIT_PAIRS[val * 2];
            dst[pos + 1] = DIGIT_PAIRS[val * 2 + 1];
        }
        assert(pos == 0 && "format_highest_chunk failed to match exact digit count");
    }

    /// <summary>
    /// 格式化中間 19 位 fixed-width chunk：倒序逆向填充 9 組 LUT 雙字元加上 1 個最高位單字元。
    /// </summary>
    static NUMERIC_CONSTEXPR_20 NUMERIC_ALWAYS_INLINE void format_chunk_19_digits(
        char* dst, uint64_t val) noexcept
    {
        for (int p = 8; p >= 0; --p) {
            uint32_t rem = static_cast<uint32_t>(val % 100);
            val /= 100;
            dst[1 + p * 2]     = DIGIT_PAIRS[rem * 2];
            dst[1 + p * 2 + 1] = DIGIT_PAIRS[rem * 2 + 1];
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
        if (scratch_size <= 576) {
            uint64_t stack_scratch[576];
            mul_karatsuba_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size, stack_scratch);
        } else {
            std::vector<uint64_t> heap_scratch(scratch_size);
            mul_karatsuba_raw(res.data(), a.data(), a.m_size, b.data(), b.m_size, heap_scratch.data());
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
        int cmp = compare_unsigned(u.data(), u.m_size, v.data(), v.m_size);
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
            uint64_t divisor = v.data()[0];
            q.resize(u.m_size, 0);
            uint64_t rem = 0;
            for (size_t i = u.m_size; i > 0; --i) {
                uint64_t next_rem = 0;
                q.data()[i - 1] = div128_64(rem, u.data()[i - 1], divisor, next_rem);
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
        int s = clz64(v.data()[n - 1]);
        BigIntStorage vn, un;
        shift_left(vn, v, static_cast<size_t>(s));
        shift_left(un, u, static_cast<size_t>(s));
        if (un.m_size < u.m_size + 1) {
            un.resize(u.m_size + 1, 0);
        }

        q.resize(m + 1, 0);

        uint64_t v_hi = vn.data()[n - 1];
        uint64_t v_lo = vn.data()[n - 2];

        // D2~D7: 主迴圈
        for (size_t k = m + 1; k > 0; --k) {
            size_t j = k - 1;
            uint64_t u_hi = un.data()[j + n];
            uint64_t u_mid = un.data()[j + n - 1];
            uint64_t u_lo = (j + n >= 2) ? un.data()[j + n - 2] : 0;

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
                uint64_t p_lo = mul64_wide(q_hat, vn.data()[i], p_hi);
                uint64_t p_full = p_lo + carry;
                uint64_t c1 = (p_full < p_lo) ? 1 : 0;
                carry = p_hi + c1;

                uint64_t cur = un.data()[j + i];
                uint64_t diff = cur - borrow;
                uint64_t b1 = (cur < borrow) ? 1 : 0;
                uint64_t diff2 = diff - p_full;
                uint64_t b2 = (diff < p_full) ? 1 : 0;
                borrow = b1 + b2;
                un.data()[j + i] = diff2;
            }

            uint64_t cur = un.data()[j + n];
            uint64_t diff = cur - borrow;
            uint64_t b1 = (cur < borrow) ? 1 : 0;
            uint64_t diff2 = diff - carry;
            uint64_t b2 = (diff < carry) ? 1 : 0;
            un.data()[j + n] = diff2;

            // D5: 判斷是否需要回加
            if (b1 + b2 > 0) {
                --q_hat;
                uint64_t add_carry = 0;
                for (size_t i = 0; i < n; ++i) {
                    uint64_t val = un.data()[j + i];
                    uint64_t sum = val + vn.data()[i] + add_carry;
                    add_carry = (sum < val || (add_carry && sum == val)) ? 1 : 0;
                    un.data()[j + i] = sum;
                }
                un.data()[j + n] += add_carry;
            }

            q.data()[j] = q_hat;
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
    /// 分治解析 19-digit chunks 陣列 [start, end)。
    /// </summary>
    static BigIntStorage parse_chunks_dc(const uint64_t* chunks, size_t start, size_t end) {
        size_t count = end - start;
        if (count == 1) {
            BigIntStorage s;
            s.set_uint64(chunks[start], 1);
            return s;
        }
        if (count == 2) {
            BigIntStorage s;
            BigIntStorage high;
            high.set_uint64(chunks[start + 1], 1);
            mul_single_limb(s, high, 10000000000000000000ULL, 1);
            BigIntStorage low;
            low.set_uint64(chunks[start], 1);
            BigIntStorage final_s;
            add_signed(final_s, s, low);
            return final_s;
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

        if (chunks.size() <= 4) {
            BigIntStorage cur;
            cur.set_uint64(chunks.back(), 1);
            constexpr uint64_t RADIX10_19 = 10000000000000000000ULL;
            for (size_t i = chunks.size() - 1; i > 0; --i) {
                BigIntStorage next_cur;
                mul_single_limb(next_cur, cur, RADIX10_19, 1);
                BigIntStorage limb_st;
                limb_st.set_uint64(chunks[i - 1], 1);
                add_signed(cur, next_cur, limb_st);
            }
            cur.m_sign = sign;
            cur.normalize();
            res = std::move(cur);
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
    static std::string to_string(const BigIntStorage& a) {
        if (a.m_size == 0 || a.m_sign == 0) {
            return "0";
        }

        uint64_t stack_limbs[128]; // 8192 bits
        std::vector<uint64_t> heap_limbs;
        uint64_t* work_limbs = stack_limbs;
        if (a.m_size > 128) {
            heap_limbs.resize(a.m_size);
            work_limbs = heap_limbs.data();
        }
        std::copy_n(a.data(), a.m_size, work_limbs);
        size_t work_size = a.m_size;

        // 8192 bits 滿載需 130 chunks，棧大小配置為 136 確保絕對不溢位
        uint64_t stack_chunks[136];
        std::vector<uint64_t> heap_chunks;
        uint64_t* chunks = stack_chunks;
        if (a.m_size > 128) {
            size_t max_chunks = (a.m_size * 64) / 63 + 8;
            heap_chunks.resize(max_chunks);
            chunks = heap_chunks.data();
        }

        size_t chunk_count = 0;
        constexpr uint64_t RADIX10_19 = 10000000000000000000ULL;

        while (work_size > 0) {
            uint64_t rem = 0;
            for (size_t i = work_size; i > 0; --i) {
                uint64_t next_rem = 0;
                work_limbs[i - 1] = div128_64(rem, work_limbs[i - 1], RADIX10_19, next_rem);
                rem = next_rem;
            }
            while (work_size > 0 && work_limbs[work_size - 1] == 0) {
                --work_size;
            }
            chunks[chunk_count++] = rem;
        }

        if (chunk_count == 0) {
            return "0";
        }

        // 100% 精確字串長度計算
        bool is_neg = (a.m_sign < 0);
        size_t top_digits = digits10_u64(chunks[chunk_count - 1]);
        size_t total_chars = (chunk_count - 1) * 19 + top_digits + (is_neg ? 1 : 0);

        std::string result;
        result.resize(total_chars);
        char* out_ptr = result.data();
        if (is_neg) {
            *out_ptr++ = '-';
        }

        // 格式化最高位 chunk（傳入 top_digits，狀態一致閉合）
        format_highest_chunk(out_ptr, chunks[chunk_count - 1], top_digits);
        out_ptr += top_digits;

        // 其餘 chunk 倒序 2-Digit LUT 填入
        for (size_t i = chunk_count - 1; i > 0; --i) {
            format_chunk_19_digits(out_ptr, chunks[i - 1]);
            out_ptr += 19;
        }

        return result;
    }
};

} // namespace detail
} // namespace numeric
