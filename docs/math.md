# 數學與數論函式 (Mathematics & Number Theory)

介紹 `<numeric/BigIntMath.hpp>` 提供的任意精度整數數論與進階數學運算函式。所有函式皆同時提供於 `numeric::` 命名空間與標準庫 `std::` 命名空間重載。

## 命名空間與標頭檔
- **命名空間**: `numeric` 與 `std`
- **標頭檔**: `<numeric/BigIntMath.hpp>`

---

## 函式清單總覽 (Function List)

| 函式名稱 | 語法宣告摘要 | 說明 |
| :--- | :--- | :--- |
| `abs` | `bigint abs(const bigint& x) noexcept;` | 計算整數絕對值。 |
| `isqrt` | `bigint isqrt(const bigint& x);` | 計算整數平方根（回傳最大滿足 $r^2 \le x$ 之整數）。 |
| `sqrt` | `bigint sqrt(const bigint& x);` | `isqrt` 之別名函式。 |
| `icbrt` | `bigint icbrt(const bigint& x) noexcept;` | 計算整數立方根（支援負數奇函數性質）。 |
| `cbrt` | `bigint cbrt(const bigint& x) noexcept;` | `icbrt` 之別名函式。 |
| `pow` | `bigint pow(const bigint& base, unsigned int exp);`<br>`bigint pow(const bigint& base, const bigint& exp);` | 快速冪次運算（支援純量與 `bigint` 指數）。 |
| `gcd` | `bigint gcd(bigint a, bigint b);` | 計算兩整數之最大公因數（Greatest Common Divisor）。 |
| `lcm` | `bigint lcm(const bigint& a, const bigint& b);` | 計算兩整數之最小公倍數（Least Common Multiple）。 |

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

### 2. `isqrt` / `sqrt` 函式

計算任意精度整數之**整數平方根**（向下取整）。

#### 語法
```cpp
namespace numeric {
    NUMERIC_NODISCARD inline bigint isqrt(const bigint& x);
    NUMERIC_NODISCARD inline bigint sqrt(const bigint& x);
}
namespace std {
    inline numeric::bigint isqrt(const numeric::bigint& x);
    inline numeric::bigint sqrt(const numeric::bigint& x);
}
```

#### 參數
- `x`: `const bigint&`
  待開平方之非負整數。

#### 傳回值
- `bigint`
  回傳滿足 $r^2 \le x < (r + 1)^2$ 的最大非負整數 $r$。若 $x = 0$ 則傳回 $0$。

#### 例外狀況
- `std::invalid_argument`
  當傳入的數值為負數（`x.sign() < 0`）時拋出。

#### 演算法備註
採用位元長度初值猜測搭配 **Newton-Raphson（牛頓切線法）** 整數迭代：

$$x_{k+1} = \left\lfloor \frac{x_k + \lfloor x / x_k \rfloor}{2} \right\rfloor$$

迭代過程具二次收斂速度（Quadratic Convergence），能迅速在幾次迴圈內求得數百位整數的精準平方根。

#### 範例
```cpp
numeric::bigint perfect_square("100000000000000000000"); // 10^20
std::cout << numeric::isqrt(perfect_square) << "\n";      // 輸出: 10000000000 (10^10)

numeric::bigint non_perfect(10);
std::cout << numeric::isqrt(non_perfect) << "\n";         // 輸出: 3 (3^2 <= 10 < 4^2)
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
    NUMERIC_NODISCARD inline bigint pow(const bigint& base, unsigned int exp);
    NUMERIC_NODISCARD inline bigint pow(const bigint& base, const bigint& exp);
}
namespace std {
    inline numeric::bigint pow(const numeric::bigint& base, unsigned int exp);
    inline numeric::bigint pow(const numeric::bigint& base, const numeric::bigint& exp);
}
```

#### 參數
- `base`: `const bigint&` 底數。
- `exp`: 指數。可為 `unsigned int` 或 `numeric::bigint`。

#### 傳回值
- `bigint`: 計算結果 $\text{base}^{\text{exp}}$。

#### 例外狀況
- `std::invalid_argument`:
  若指數為負數（`exp < 0`），且底數不是 `1` 或 `-1` 時拋出（因為整數運算不支援小數分數結果）。

#### 備註
採用二進位**快速冪演算法 (Exponentiation by Squaring)**，在 $O(\log(\text{exp}))$ 次乘法內完成計算。

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
    NUMERIC_NODISCARD inline bigint gcd(bigint a, bigint b);
    NUMERIC_NODISCARD inline bigint lcm(const bigint& a, const bigint& b);
}
namespace std {
    inline numeric::bigint gcd(const numeric::bigint& a, const numeric::bigint& b);
    inline numeric::bigint lcm(const numeric::bigint& a, const numeric::bigint& b);
}
```

#### 參數
- `a`, `b`: 輸入的兩個整數。

#### 傳回值
- `gcd`：回傳最大公因數（恆為非負數）。
- `lcm`：回傳最小公倍數（恆為非負數；若其中一數為 0 則回傳 0）。

#### 演算法備註
- `gcd` 採用輾轉相除法（Euclidean Algorithm）。
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
- C++11 及以上版本。
- `abs` 於 C++20 起支援 `constexpr`。
