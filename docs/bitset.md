# std::bitset 泛型互轉 (Bitset Interoperability)

介紹 `<numeric/Bitset.hpp>` 提供的泛型函式，用於在 `numeric::bigint` 與標準庫固定寬度 `std::bitset<N>` 之間進行雙向轉換。

## 命名空間與標頭檔
- **命名空間**: `numeric`
- **標頭檔**: `<numeric/Bitset.hpp>`

---

## 泛型函式清單 (Function Template List)

| 函式名稱 | 語法宣告摘要 | 說明 |
| :--- | :--- | :--- |
| `to_bitset<N>` | `template <size_t N> std::bitset<N> to_bitset(const bigint& b);` | 將任意精度整數轉換為固定寬度之 `std::bitset<N>`。 |
| `to_bigint<N>` | `template <size_t N> bigint to_bigint(const std::bitset<N>& bs);` | 將 `std::bitset<N>` 轉換為非負任意精度整數。 |

---

## 詳細說明

### 1. `to_bitset<N>` 泛型函式

將 `numeric::bigint` 依據二進位或二補數編碼格式映射至指定寬度之 `std::bitset<N>`。

#### 語法
```cpp
namespace numeric {
    template <size_t N>
    inline std::bitset<N> to_bitset(const bigint& b);
}
```

#### 型別參數
- `size_t N`: 目標 `std::bitset` 之位元寬度。可為任意正整數（如 8, 16, 32, 64, 128, 256, 1024 等）。

#### 參數
- `b`: `const bigint&`
  來源任意精度整數。

#### 傳回值
- `std::bitset<N>`
  轉換後的位元集合物件。

#### 數值編碼與符號延伸規則
- **非負數 ($b \ge 0$)**：
  直接按無符號二進位位元填充至第 $0 \sim N-1$ 位，多餘的高位元自動補 `0`。
- **負數 ($b < 0$)**：
  **嚴格遵循二補數（Two's Complement）表示法**。
  內部計算等價於 $(2^N + b) \pmod{2^N}$。其符號位元向高位自動展開填充為 `1`。
- **截斷行為**：
  若數值的二進位長度大於 $N$，僅保留最低之 $N$ 個位元（捨棄更高位元）。

#### 範例
```cpp
#include <numeric/BigInt.hpp>
#include <numeric/Bitset.hpp>
#include <iostream>

int main() {
    numeric::bigint pos = 13; // 0b1101
    numeric::bigint neg = -5; // 二補數

    // 轉換至 8 位元
    auto bs_pos8 = numeric::to_bitset<8>(pos);
    auto bs_neg8 = numeric::to_bitset<8>(neg);
    std::cout << " 13 in 8-bit : " << bs_pos8 << "\n"; // 輸出: 00001101
    std::cout << " -5 in 8-bit : " << bs_neg8 << "\n"; // 輸出: 11111011

    // 轉換至 16 位元（展示負數二補數符號位元全展開）
    auto bs_neg16 = numeric::to_bitset<16>(neg);
    std::cout << " -5 in 16-bit: " << bs_neg16 << "\n"; // 輸出: 1111111111111011
}
```

---

### 2. `to_bigint<N>` 泛型函式

將 `std::bitset<N>` 按無符號二進位整數解譯並建構為 `numeric::bigint`。

#### 語法
```cpp
namespace numeric {
    template <size_t N>
    inline bigint to_bigint(const std::bitset<N>& bs);
}
```

#### 型別參數
- `size_t N`: 來源 `std::bitset` 的位元寬度。

#### 參數
- `bs`: `const std::bitset<N>&`
  待轉換之位元集合。

#### 傳回值
- `bigint`
  非負的任意精度整數（數值恆大於或等於 0）。

#### 備註
此函式內部透過批次 64-bit 打包處理，能夠快速將超長（如 1024 位元、4096 位元）的位元集合轉換為 `bigint`。

#### 範例
```cpp
#include <numeric/BigInt.hpp>
#include <numeric/Bitset.hpp>
#include <iostream>

int main() {
    std::bitset<128> bs128;
    bs128.set(0);   // 2^0
    bs128.set(127); // 2^127

    numeric::bigint b = numeric::to_bigint(bs128);
    std::cout << "b = " << b << "\n";
    // 輸出: 170141183460469231731687303715884105729 (2^127 + 1)
}
```

---

## 適用於
- C++11 及以上版本。
