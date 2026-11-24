// filepath: include/numeric/Config.hpp
/**
 * @file Config.hpp
 * @brief Modern C++23 compiler feature detection, inline/constexpr attributes, and safety macros for CPP-BigInt.
 *        Defines standard SBO capacity, hardware capability probes, and exception/abort fallbacks.
 *        現代 C++23 編譯器特性檢測、內聯與常數屬性，以及 CPP-BigInt 安全巨集設定。
 *        定義預設 SBO 容量、硬體指令集探測以及例外/中止回退機制。
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

#if defined(_MSVC_LANG)
#  define NUMERIC_CPLUSPLUS _MSVC_LANG
#else
#  define NUMERIC_CPLUSPLUS __cplusplus
#endif

// C++23 standard version baseline constant
// C++23 語言標準基準常數
#define NUMERIC_CXX_23 202302L

// Enforce C++23 or later (allows MSVC /std:c++latest >= 202004L and GCC/Clang -std=c++23 >= 202302L)
// 強制要求 C++23 或更新標準；舊版 C++ (C++11~20) 或 C 專案請改用 C-ABI 匯出函式庫 (c_api.h / bigint_c.h)
#if NUMERIC_CPLUSPLUS <= 202002L
#  error "CPP-BigInt requires C++23 or later (use /std:c++latest on MSVC or -std=c++23 on GCC/Clang). For legacy C++11~20 or C projects, link against the exported C-API library."
#endif

// Small Buffer Optimization (SBO) limb capacity: default 4 limbs (256-bit, 64 bytes total)
// 小型緩衝區最佳化 (SBO) 肢數量容量：預設 4 limbs（256 位元，總計 64 位元組剛好對齊單一 L1 快取行）
#ifndef NUMERIC_BIGINT_SBO_LIMBS
#  define NUMERIC_BIGINT_SBO_LIMBS 4
#endif

// Mandatory [[nodiscard]] attribute for pure calculations and status values
// 純運算與狀態回傳之強制 [[nodiscard]] 屬性標註
#define NUMERIC_NODISCARD [[nodiscard]]

// Compile-time constexpr evaluation directive (C++23 baseline)
// 編譯期常數求值 (constexpr) 指令（C++23 基準）
#define NUMERIC_CONSTEXPR_20 constexpr

// Lifetimebound attribute for views/references bound to *this or parameters
// 衍生視圖與參考生命週期綁定標註，防止懸空參考
#if defined(_MSC_VER) && !defined(__clang__)
#  define NUMERIC_LIFETIMEBOUND [[msvc::lifetimebound]]
#else
#  define NUMERIC_LIFETIMEBOUND
#endif

// Exception handling and fail-fast abort abstraction
// 例外處理與無法啟用例外時之即刻中止 (Fail-Fast) 抽象
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || (defined(_MSC_VER) && defined(_CPPUNWIND))
#  define NUMERIC_HAS_EXCEPTIONS 1
#  define NUMERIC_THROW_OR_ABORT(ex) throw (ex)
#else
#  define NUMERIC_HAS_EXCEPTIONS 0
#  define NUMERIC_THROW_OR_ABORT(ex) std::abort()
#endif

// Force-inline hotpath directive
// 熱路徑強制內聯指令
#if defined(_MSC_VER)
#  define NUMERIC_ALWAYS_INLINE __forceinline
#  define NUMERIC_CONSTEXPR_20_FORCEINLINE constexpr __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#  define NUMERIC_ALWAYS_INLINE __attribute__((always_inline)) inline
#  define NUMERIC_CONSTEXPR_20_FORCEINLINE constexpr __attribute__((always_inline)) inline
#else
#  define NUMERIC_ALWAYS_INLINE inline
#  define NUMERIC_CONSTEXPR_20_FORCEINLINE constexpr inline
#endif

// Strict pointer non-aliasing hint
// 嚴格指標無別名提示
#if defined(_MSC_VER)
#  define NUMERIC_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#  define NUMERIC_RESTRICT __restrict__
#else
#  define NUMERIC_RESTRICT
#endif

// Branch prediction hints
// 分支預測提示
#if defined(__GNUC__) || defined(__clang__)
#  define NUMERIC_LIKELY(x)   __builtin_expect(!!(x), 1)
#  define NUMERIC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define NUMERIC_LIKELY(x)   (x)
#  define NUMERIC_UNLIKELY(x) (x)
#endif

// Standard library feature probe
// 標準程式庫特性探測
#if defined(__has_include)
#  if __has_include(<version>)
#    include <version>
#  endif
#endif

// Feature detection: std::format (C++20/23)
// 特性偵測：std::format（C++20/23）
#if defined(__cpp_lib_format) && (__cpp_lib_format >= 201907L)
#  define NUMERIC_HAS_STD_FORMAT 1
#elif defined(_MSC_VER) && (_MSC_VER >= 1929)
#  define NUMERIC_HAS_STD_FORMAT 1
#else
#  define NUMERIC_HAS_STD_FORMAT 0
#endif

// Native 128-bit integer extension detection
// 原生 128 位元整數擴充偵測
#if defined(__SIZEOF_INT128__)
#  define NUMERIC_HAS_INT128 1
#else
#  define NUMERIC_HAS_INT128 0
#endif

namespace numeric {

/// <summary>
/// Read-only string view alias standardized on std::string_view in C++23.
/// 在 C++23 下直接對齊 std::string_view 的唯讀字串視圖別名。
/// </summary>
using string_view = std::string_view;

} // namespace numeric
