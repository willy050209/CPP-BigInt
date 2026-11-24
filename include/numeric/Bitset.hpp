// filepath: include/numeric/Bitset.hpp
/**
 * @file Bitset.hpp
 * @brief Bidirectional conversion helpers between numeric::BasicBigInt and std::bitset&lt;N&gt;.
 *        Enforces exact two's complement extension for negative values.
 *        提供 numeric::BasicBigInt 與任意寬度 std::bitset&lt;N&gt; 雙向轉換輔助介面，
 *        負數時嚴格遵循標準二補數延伸。
 */
#pragma once

#include "BigInt.hpp"
#include "Config.hpp"
#include <bitset>
#include <cstddef>

namespace numeric {

    /// <summary>
    /// Converts numeric::BasicBigInt&lt;SboLimbs&gt; into std::bitset&lt;N&gt; with two's complement sign extension.
    /// 將 numeric::BasicBigInt&lt;SboLimbs&gt; 轉換為指定寬度之 std::bitset，負數採用標準二補數表示法。
    /// </summary>
    /// <typeparam name="N">Target bitset width. / 目標位元寬度。</typeparam>
    /// <typeparam name="SboLimbs">Source SBO limb capacity. / 來源 SBO 內聯容量。</typeparam>
    /// <param name="b">Source integer. / 來源任意精度整數。</param>
    /// <returns>Converted std::bitset&lt;N&gt;. / 對應之 std::bitset 物件。</returns>
    template <std::size_t N, std::size_t SboLimbs = NUMERIC_BIGINT_SBO_LIMBS>
    NUMERIC_NODISCARD inline auto to_bitset(const BasicBigInt<SboLimbs>& b) -> std::bitset<N> {
        return b.template to_bitset<N>();
    }

    /// <summary>
    /// Converts std::bitset&lt;N&gt; into a non-negative numeric::BasicBigInt&lt;SboLimbs&gt;.
    /// 將 std::bitset&lt;N&gt; 轉換為非負之 numeric::BasicBigInt&lt;SboLimbs&gt;（按無符號二進位數解析）。
    /// </summary>
    /// <typeparam name="N">Source bitset width. / 來源位元寬度。</typeparam>
    /// <typeparam name="SboLimbs">Target SBO limb capacity. / 目標 SBO 內聯容量。</typeparam>
    /// <param name="bs">Source bitset. / 來源 bitset 物件。</param>
    /// <returns>Non-negative BasicBigInt. / 非負 BasicBigInt 物件。</returns>
    template <std::size_t N, std::size_t SboLimbs = NUMERIC_BIGINT_SBO_LIMBS>
    NUMERIC_NODISCARD inline auto to_bigint(const std::bitset<N>& bs) -> BasicBigInt<SboLimbs> {
        return BasicBigInt<SboLimbs>{bs};
    }

} // namespace numeric
