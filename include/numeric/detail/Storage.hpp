#pragma once

// Storage.hpp
// Decoupled storage and memory management base class (BigIntBase) for CPP-BigInt.
// Employs LLVM SmallVector two-layer architecture with compile-time SBO parameterization.
// Zero external dependencies, downward compatible from C++23 to C++11.

#include "../Config.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <utility>

namespace numeric {

class BigIntBase;

namespace detail {
    using BigIntBase = numeric::BigIntBase;
    using BigIntStorage = numeric::BigIntBase;
} // namespace detail

/// <summary>
/// BigInt 基礎儲存與演算法基底類別，採用 LLVM SmallVector 雙層解耦架構。
/// 支援動態指定 SBO 內聯容量，在 64 位元架構下基底大小嚴格控制為 32 位元組。
/// </summary>
class BigIntBase {
public:
    uint64_t* m_data;
    uint64_t* m_inline_data;
    uint32_t  m_size;
    uint32_t  m_capacity;
    uint32_t  m_sbo_capacity;
    int8_t    m_sign;
    bool      m_is_sbo;
    uint8_t   m_pad[2];

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
        return m_is_sbo;
    }

    /// <summary>
    /// 檢查是否為小整數（SBO 模式之別名）。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 bool is_small() const noexcept {
        return is_sbo();
    }

    /// <summary>
    /// 檢查當前資料指標是否指向內聯 SBO 陣列。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 bool is_inline() const noexcept {
        return m_is_sbo;
    }

    /// <summary>
    /// 取得 limbs 資料指標（可修改）。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 uint64_t* data() noexcept {
        return m_data;
    }

    /// <summary>
    /// 取得 limbs 資料常數指標。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 const uint64_t* data() const noexcept {
        return m_data;
    }

    /// <summary>
    /// 取得 limbs 資料常數指標。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 const uint64_t* limbs() const noexcept {
        return m_data;
    }

    /// <summary>
    /// 取得有效 limbs 數量。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 size_t size() const noexcept {
        return m_size;
    }

    /// <summary>
    /// 取得有效 limbs 數量（size 之別名）。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 size_t limb_count() const noexcept {
        return m_size;
    }

    /// <summary>
    /// 取得目前配置之總容量。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 size_t capacity() const noexcept {
        return m_capacity;
    }

    /// <summary>
    /// 取得物件之 SBO 靜態內聯容量。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 size_t sbo_capacity() const noexcept {
        return m_sbo_capacity;
    }

    /// <summary>
    /// 取得整數符號：負數為 -1，零為 0，正數為 1。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 int8_t sign() const noexcept {
        return m_sign;
    }

    /// <summary>
    /// 判斷是否為負數。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 bool is_negative() const noexcept {
        return m_sign < 0;
    }

    /// <summary>
    /// 判斷數值是否為 0。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 bool is_zero() const noexcept {
        return m_sign == 0 || m_size == 0;
    }

    /// <summary>
    /// 下標運算子，直接存取指定索引之 limb。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 uint64_t& operator[](size_t idx) noexcept {
        return m_data[idx];
    }

    /// <summary>
    /// 下標常數運算子，直接唯讀存取指定索引之 limb。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 const uint64_t& operator[](size_t idx) const noexcept {
        return m_data[idx];
    }

    /// <summary>
    /// 釋放堆積緩衝區並將內部指標重置回 SBO。
    /// </summary>
    NUMERIC_CONSTEXPR_20 void reset_heap() noexcept {
        if (!m_is_sbo && m_data != nullptr && m_data != m_inline_data) {
            delete[] m_data;
        }
        m_data = m_inline_data;
        m_capacity = m_sbo_capacity;
        m_is_sbo = (m_sbo_capacity > 0);
    }

    /// <summary>
    /// 預設建構子：未配置 SBO 緩衝區（一般由衍生類別提供，或作為演算法內部暫存）。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntBase() noexcept
        : m_data(nullptr),
          m_inline_data(nullptr),
          m_size(0),
          m_capacity(0),
          m_sbo_capacity(0),
          m_sign(0),
          m_is_sbo(false),
          m_pad{0, 0} {}

    /// <summary>
    /// 基底建構子：由衍生類別傳入 inline buffer 的位置與容量。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntBase(uint64_t* inline_ptr, uint32_t inline_cap) noexcept
        : m_data(inline_cap > 0 ? inline_ptr : nullptr),
          m_inline_data(inline_cap > 0 ? inline_ptr : nullptr),
          m_size(0),
          m_capacity(inline_cap),
          m_sbo_capacity(inline_cap),
          m_sign(0),
          m_is_sbo(inline_cap > 0),
          m_pad{0, 0} {}

    /// <summary>
    /// 複製建構子：深拷貝另一物件之 limbs。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntBase(const BigIntBase& other)
        : m_data(nullptr),
          m_inline_data(nullptr),
          m_size(0),
          m_capacity(0),
          m_sbo_capacity(0),
          m_sign(0),
          m_is_sbo(false),
          m_pad{0, 0} {
        assign_from(other);
    }

    /// <summary>
    /// 移動建構子。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntBase(BigIntBase&& other) noexcept
        : m_data(nullptr),
          m_inline_data(nullptr),
          m_size(0),
          m_capacity(0),
          m_sbo_capacity(0),
          m_sign(0),
          m_is_sbo(false),
          m_pad{0, 0} {
        move_from(std::move(other));
    }

    /// <summary>
    /// 解構子：若已配置堆積記憶體則進行釋放。
    /// </summary>
    NUMERIC_CONSTEXPR_20 ~BigIntBase() noexcept {
        if (!m_is_sbo && m_data != nullptr && m_data != m_inline_data) {
            delete[] m_data;
            m_data = nullptr;
        }
    }

    /// <summary>
    /// 複製賦值運算子。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntBase& operator=(const BigIntBase& other) {
        assign_from(other);
        return *this;
    }

    /// <summary>
    /// 移動賦值運算子。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntBase& operator=(BigIntBase&& other) noexcept {
        move_from(std::move(other));
        return *this;
    }

    /// <summary>
    /// 複製賦值輔助函式：深拷貝另一物件之 limbs，支援跨 SBO 容量。
    /// </summary>
    NUMERIC_CONSTEXPR_20 void assign_from(const BigIntBase& other) {
        if (this != &other) {
            if (m_sbo_capacity > 0 && other.m_size <= m_sbo_capacity) {
                reset_heap();
                if (other.m_size > 0) {
                    copy_limbs(m_inline_data, other.m_data, other.m_size);
                }
                if (m_sbo_capacity > other.m_size) {
                    zero_limbs(m_inline_data + other.m_size, m_sbo_capacity - other.m_size);
                }
                m_data = m_inline_data;
                m_capacity = m_sbo_capacity;
                m_is_sbo = true;
            } else {
                size_t needed = (other.m_capacity > other.m_size) ? other.m_capacity : other.m_size;
                if (needed == 0) needed = 1;
                if (other.m_size > 0) {
                    if (m_capacity < other.m_size || m_is_sbo || m_data == nullptr) {
                        uint64_t* new_heap = new uint64_t[needed];
                        reset_heap();
                        m_data = new_heap;
                        m_capacity = static_cast<uint32_t>(needed);
                        m_is_sbo = false;
                    }
                    copy_limbs(m_data, other.m_data, other.m_size);
                } else {
                    reset_heap();
                }
            }
            m_size = other.m_size;
            m_sign = other.m_sign;
        }
    }

    /// <summary>
    /// 移動賦值輔助函式：若來源在 SBO 內則 memcpy，超出 SBO 則直接竊取 heap 指標。
    /// </summary>
    NUMERIC_CONSTEXPR_20 void move_from(BigIntBase&& other) noexcept {
        if (this != &other) {
            reset_heap();
            if (m_sbo_capacity > 0 && other.m_size <= m_sbo_capacity) {
                if (other.m_size > 0) {
                    copy_limbs(m_inline_data, other.m_data, other.m_size);
                }
                if (m_sbo_capacity > other.m_size) {
                    zero_limbs(m_inline_data + other.m_size, m_sbo_capacity - other.m_size);
                }
                m_data = m_inline_data;
                m_capacity = m_sbo_capacity;
                m_is_sbo = true;
                if (!other.m_is_sbo && other.m_data != nullptr && other.m_data != other.m_inline_data) {
                    delete[] other.m_data;
                }
                other.m_data = other.m_inline_data;
                other.m_capacity = other.m_sbo_capacity;
                other.m_is_sbo = (other.m_sbo_capacity > 0);
            } else if (other.m_is_sbo) {
                if (other.m_size > 0) {
                    m_data = new uint64_t[other.m_size];
                    m_capacity = static_cast<uint32_t>(other.m_size);
                    m_is_sbo = false;
                    copy_limbs(m_data, other.m_inline_data, other.m_size);
                } else {
                    m_data = m_inline_data;
                    m_capacity = m_sbo_capacity;
                    m_is_sbo = (m_sbo_capacity > 0);
                }
            } else {
                m_data = other.m_data;
                m_capacity = other.m_capacity;
                m_is_sbo = false;
                other.m_data = other.m_inline_data;
                other.m_capacity = other.m_sbo_capacity;
                other.m_is_sbo = (other.m_sbo_capacity > 0);
            }
            m_size = other.m_size;
            m_sign = other.m_sign;
            other.m_size = 0;
            other.m_sign = 0;
            if (other.m_sbo_capacity > 0) {
                zero_limbs(other.m_inline_data, other.m_sbo_capacity);
            }
        }
    }

    /// <summary>
    /// 原地單元取負操作。
    /// </summary>
    NUMERIC_CONSTEXPR_20 void negate() noexcept {
        if (m_size > 0 && m_sign != 0) {
            m_sign = -m_sign;
        }
    }

    /// <summary>
    /// 預留緩衝區容量。
    /// </summary>
    NUMERIC_CONSTEXPR_20 void reserve(size_t new_cap) {
        if (new_cap <= m_capacity) return;
        uint64_t* new_heap = new uint64_t[new_cap];
        if (m_size > 0) {
            copy_limbs(new_heap, m_data, m_size);
        }
        if (!m_is_sbo && m_data != nullptr && m_data != m_inline_data) {
            delete[] m_data;
        }
        m_data = new_heap;
        m_capacity = static_cast<uint32_t>(new_cap);
        m_is_sbo = false;
    }

    /// <summary>
    /// 調整 limbs 數量大小並可選填預設值。
    /// </summary>
    NUMERIC_CONSTEXPR_20 void resize(size_t new_size, uint64_t init_val = 0) {
        if (new_size > m_capacity) {
            size_t next_cap = static_cast<size_t>(m_capacity) * 2;
            if (next_cap < new_size) next_cap = new_size;
            reserve(next_cap);
        }
        uint64_t* d = data();
        if (new_size > m_size) {
            for (size_t i = m_size; i < new_size; ++i) {
                d[i] = init_val;
            }
        }
        m_size = static_cast<uint32_t>(new_size);
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
        if (!m_is_sbo && m_sbo_capacity > 0 && m_size <= m_sbo_capacity) {
            uint64_t* old_heap = m_data;
            for (size_t i = 0; i < m_sbo_capacity; ++i) {
                m_inline_data[i] = (i < m_size && old_heap != nullptr) ? old_heap[i] : 0;
            }
            m_data = m_inline_data;
            m_capacity = m_sbo_capacity;
            m_is_sbo = true;
            if (old_heap != nullptr && old_heap != m_inline_data) {
                delete[] old_heap;
            }
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
    NUMERIC_CONSTEXPR_20 void set_uint64(uint64_t val, int8_t sign) {
        if (val == 0) {
            m_size = 0;
            m_sign = 0;
            return;
        }
        if (m_sbo_capacity > 0) {
            reset_heap();
            for (size_t i = 0; i < m_sbo_capacity; ++i) {
                m_inline_data[i] = 0;
            }
            m_size = 1;
            m_sign = sign;
            m_inline_data[0] = val;
            m_is_sbo = true;
        } else {
            if (m_capacity < 1 || m_data == nullptr) {
                reserve(1);
            }
            m_size = 1;
            m_sign = sign;
            m_data[0] = val;
            m_is_sbo = false;
        }
    }
};

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
static_assert(sizeof(BigIntBase) == 32, "BigIntBase must be exactly 32 bytes on 64-bit platforms!");
#endif

} // namespace numeric
