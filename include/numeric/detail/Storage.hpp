// filepath: include/numeric/detail/Storage.hpp
/**
 * @file Storage.hpp
 * @brief Decoupled storage and RAII memory management base class (BigIntBase) for CPP-BigInt.
 *        Employs LLVM SmallVector two-layer architecture with compile-time SBO parameterization,
 *        std::span limb manipulation, and C++23 constexpr RAII heap allocation guards.
 *        解耦之儲存與 RAII 記憶體管理基底類別 (BigIntBase)。
 *        採用 LLVM SmallVector 雙層架構、std::span 安全肢段存取與 C++23 constexpr RAII 堆積配置保護。
 */
#pragma once

#include "../Config.hpp"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ranges>
#include <span>
#include <utility>

namespace numeric {

class BigIntBase;

namespace detail {
    using BigIntBase = numeric::BigIntBase;
    using BigIntStorage = numeric::BigIntBase;
} // namespace detail

/// <summary>
/// Fundamental storage and buffer lifecycle base class using LLVM SmallVector two-layer decoupling.
/// Maintains exact 32-byte layout on 64-bit platforms for zero-overhead inline SBO buffers.
/// BigInt 基礎儲存與緩衝區生命週期基底類別，採用 LLVM SmallVector 雙層解耦架構，
/// 於 64 位元平台嚴格維持 32 位元組結構大小。
/// </summary>
class BigIntBase {
public:
    std::uint64_t* m_data{nullptr};
    std::uint64_t* m_inline_data{nullptr};
    std::uint32_t  m_size{0};
    std::uint32_t  m_capacity{0};
    std::uint32_t  m_sbo_capacity{0};
    std::int8_t    m_sign{0};
    bool           m_is_sbo{false};
    std::array<std::uint8_t, 2> m_pad{0, 0};

    /// <summary>
    /// Allocates an uninitialized limb array using C++23 RAII std::make_unique.
    /// 透過 C++23 RAII std::make_unique 配置 limbs 緩衝區。
    /// </summary>
    /// <param name="count">Number of 64-bit limbs to allocate. / 欲配置之 limb 數量。</param>
    /// <returns>Owning raw pointer transferred to BigIntBase. / 轉移擁有權至 BigIntBase 之指標。</returns>
    NUMERIC_NODISCARD static NUMERIC_CONSTEXPR_20 auto allocate_limbs(const std::size_t count) -> std::uint64_t* {
        auto heap{std::make_unique<std::uint64_t[]>(count)};
        return heap.release();
    }

    /// <summary>
    /// Releases an owned heap limb array safely through an RAII unique_ptr guard.
    /// 透過 RAII std::unique_ptr 守衛安全釋放堆積配置之 limbs 緩衝區。
    /// </summary>
    /// <param name="ptr">Heap pointer to release (nullptr is safe). / 欲釋放之堆積指標。</param>
    static NUMERIC_CONSTEXPR_20 auto deallocate_limbs(std::uint64_t* const ptr) noexcept -> void {
        const std::unique_ptr<std::uint64_t[]> guard{ptr};
    }

    /// <summary>
    /// Copies limbs from source span into destination span (supports constexpr evaluation).
    /// 拷貝指定來源 span 至目標 span，支援編譯期 constexpr 運算。
    /// </summary>
    /// <param name="dst">Destination span (must be >= src.size()). / 目標輸出 span。</param>
    /// <param name="src">Source span. / 來源輸入 span。</param>
    static NUMERIC_CONSTEXPR_20 auto copy_limbs(
        const std::span<std::uint64_t> dst,
        const std::span<const std::uint64_t> src) noexcept -> void
    {
        assert(dst.size() >= src.size());
        const std::size_t count{std::min(dst.size(), src.size())};
        std::ranges::copy(src.first(count), dst.begin());
    }

    /// <summary>
    /// Zeroes all elements within the target limb span (supports constexpr evaluation).
    /// 將目標 span 內所有 limbs 清零，支援編譯期 constexpr 運算。
    /// </summary>
    /// <param name="dst">Target span to clear. / 欲清零之目標 span。</param>
    static NUMERIC_CONSTEXPR_20 auto zero_limbs(const std::span<std::uint64_t> dst) noexcept -> void {
        std::ranges::fill(dst, std::uint64_t{0});
    }

    /// <summary>
    /// Checks whether storage currently resides in the inline SBO buffer.
    /// 檢查當前是否使用 SBO 內建緩衝區儲存。
    /// </summary>
    /// <returns>True if inline SBO is active. / 若處於 SBO 模式回傳 true。</returns>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto is_sbo() const noexcept -> bool {
        return m_is_sbo;
    }

    /// <summary>
    /// Alias for is_sbo(). / is_sbo() 之別名。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto is_small() const noexcept -> bool {
        return is_sbo();
    }

    /// <summary>
    /// Checks whether the data pointer currently points to the inline SBO array.
    /// 檢查當前資料指標是否指向內聯 SBO 陣列。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto is_inline() const noexcept -> bool {
        return m_is_sbo;
    }

    /// <summary>
    /// Returns mutable pointer to active limb buffer.
    /// 取得可修改之 limbs 資料指標。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto data() noexcept -> std::uint64_t* {
        return m_data;
    }

    /// <summary>
    /// Returns const pointer to active limb buffer.
    /// 取得唯讀之 limbs 資料常數指標。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto data() const noexcept -> const std::uint64_t* {
        return m_data;
    }

    /// <summary>
    /// Alias for const data(). / 唯讀 limbs 指標別名。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto limbs() const noexcept -> const std::uint64_t* {
        return m_data;
    }

    /// <summary>
    /// Returns a bounds-safe mutable std::span covering active limbs.
    /// 取得涵蓋有效 limbs 的可寫 std::span 安全視圖。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto as_span() noexcept -> std::span<std::uint64_t> {
        return std::span<std::uint64_t>{m_data, m_size};
    }

    /// <summary>
    /// Returns a bounds-safe read-only std::span covering active limbs.
    /// 取得涵蓋有效 limbs 的唯讀 std::span 安全視圖。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto as_span() const noexcept -> std::span<const std::uint64_t> {
        return std::span<const std::uint64_t>{m_data, m_size};
    }

    /// <summary>
    /// Returns a mutable std::span covering full allocated capacity.
    /// 取得涵蓋完整配置容量的可寫 std::span 安全視圖。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto capacity_span() noexcept -> std::span<std::uint64_t> {
        return std::span<std::uint64_t>{m_data, m_capacity};
    }

    /// <summary>
    /// Returns a mutable std::span covering the inline SBO buffer.
    /// 取得涵蓋內聯 SBO 緩衝區的 std::span 安全視圖。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto inline_span() noexcept -> std::span<std::uint64_t> {
        return std::span<std::uint64_t>{m_inline_data, m_sbo_capacity};
    }

    /// <summary>
    /// Returns number of active 64-bit limbs.
    /// 取得有效 limbs 數量。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto size() const noexcept -> std::size_t {
        return m_size;
    }

    /// <summary>
    /// Alias for size(). / size() 之別名。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto limb_count() const noexcept -> std::size_t {
        return m_size;
    }

    /// <summary>
    /// Returns total allocated limb capacity.
    /// 取得目前配置之總容量。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto capacity() const noexcept -> std::size_t {
        return m_capacity;
    }

    /// <summary>
    /// Returns static inline SBO limb capacity.
    /// 取得物件之 SBO 靜態內聯容量。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto sbo_capacity() const noexcept -> std::size_t {
        return m_sbo_capacity;
    }

    /// <summary>
    /// Returns integer sign (-1 for negative, 0 for zero, 1 for positive).
    /// 取得整數符號：負數為 -1，零為 0，正數為 1。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto sign() const noexcept -> std::int8_t {
        return m_sign;
    }

    /// <summary>
    /// Checks whether the integer is negative.
    /// 判斷是否為負數。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto is_negative() const noexcept -> bool {
        return m_sign < 0;
    }

    /// <summary>
    /// Checks whether the integer equals zero.
    /// 判斷數值是否為 0。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto is_zero() const noexcept -> bool {
        return m_sign == 0 || m_size == 0;
    }

    /// <summary>
    /// Mutable element subscript access with bounds assertion in debug builds.
    /// 可寫下標運算子（除錯模式下執行邊界斷言）。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto operator[](const std::size_t idx) noexcept -> std::uint64_t& {
        assert(idx < m_capacity);
        return m_data[idx];
    }

    /// <summary>
    /// Read-only element subscript access with bounds assertion in debug builds.
    /// 唯讀下標運算子（除錯模式下執行邊界斷言）。
    /// </summary>
    NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto operator[](const std::size_t idx) const noexcept -> const std::uint64_t& {
        assert(idx < m_capacity);
        return m_data[idx];
    }

    /// <summary>
    /// Releases any heap buffer and resets internal pointer back to inline SBO.
    /// 釋放堆積緩衝區並將內部指標重置回 SBO。
    /// </summary>
    NUMERIC_CONSTEXPR_20 auto reset_heap() noexcept -> void {
        if (!m_is_sbo && m_data != nullptr && m_data != m_inline_data) {
            deallocate_limbs(m_data);
        }
        m_data = m_inline_data;
        m_capacity = m_sbo_capacity;
        m_is_sbo = (m_sbo_capacity > 0U);
    }

    /// <summary>
    /// Default constructor: zero-initialized with no inline buffer.
    /// 預設建構子：無內聯 SBO 緩衝區，供演算法內部暫存物件使用。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntBase() noexcept = default;

    /// <summary>
    /// Binds a derived class's inline SBO storage span after member initialization completes.
    /// 於成員初始化完成後綁定衍生類別之內聯 SBO 緩衝區，確保嚴格符合 C++ constexpr 物件生命週期規範。
    /// </summary>
    /// <param name="inline_buf">Span covering derived class inline storage. / 衍生類別內聯緩衝區 span。</param>
    NUMERIC_CONSTEXPR_20 auto bind_inline(const std::span<std::uint64_t> inline_buf) noexcept -> void {
        m_inline_data = inline_buf.empty() ? nullptr : inline_buf.data();
        m_data = m_inline_data;
        m_capacity = static_cast<std::uint32_t>(inline_buf.size());
        m_sbo_capacity = static_cast<std::uint32_t>(inline_buf.size());
        m_is_sbo = !inline_buf.empty();
    }

    /// <summary>
    /// Base constructor binding a derived class's inline SBO storage span.
    /// 基底建構子：由衍生類別傳入 inline SBO 緩衝區 span。
    /// </summary>
    /// <param name="inline_buf">Span covering derived class inline storage. / 衍生類別內聯緩衝區 span。</param>
    explicit NUMERIC_CONSTEXPR_20 BigIntBase(const std::span<std::uint64_t> inline_buf) noexcept
        : m_data{inline_buf.empty() ? nullptr : inline_buf.data()},
          m_inline_data{inline_buf.empty() ? nullptr : inline_buf.data()},
          m_size{0},
          m_capacity{static_cast<std::uint32_t>(inline_buf.size())},
          m_sbo_capacity{static_cast<std::uint32_t>(inline_buf.size())},
          m_sign{0},
          m_is_sbo{!inline_buf.empty()},
          m_pad{0, 0} {}

    /// <summary>
    /// Copy constructor: deep-copies limbs from other.
    /// 複製建構子：深拷貝另一物件之 limbs。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntBase(const BigIntBase& other) {
        assign_from(other);
    }

    /// <summary>
    /// Move constructor: transfers ownership or copies inline limbs.
    /// 移動建構子：轉移堆積擁有權或複製內聯 limbs。
    /// </summary>
    NUMERIC_CONSTEXPR_20 BigIntBase(BigIntBase&& other) noexcept {
        move_from(std::move(other));
    }

    /// <summary>
    /// Destructor: releases heap buffer via RAII guard if heap-allocated.
    /// 解構子：若為堆積配置則透過 RAII 守衛釋放記憶體。
    /// </summary>
    NUMERIC_CONSTEXPR_20 ~BigIntBase() noexcept {
        if (!m_is_sbo && m_data != nullptr && m_data != m_inline_data) {
            deallocate_limbs(m_data);
            m_data = nullptr;
        }
    }

    /// <summary>
    /// Copy assignment operator. / 複製賦值運算子。
    /// </summary>
    NUMERIC_CONSTEXPR_20 auto operator=(const BigIntBase& other) -> BigIntBase& {
        assign_from(other);
        return *this;
    }

    /// <summary>
    /// Move assignment operator. / 移動賦值運算子。
    /// </summary>
    NUMERIC_CONSTEXPR_20 auto operator=(BigIntBase&& other) noexcept -> BigIntBase& {
        move_from(std::move(other));
        return *this;
    }

    /// <summary>
    /// Deep-copies limbs from another BigIntBase, reusing buffer capacity whenever possible.
    /// 複製賦值輔助函式：深拷貝另一物件之 limbs，盡可能重用既有緩衝區容量。
    /// </summary>
    /// <param name="other">Source BigIntBase object. / 來源 BigIntBase 物件。</param>
    NUMERIC_CONSTEXPR_20 auto assign_from(const BigIntBase& other) -> void {
        if (this == &other) {
            return;
        }
        const std::uint32_t src_size{
            other.m_is_sbo ? std::min(other.m_size, other.m_sbo_capacity) : other.m_size
        };
        const std::span<const std::uint64_t> src_span{other.m_data, src_size};
        if (m_capacity >= src_size && m_data != nullptr) {
            if (src_size > 0U) {
                copy_limbs(capacity_span(), src_span);
            }
            if (m_is_sbo && m_sbo_capacity > src_size) {
                zero_limbs(inline_span().subspan(src_size));
            }
        } else if (m_sbo_capacity > 0U && src_size <= m_sbo_capacity) {
            reset_heap();
            if (src_size > 0U) {
                copy_limbs(inline_span(), src_span);
            }
            if (m_sbo_capacity > src_size) {
                zero_limbs(inline_span().subspan(src_size));
            }
            m_data = m_inline_data;
            m_capacity = m_sbo_capacity;
            m_is_sbo = true;
        } else {
            const std::size_t needed{(other.m_capacity > src_size) ? other.m_capacity : (src_size > 0U ? src_size : 1U)};
            std::uint64_t* const new_heap{allocate_limbs(needed)};
            reset_heap();
            m_data = new_heap;
            m_capacity = static_cast<std::uint32_t>(needed);
            m_is_sbo = false;
            if (src_size > 0U) {
                copy_limbs(capacity_span(), src_span);
            }
        }
        m_size = src_size;
        m_sign = other.m_sign;
    }

    /// <summary>
    /// Deep-copies limbs from another BigIntBase and flips the sign bit.
    /// Reuses existing capacity for zero-allocation negated copy when sufficient.
    /// 複製並取負賦值輔助函式：深拷貝另一物件並反轉符號，容量充足時達成零記憶體配置。
    /// </summary>
    /// <param name="other">Source BigIntBase object. / 來源 BigIntBase 物件。</param>
    NUMERIC_CONSTEXPR_20 auto assign_negated(const BigIntBase& other) -> void {
        assign_from(other);
        if (m_size > 0U && m_sign != 0) {
            m_sign = static_cast<std::int8_t>(-m_sign);
        }
    }

    /// <summary>
    /// Move assignment helper: steals heap pointer or copies inline SBO limbs.
    /// 移動賦值輔助函式：若超出 SBO 直接轉移堆積指標所有權，否則複製 SBO 內容。
    /// </summary>
    /// <param name="other">Moved-from source object. / 被移動之來源物件。</param>
    NUMERIC_CONSTEXPR_20 auto move_from(BigIntBase&& other) noexcept -> void {
        if (this == &other) {
            return;
        }
        reset_heap();
        if (m_sbo_capacity > 0U && other.m_size <= m_sbo_capacity) {
            if (other.m_size > 0U) {
                copy_limbs(inline_span(), other.as_span());
            }
            if (m_sbo_capacity > other.m_size) {
                zero_limbs(inline_span().subspan(other.m_size));
            }
            m_data = m_inline_data;
            m_capacity = m_sbo_capacity;
            m_is_sbo = true;
            if (!other.m_is_sbo && other.m_data != nullptr && other.m_data != other.m_inline_data) {
                deallocate_limbs(other.m_data);
            }
            other.m_data = other.m_inline_data;
            other.m_capacity = other.m_sbo_capacity;
            other.m_is_sbo = (other.m_sbo_capacity > 0U);
        } else if (other.m_is_sbo) {
            if (other.m_size > 0U) {
                m_data = allocate_limbs(other.m_size);
                m_capacity = static_cast<std::uint32_t>(other.m_size);
                m_is_sbo = false;
                copy_limbs(capacity_span(), other.as_span());
            } else {
                m_data = m_inline_data;
                m_capacity = m_sbo_capacity;
                m_is_sbo = (m_sbo_capacity > 0U);
            }
        } else {
            m_data = other.m_data;
            m_capacity = other.m_capacity;
            m_is_sbo = false;
            other.m_data = other.m_inline_data;
            other.m_capacity = other.m_sbo_capacity;
            other.m_is_sbo = (other.m_sbo_capacity > 0U);
        }
        m_size = other.m_size;
        m_sign = other.m_sign;
        other.m_size = 0;
        other.m_sign = 0;
        if (other.m_sbo_capacity > 0U) {
            zero_limbs(other.inline_span());
        }
    }

    /// <summary>
    /// Performs in-place sign negation.
    /// 原地單元取負操作。
    /// </summary>
    NUMERIC_CONSTEXPR_20 auto negate() noexcept -> void {
        if (m_size > 0U && m_sign != 0) {
            m_sign = static_cast<std::int8_t>(-m_sign);
        }
    }

    /// <summary>
    /// Reserves heap storage capacity to at least new_cap limbs.
    /// 預留緩衝區容量至至少 new_cap 個 limbs。
    /// </summary>
    /// <param name="new_cap">Requested minimum capacity. / 請求之最小容量。</param>
    NUMERIC_CONSTEXPR_20 auto reserve(const std::size_t new_cap) -> void {
        if (new_cap <= m_capacity) {
            return;
        }
        std::uint64_t* const new_heap{allocate_limbs(new_cap)};
        if (m_size > 0U) {
            copy_limbs(std::span<std::uint64_t>{new_heap, new_cap}, as_span());
        }
        if (!m_is_sbo && m_data != nullptr && m_data != m_inline_data) {
            deallocate_limbs(m_data);
        }
        m_data = new_heap;
        m_capacity = static_cast<std::uint32_t>(new_cap);
        m_is_sbo = false;
    }

    /// <summary>
    /// Resizes active limb count, zero/value-filling newly added limbs if expanded.
    /// 調整有效 limbs 數量大小，若擴張則填入預設值。
    /// </summary>
    /// <param name="new_size">Target limb count. / 目標 limb 數量。</param>
    /// <param name="init_val">Fill value for expanded limbs. / 擴張元素填充初值。</param>
    NUMERIC_CONSTEXPR_20 auto resize(const std::size_t new_size, const std::uint64_t init_val = 0ULL) -> void {
        if (new_size > m_capacity) {
            std::size_t next_cap{static_cast<std::size_t>(m_capacity) * 2U};
            if (next_cap < new_size) {
                next_cap = new_size;
            }
            reserve(next_cap);
        }
        if (new_size > m_size) {
            std::ranges::fill(capacity_span().subspan(m_size, new_size - m_size), init_val);
        }
        m_size = static_cast<std::uint32_t>(new_size);
    }

    /// <summary>
    /// Normalizes limb array by stripping leading zeros and retracting to inline SBO if eligible.
    /// 規範化 limbs 陣列，移除高位無效零肢段；若長度落回 SBO 則縮回內聯緩衝區。
    /// </summary>
    NUMERIC_CONSTEXPR_20 auto normalize() noexcept -> void {
        const std::span<const std::uint64_t> limbs_view{m_data, m_size};
        while (m_size > 0U && limbs_view[m_size - 1U] == 0ULL) {
            --m_size;
        }
        if (m_size == 0U) {
            m_sign = 0;
        }
        shrink_to_sbo_if_possible();
    }

    /// <summary>
    /// Releases unused capacity by shrinking back into inline SBO when possible.
    /// 釋放未使用的堆積容量；若符合 SBO 大小則縮回內聯陣列。
    /// </summary>
    NUMERIC_CONSTEXPR_20 auto shrink_to_fit() noexcept -> void {
        shrink_to_sbo_if_possible();
    }

    /// <summary>
    /// Moves heap data back to inline SBO storage if current size fits within m_sbo_capacity.
    /// 當 limbs 數量小於等於 SBO 容量且目前為堆積配置時，縮回 SBO 緩衝區。
    /// </summary>
    NUMERIC_CONSTEXPR_20 auto shrink_to_sbo_if_possible() noexcept -> void {
        if (!m_is_sbo && m_sbo_capacity > 0U && m_size <= m_sbo_capacity) {
            std::uint64_t* const old_heap{m_data};
            const std::span<std::uint64_t> dst{inline_span()};
            for (std::size_t i{0}; i < m_sbo_capacity; ++i) {
                dst[i] = (i < m_size && old_heap != nullptr) ? old_heap[i] : 0ULL;
            }
            m_data = m_inline_data;
            m_capacity = m_sbo_capacity;
            m_is_sbo = true;
            if (old_heap != nullptr && old_heap != m_inline_data) {
                deallocate_limbs(old_heap);
            }
        }
    }

    /// <summary>
    /// Resets value to zero while preserving buffer allocation.
    /// 清空數值為 0（保留既有配置空間）。
    /// </summary>
    NUMERIC_CONSTEXPR_20 auto clear() noexcept -> void {
        m_size = 0;
        m_sign = 0;
    }

    /// <summary>
    /// Sets storage value from an unsigned 64-bit magnitude and explicit sign.
    /// 設定為 64 位元無符號整數與指定正負號。
    /// </summary>
    /// <param name="val">64-bit unsigned magnitude. / 64 位元無符號大小。</param>
    /// <param name="sign">Sign (-1, 0, or 1). / 正負符號。</param>
    NUMERIC_CONSTEXPR_20 auto set_uint64(const std::uint64_t val, const std::int8_t sign) -> void {
        if (val == 0ULL) {
            m_size = 0;
            m_sign = 0;
            return;
        }
        if (m_sbo_capacity > 0U) {
            reset_heap();
            zero_limbs(inline_span());
            m_size = 1;
            m_sign = sign;
            inline_span()[0] = val;
            m_is_sbo = true;
        } else {
            if (m_capacity < 1U || m_data == nullptr) {
                reserve(1U);
            }
            m_size = 1;
            m_sign = sign;
            capacity_span()[0] = val;
            m_is_sbo = false;
        }
    }
};

#if defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || (defined(__SIZEOF_POINTER__) && __SIZEOF_POINTER__ == 8)
static_assert(sizeof(BigIntBase) == 32, "BigIntBase must be exactly 32 bytes on 64-bit platforms!");
#endif

} // namespace numeric
