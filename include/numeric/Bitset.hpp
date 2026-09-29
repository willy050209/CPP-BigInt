#pragma once

// Bitset.hpp
// Interoperability shims and bidirectional conversions between numeric::BasicBigInt and std::bitset.
// Zero external dependencies, downward compatible from C++23 to C++11.

#include "Config.hpp"
#include "BigInt.hpp"
#include <bitset>
#include <cstddef>

namespace numeric {

    /// <summary>
    /// 將 numeric::BasicBigInt<SboLimbs> 轉換為指定寬度之 std::bitset，負數時採用標準二補數表示法。
    /// </summary>
    /// <typeparam name="N">目標位元寬度</typeparam>
    /// <typeparam name="SboLimbs">SBO 內聯容量</typeparam>
    /// <param name="b">來源任意精度整數</param>
    /// <returns>對應之 std::bitset 物件</returns>
    template <size_t N, size_t SboLimbs = NUMERIC_BIGINT_SBO_LIMBS>
    inline std::bitset<N> to_bitset(const BasicBigInt<SboLimbs>& b) {
        return b.template to_bitset<N>();
    }

    /// <summary>
    /// 將 std::bitset 轉換為非負 numeric::BasicBigInt<SboLimbs>（按無符號二進位數解析）。
    /// </summary>
    /// <typeparam name="N">來源位元寬度</typeparam>
    /// <typeparam name="SboLimbs">目標 SBO 內聯容量</typeparam>
    /// <param name="bs">來源 bitset 物件</param>
    /// <returns>對應之 numeric::BasicBigInt<SboLimbs> 物件</returns>
    template <size_t N, size_t SboLimbs = NUMERIC_BIGINT_SBO_LIMBS>
    inline BasicBigInt<SboLimbs> to_bigint(const std::bitset<N>& bs) {
        return BasicBigInt<SboLimbs>(bs);
    }

} // namespace numeric
