#pragma once

// BigIntCore.hpp
// Core arbitrary-precision integer algorithms and SBO storage layer for CPP-BigInt.
// Zero external dependencies, downward compatible from C++23 to C++11.
// Modularized into Intrinsics, Storage, Arithmetic, Bitwise, Multiplication, Division, and StringConversion.

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
/// BigInt 演算法核心類別，繼承自 BigIntStringConversion（具備完整算術、乘除、位元與字串轉換功能）。
/// 提供 100% 向後相容之靜態方法與演算法入口。
/// </summary>
class BigIntCore : public BigIntStringConversion {
public:
    // 所有演算法與常數（如 KARATSUBA_THRESHOLD, TOOM3_THRESHOLD, BZ_THRESHOLD,
    // FROM_STRING_DC_THRESHOLD, adc64, sbb64, clz64, mul64_wide, div128_64,
    // add_unsigned, sub_unsigned, mul_signed, div_qr_signed, bitwise_*, to_string, from_string 等）
    // 均透過公開繼承自 BigIntStringConversion -> BigIntDivision -> BigIntMultiplication
    // -> BigIntBitwise -> BigIntArithmetic -> BigIntIntrinsics 完整提供，
    // 維持 100% 原始 API 相容性且零額外執行期或二進位開銷。
};

} // namespace detail

using BigIntCore = detail::BigIntCore;

} // namespace numeric
