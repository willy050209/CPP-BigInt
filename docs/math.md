# 數學與數論函式 (Mathematics & Number Theory)

介紹 `<numeric/BigIntMath.hpp>` 提供的任意精度整數數論與進階數學運算函式。所有函式皆同時提供於 `numeric::` 命名空間與標準庫 `std::` 命名空間重載。

## 命名空間與標頭檔
- **命名空間**: `numeric` 與 `std`
- **標頭檔**: `<numeric/BigIntMath.hpp>`

---

## 函式清單總覽 (Function List)

| 函式名稱 | 語法宣告摘要 | 說明 |
| :--- | :--- | :--- |
| `abs` | `template <std::size_t N> auto abs(const BasicBigInt<N>& x) noexcept -> BasicBigInt<N>;` | 計算整數絕對值。 |
| `try_isqrt` | `template <std::size_t N> auto try_isqrt(const BasicBigInt<N>& x) -> std::expected<BasicBigInt<N>, BigIntError>;` | 無例外單子式整數平方根（負數輸入回傳 `BigIntError::NegativeInput`）。 |
| `isqrt` | `template <std::size_t N> auto isqrt(const BasicBigInt<N>& x) -> BasicBigInt<N>;` | 計算整數平方根（回傳最大滿足 $r^2 \le x$ 之整數；負數拋出例外）。 |
| `sqrt` | `template <std::size_t N> auto sqrt(const BasicBigInt<N>& x) -> BasicBigInt<N>;` | `isqrt` 之別名函式。 |
| `icbrt` | `template <std::size_t N> auto icbrt(const BasicBigInt<N>& x) noexcept -> BasicBigInt<N>;` | 計算整數立方根（支援負數奇函數性質）。 |
| `cbrt` | `template <std::size_t N> auto cbrt(const BasicBigInt<N>& x) noexcept -> BasicBigInt<N>;` | `icbrt` 之別名函式。 |
| `pow` | `template <std::size_t N, std::unsigned_integral ExpT> auto pow(const BasicBigInt<N>& base, ExpT exp) -> BasicBigInt<N>;`<br>`template <std::size_t N, std::signed_integral ExpT> auto pow(const BasicBigInt<N>& base, ExpT exp) -> BasicBigInt<N>;`<br>`template <std::size_t N1, std::size_t N2> auto pow(const BasicBigInt<N1>& base, const BasicBigInt<N2>& exp) -> BasicBigInt<N1>;` | 快速冪次運算（支援無符號/有符號純量與任意精度指數；純量指數採用尾遞迴 `detail::pow_tail`）。 |
| `gcd` | `template <std::size_t N1, std::size_t N2> auto gcd(BasicBigInt<N1> a, const BasicBigInt<N2>& b) -> BasicBigInt<N1>;` | 計算兩整數之最大公因數（透過尾遞迴 `detail::gcd_tail` 實作）。 |
| `lcm` | `template <std::size_t N1, std::size_t N2> auto lcm(const BasicBigInt<N1>& a, const BasicBigInt<N2>& b) -> BasicBigInt<N1>;` | 計算兩整數之最小公倍數（防溢位除後乘算法）。 |

---

## 詳細函式說明

### 1. `abs` 函式

計算 `bigint` 之絕對值。

#### 語法
```cpp
namespace numeric {
    NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 inline bigint abs(const bigint& x) noexcept;
}
namespace std {
    NUMERIC_CONSTEXPR_20 inline numeric::bigint abs(const numeric::bigint& x) noexcept;
}
```

#### 參數
- `x`: 輸入整數。

#### 傳回值
- `bigint`: 若 $x < 0$ 回傳 $-x$；否則回傳 $x$。

#### 範例
```cpp
numeric::bigint val = -123456789;
std::cout << numeric::abs(val) << "\n"; // 輸出: 123456789
std::cout << std::abs(val)     << "\n"; // 輸出: 123456789
```

---

### 2. `try_isqrt` / `isqrt` / `sqrt` 函式

計算任意精度整數之**整數平方根**（向下取整）。`try_isqrt` 提供無例外回傳 `std::expected<BasicBigInt<N>, BigIntError>` 的安全路徑；`isqrt` 與 `sqrt` 則於負數輸入時拋出 `std::invalid_argument`。

#### 語法
```cpp
namespace numeric {
    template <std::size_t N = NUMERIC_BIGINT_SBO_LIMBS>
    [[nodiscard]] inline auto try_isqrt(const BasicBigInt<N>& x) -> std::expected<BasicBigInt<N>, BigIntError>;

    template <std::size_t N = NUMERIC_BIGINT_SBO_LIMBS>
    NUMERIC_NODISCARD inline auto isqrt(const BasicBigInt<N>& x) -> BasicBigInt<N>;

    template <std::size_t N = NUMERIC_BIGINT_SBO_LIMBS>
    NUMERIC_NODISCARD inline auto sqrt(const BasicBigInt<N>& x) -> BasicBigInt<N>;
}
namespace std {
    template <std::size_t N>
    NUMERIC_NODISCARD inline auto isqrt(const numeric::BasicBigInt<N>& x) -> numeric::BasicBigInt<N>;

    template <std::size_t N>
    NUMERIC_NODISCARD inline auto sqrt(const numeric::BasicBigInt<N>& x) -> numeric::BasicBigInt<N>;
}
```

#### 參數
- `x`: `const BasicBigInt<N>&`
  待開平方之非負整數。

#### 傳回值與錯誤碼
- `try_isqrt`：非負輸入回傳最大滿足 $r^2 \le x < (r + 1)^2$ 的非負整數 $r$；若 `x.sign() < 0`，回傳 `std::unexpected{BigIntError::NegativeInput}`。
- `isqrt` / `sqrt`：成功時回傳整數 $r$（若 $x = 0$ 回傳 $0$）。

#### 例外狀況
- `std::invalid_argument`（僅 `isqrt` / `sqrt`）
  當傳入的數值為負數（`x.sign() < 0`）時拋出。

#### 演算法備註
使用 `<bit>` 標準函式 `std::countl_zero` 與 `x.as_span()` 計算精確位元寬度來設定初值，再搭配 **Newton-Raphson（牛頓切線法）** 整數迭代：

$$x_{k+1} = \left\lfloor \frac{x_k + \lfloor x / x_k \rfloor}{2} \right\rfloor$$

迭代過程具二次收斂速度（Quadratic Convergence），能迅速在幾次迴圈內求得數百位整數的精準平方根。

#### 範例
```cpp
numeric::bigint perfect_square("100000000000000000000"); // 10^20
std::cout << numeric::isqrt(perfect_square) << "\n";      // 輸出: 10000000000 (10^10)

if (auto res = numeric::try_isqrt(numeric::bigint(-4)); !res) {
    // res.error() == numeric::BigIntError::NegativeInput
}
```

---

### 3. `icbrt` / `cbrt` 函式

計算整數立方根。

#### 語法
```cpp
namespace numeric {
    NUMERIC_NODISCARD inline bigint icbrt(const bigint& x) noexcept;
    NUMERIC_NODISCARD inline bigint cbrt(const bigint& x) noexcept;
}
namespace std {
    inline numeric::bigint icbrt(const numeric::bigint& x) noexcept;
    inline numeric::bigint cbrt(const numeric::bigint& x) noexcept;
}
```

#### 參數
- `x`: `const bigint&`
  任意整數（支援正數、零與負數）。

#### 傳回值
- `bigint`
  回傳整數立方根 $r$。

#### 備註
- `icbrt` 具備**奇函數性質**：對於負數 $x < 0$，滿足 $\operatorname{icbrt}(x) = -\operatorname{icbrt}(-x)$，**不會拋出例外**。
- 同樣基於牛頓法整數立方根迭代演算。

#### 範例
```cpp
std::cout << numeric::icbrt(numeric::bigint(27))  << "\n"; // 輸出: 3
std::cout << numeric::icbrt(numeric::bigint(-27)) << "\n"; // 輸出: -3
std::cout << numeric::icbrt(numeric::bigint(30))  << "\n"; // 輸出: 3 (3^3 = 27 <= 30)
```

---

### 4. `pow` 函式

計算任意精度整數的次方冪。

#### 語法
```cpp
namespace numeric {
    template <std::size_t N = NUMERIC_BIGINT_SBO_LIMBS, std::unsigned_integral ExpT = std::uint32_t>
    NUMERIC_NODISCARD inline auto pow(const BasicBigInt<N>& base, ExpT exp) -> BasicBigInt<N>;

    template <std::size_t N = NUMERIC_BIGINT_SBO_LIMBS, std::signed_integral ExpT>
        requires (!std::same_as<ExpT, bool>)
    NUMERIC_NODISCARD inline auto pow(const BasicBigInt<N>& base, ExpT exp) -> BasicBigInt<N>;

    template <std::size_t N1 = NUMERIC_BIGINT_SBO_LIMBS, std::size_t N2 = NUMERIC_BIGINT_SBO_LIMBS>
    NUMERIC_NODISCARD inline auto pow(const BasicBigInt<N1>& base, const BasicBigInt<N2>& exp) -> BasicBigInt<N1>;
}
namespace std {
    template <std::size_t N, std::integral ExpT>
    NUMERIC_NODISCARD inline auto pow(const numeric::BasicBigInt<N>& base, ExpT exp) -> numeric::BasicBigInt<N>;

    template <std::size_t N1, std::size_t N2>
    NUMERIC_NODISCARD inline auto pow(const numeric::BasicBigInt<N1>& base, const numeric::BasicBigInt<N2>& exp) -> numeric::BasicBigInt<N1>;
}
```

#### 參數
- `base`: `const BasicBigInt<N>&` 底數。
- `exp`: 指數。透過 C++23 Concepts 支援任意有符號/無符號整數（`ExpT`，排除 `bool`）或 `BasicBigInt<N2>`。

#### 傳回值
- `BasicBigInt<N>`: 計算結果 $\text{base}^{\text{exp}}$。

#### 例外狀況
- `std::invalid_argument`:
  若指數為負數（`exp < 0`），且底數不是 `1` 或 `-1` 時拋出（因為整數運算不支援小數分數結果）。

#### 備註
純量指數多載採用**累加器尾遞迴快速冪 (`detail::pow_tail`)** 處於純尾位置進行迭代平方乘冪；大數指數則採二進位平方乘冪循環，皆於 $O(\log(\text{exp}))$ 次乘法內完成計算。

#### 範例
```cpp
numeric::bigint base = 2;
std::cout << numeric::pow(base, 64) << "\n"; // 輸出: 18446744073709551616

// 大數指數
numeric::bigint b = 3;
numeric::bigint e = 10;
std::cout << numeric::pow(b, e) << "\n";     // 輸出: 59049
```

---

### 5. `gcd` 與 `lcm` 函式

計算兩數的最大公因數與最小公倍數。

#### 語法
```cpp
namespace numeric {
    template <std::size_t N1 = NUMERIC_BIGINT_SBO_LIMBS, std::size_t N2 = NUMERIC_BIGINT_SBO_LIMBS>
    NUMERIC_NODISCARD inline auto gcd(BasicBigInt<N1> a, const BasicBigInt<N2>& b) -> BasicBigInt<N1>;

    template <std::size_t N1 = NUMERIC_BIGINT_SBO_LIMBS, std::size_t N2 = NUMERIC_BIGINT_SBO_LIMBS>
    NUMERIC_NODISCARD inline auto lcm(const BasicBigInt<N1>& a, const BasicBigInt<N2>& b) -> BasicBigInt<N1>;
}
namespace std {
    template <std::size_t N1, std::size_t N2>
    NUMERIC_NODISCARD inline auto gcd(const numeric::BasicBigInt<N1>& a, const numeric::BasicBigInt<N2>& b) -> numeric::BasicBigInt<N1>;

    template <std::size_t N1, std::size_t N2>
    NUMERIC_NODISCARD inline auto lcm(const numeric::BasicBigInt<N1>& a, const numeric::BasicBigInt<N2>& b) -> numeric::BasicBigInt<N1>;
}
```

#### 參數
- `a`, `b`: 輸入的兩個任意精度整數（支援不同 SBO 容量混算）。

#### 傳回值
- `gcd`：回傳最大公因數（恆為非負數）。
- `lcm`：回傳最小公倍數（恆為非負數；若其中一數為 0 則回傳 0）。

#### 演算法備註
- `gcd` 內部採用純尾位置移動傳遞之歐幾里得輾轉相除尾遞迴 (`detail::gcd_tail`)。
- `lcm` 採用防溢位運算次序：先除後乘 $\operatorname{lcm}(a, b) = (|a| / \gcd(a, b)) \times |b|$。

#### 範例
```cpp
numeric::bigint a = 48;
numeric::bigint b = 18;

std::cout << "gcd: " << numeric::gcd(a, b) << "\n"; // 輸出: 6
std::cout << "lcm: " << numeric::lcm(a, b) << "\n"; // 輸出: 144
```

---

## 適用於
- **C++23** 嚴格標準環境（支援 Concepts 約束、`<bit>` `std::countl_zero`、`std::expected` 與 `abs` 之 `constexpr` 求值）。
