// filepath: include/numeric/detail/BigIntCore.hpp
/**
 * @file BigIntCore.hpp
 * @brief Unified arbitrary-precision integer core combining intrinsics, SBO storage,
 *        arithmetic, bitwise logic, Karatsuba/Toom-3 multiplication, Knuth/BZ division, and string I/O.
 *        彙聚硬體原語、SBO 儲存、四則運算、位元邏輯、高階乘除法與字串轉換之核心介面。
 */
#pragma once

#include "../Config.hpp"
#include "Intrinsics.hpp"
#include "Storage.hpp"
#include "Arithmetic.hpp"
#include "Bitwise.hpp"
#include "Multiplication.hpp"
#include "Division.hpp"
#include "StringConversion.hpp"

namespace numeric {
namespace detail {

/// <summary>
/// Unified BigInt algorithm core class inheriting BigIntStringConversion.
/// Provides backward-compatible static entry points with zero runtime overhead.
/// BigInt 演算法核心類別，繼承自 BigIntStringConversion，提供零開銷靜態演算法入口。
/// </summary>
class BigIntCore : public BigIntStringConversion {
public:
    // Inherits full arithmetic, multiplication, division, bitwise, and string conversion operations.
    // 透過公開繼承完整提供所有進階算術、乘除、位元與字串轉換演算法入口。
};

} // namespace detail

using BigIntCore = detail::BigIntCore;

} // namespace numeric
