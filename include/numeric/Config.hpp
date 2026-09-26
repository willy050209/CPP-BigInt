#pragma once

// Config.hpp
// Configuration and compiler feature detection for numeric library.
// Zero external dependencies, downward compatible from C++23 to C++11.

#include <cstdlib>

#if defined(_MSVC_LANG)
#  define NUMERIC_CPLUSPLUS _MSVC_LANG
#else
#  define NUMERIC_CPLUSPLUS __cplusplus
#endif

// C++ standard level constants
#define NUMERIC_CXX_11 201103L
#define NUMERIC_CXX_14 201402L
#define NUMERIC_CXX_17 201703L
#define NUMERIC_CXX_20 202002L
#define NUMERIC_CXX_23 202302L

// Small Buffer Optimization (SBO) Limb Capacity
// Default: 4 limbs (256-bit), total sizeof(bigint) == 64 bytes (1 L1 cache line).
// Users can define NUMERIC_BIGINT_SBO_LIMBS=8 (512-bit) for zero-allocation cryptography workloads.
#ifndef NUMERIC_BIGINT_SBO_LIMBS
#  define NUMERIC_BIGINT_SBO_LIMBS 4
#endif

// [[nodiscard]] attribute abstraction
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_17)
#  define NUMERIC_NODISCARD [[nodiscard]]
#elif defined(__GNUC__) || defined(__clang__)
#  define NUMERIC_NODISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER) && (_MSC_VER >= 1700)
#  define NUMERIC_NODISCARD _Check_return_
#else
#  define NUMERIC_NODISCARD
#endif

// Constexpr support for C++14+
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_14)
#  define NUMERIC_CONSTEXPR_14 constexpr
#else
#  define NUMERIC_CONSTEXPR_14 inline
#endif

// Constexpr support for C++20+
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
#  define NUMERIC_CONSTEXPR_20 constexpr
#else
#  define NUMERIC_CONSTEXPR_20 inline
#endif

// Exception handling and throw-or-abort abstraction
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || (defined(_MSC_VER) && defined(_CPPUNWIND))
#  define NUMERIC_HAS_EXCEPTIONS 1
#  define NUMERIC_THROW_OR_ABORT(ex) throw (ex)
#else
#  define NUMERIC_HAS_EXCEPTIONS 0
#  define NUMERIC_THROW_OR_ABORT(ex) std::abort()
#endif

#if defined(_MSC_VER)
#  define NUMERIC_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#  define NUMERIC_ALWAYS_INLINE __attribute__((always_inline)) inline
#else
#  define NUMERIC_ALWAYS_INLINE inline
#endif

// Combined constexpr (C++20+) and forced inline without duplicate 'inline'
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
#  if defined(_MSC_VER)
#    define NUMERIC_CONSTEXPR_20_FORCEINLINE constexpr __forceinline
#  elif defined(__GNUC__) || defined(__clang__)
#    define NUMERIC_CONSTEXPR_20_FORCEINLINE constexpr __attribute__((always_inline)) inline
#  else
#    define NUMERIC_CONSTEXPR_20_FORCEINLINE constexpr inline
#  endif
#else
#  if defined(_MSC_VER)
#    define NUMERIC_CONSTEXPR_20_FORCEINLINE __forceinline
#  elif defined(__GNUC__) || defined(__clang__)
#    define NUMERIC_CONSTEXPR_20_FORCEINLINE __attribute__((always_inline)) inline
#  else
#    define NUMERIC_CONSTEXPR_20_FORCEINLINE inline
#  endif
#endif


// Pointer aliasing hint
#if defined(_MSC_VER)
#  define NUMERIC_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#  define NUMERIC_RESTRICT __restrict__
#else
#  define NUMERIC_RESTRICT
#endif

#if defined(__GNUC__) || defined(__clang__)
#  define NUMERIC_LIKELY(x)   __builtin_expect(!!(x), 1)
#  define NUMERIC_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#  define NUMERIC_LIKELY(x)   (x)
#  define NUMERIC_UNLIKELY(x) (x)
#endif

// Probe standard library version header if available
#if defined(__has_include)
#  if __has_include(<version>)
#    include <version>
#  endif
#endif

// Feature detection: std::format (C++20+)
#if defined(__cpp_lib_format) && (__cpp_lib_format >= 201907L)
#  define NUMERIC_HAS_STD_FORMAT 1
#elif (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20) && defined(_MSC_VER) && (_MSC_VER >= 1929)
#  define NUMERIC_HAS_STD_FORMAT 1
#else
#  define NUMERIC_HAS_STD_FORMAT 0
#endif

// 128-bit integer support detection
#if defined(__SIZEOF_INT128__)
#  define NUMERIC_HAS_INT128 1
#else
#  define NUMERIC_HAS_INT128 0
#endif

// Lightweight string_view abstraction
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_17)
#  include <string_view>
namespace numeric {
    using string_view = std::string_view;
}
#else
#  include <cstddef>
#  include <cstring>
#  include <string>
namespace numeric {
    class string_view {
    private:
        const char* data_;
        size_t size_;
    public:
        constexpr string_view() noexcept : data_(nullptr), size_(0) {}
        constexpr string_view(const char* s, size_t count) noexcept : data_(s), size_(count) {}
        string_view(const char* s) noexcept : data_(s), size_(s ? std::strlen(s) : 0) {}
        string_view(const std::string& s) noexcept : data_(s.data()), size_(s.size()) {}

        constexpr const char* data() const noexcept { return data_; }
        constexpr size_t size() const noexcept { return size_; }
        constexpr size_t length() const noexcept { return size_; }
        constexpr bool empty() const noexcept { return size_ == 0; }
        constexpr char operator[](size_t pos) const noexcept { return data_[pos]; }
        constexpr const char* begin() const noexcept { return data_; }
        constexpr const char* end() const noexcept { return data_ + size_; }
        constexpr string_view substr(size_t pos = 0, size_t count = static_cast<size_t>(-1)) const {
            return (pos >= size_) ? string_view() : string_view(data_ + pos, (count > size_ - pos) ? (size_ - pos) : count);
        }
    };
}
#endif

