# bigint 型別轉換與序列化 (Conversions & Serialization)

介紹 `numeric::bigint` 轉換為原生純量型別、字串表示法與 `std::bitset` 之所有方法與運算子。

## 命名空間與標頭檔
- **命名空間**: `numeric`
- **標頭檔**: `<numeric/BigInt.hpp>`

---

## 轉換方法與運算子清單 (Member Conversion List)

| 轉換目標 | 語法宣告摘要 | 說明 |
| :--- | :--- | :--- |
| `bool` | `explicit operator bool() const noexcept;` | 邏輯真假值轉換（非 0 即真，0 為 false）。 |
| `int64_t` / `int32_t` | `explicit operator int64_t() const noexcept;`<br>`explicit operator int32_t() const noexcept;` | 轉換至 64/32 位元有符號整數（截斷語意）。 |
| `uint64_t` / `uint32_t` | `explicit operator uint64_t() const noexcept;`<br>`explicit operator uint32_t() const noexcept;` | 轉換至 64/32 位元無符號整數（截斷語意）。 |
| `double` | `explicit operator double() const noexcept;` | 轉換至雙精度浮點數（可能捨入）。 |
| `std::string` (十進位) | `std::string to_string() const;` | 序列化為十進位字串（含選用負號）。 |
| `std::string` (二進位) | `std::string to_binary_string() const;` | 序列化為純二進位字串（負數含前綴 `'-'`）。 |
| `std::bitset<N>` | `template <size_t N> std::bitset<N> to_bitset() const;` | **泛型位元集合轉換**：以標準二補數表示法映射至固定寬度 bitset。 |

---

## 詳細說明

### 1. 布林值轉換運算子 `operator bool()`

#### 語法
```cpp
explicit NUMERIC_CONSTEXPR_20 operator bool() const noexcept;
```

#### 傳回值
- `bool`
  - 若數值不為 0，傳回 `true`（完全符合 C 語言「非零即真」規則）。
  - 若數值為 0，傳回 `false`。

#### 範例
```cpp
numeric::bigint val(42);
if (val) {
    std::cout << "val is non-zero\n";
}
```

---

### 2. 原生整數轉換運算子

#### 語法
```cpp
explicit NUMERIC_CONSTEXPR_20 operator int64_t() const noexcept;
explicit NUMERIC_CONSTEXPR_20 operator uint64_t() const noexcept;
explicit NUMERIC_CONSTEXPR_20 operator int32_t() const noexcept;
explicit NUMERIC_CONSTEXPR_20 operator uint32_t() const noexcept;
```

#### 傳回值
- 對應的原生整數值。

#### 注意事項
> [!WARNING]
> 若 `bigint` 的數值範圍超出目標整數寬度，將直接截斷並僅取其最低位的 64 或 32 位元數值並附上正負號，**不會拋出例外**。若需防止溢位，應於轉換前先確認 `limb_count()` 或與邊界常數進行比較。

#### 範例
```cpp
numeric::bigint b(12345);
int64_t val = static_cast<int64_t>(b);
std::cout << "int64: " << val << "\n";
```

---

### 3. 浮點數轉換運算子 `operator double()`

#### 語法
```cpp
explicit operator double() const noexcept;
```

#### 傳回值
- `double`
  對應之 IEEE 754 雙精度浮點數值。若數值超出雙精度最大可表示範圍，將回傳 `std::numeric_limits<double>::infinity()`（正或負無窮大）。

#### 備註
IEEE 754 雙精度浮點數具備 53 位元有效尾數（約 15~17 位十進位有效數字）。大於此精度的超大整數在轉換時，低位位元將因捨入而遺失。

#### 範例
```cpp
numeric::bigint b("100000000000000000000"); // 10^20
double d = static_cast<double>(b);
std::cout << "double: " << d << "\n"; // 輸出: 1e+20
```

---

### 4. `to_string` 方法

將 `bigint` 格式化為標準十進位字串。

#### 語法
```cpp
NUMERIC_NODISCARD std::string to_string() const;
```

#### 傳回值
- `std::string`
  代表該數值的十進位 ASCII 字串。若為負數，開頭包含 `'-'`。零恆表示為 `"0"`。

#### 備註與極致優化架構
內部序列化採用結合分治法（Divide-and-Conquer）、Burnikel-Ziegler 快速長除法與常數乘法求逆除法之現代高效架構，徹底打破傳統逐位除法的 $O(N^2)$ 效能瓶頸：
1. **單 Limb 零除法極速路徑 (Single-Limb Fast Path, ~36 ns)**：
   對於單肢數值（$N = 1$，值域 $\le 2^{64}-1$），完全不調用高階大數演算法：
   - 透過二分搜尋常數比較之 `digits10_u64` 無分支快速求得精確十進位位元數。
   - 採用 32 位元區塊常數乘法求逆將數值拆解為 32 位元整數，避免 64 位元慢速硬體整數除法。
   - 搭配雙位元查找表（2-Digit LUT, `char_table2`）以 16-bit 記憶體寫入兩兩填入棧上 `stack_buf[32]`，最後一次性構造 `std::string`。
   - 單肢 `ToString` 延遲僅需 **~36.1 ns**，超越 .NET 10 Native AOT（~62.4 ns）與 Python（~182.5 ns）。
2. **小規模棧上逆向格式化 (Small Stack Buffer Backward Formatting)**：
   對於 16 個 limbs 以內（$\le 1024$ 位元，涵蓋 128-bit、256-bit SBO）之中小型數值，直接於棧上 512-byte 刮痕緩衝區以分治法與 2-Digit LUT 逆向輸出字元，格式化過程 **0 次初步 Heap 動態配置**。最後一次性建構精確尺寸的 `std::string`。
3. **$10^{19}$ 乘法求逆除法 (Reciprocal Multiplication Division)**：
   對於 Radix-10 轉換最頻繁的 64 位元除數 $10^{19}$（最大可容納十進位冪次），預先計算 128 位元定點逆元常數 $v = \lfloor (2^{128} - 1) / 10^{19} \rfloor - 2^{64} = \text{0xd83c94fb6d2ac34a}$。以乘法高位與進位加法完全取代 x86-64 硬體 `_udiv128` 指令，將單 limb 除法延遲由 ~40 週期劇降至 ~6 週期。
4. **分治進位轉換 (Divide-and-Conquer Radix Conversion)**：
   當數值大於 16 個 chunks（約 304 位數 / 1010 bits）時，自動切換至二分切分演算法。利用 [`Pow10Cache`](../parsing.md) 快取的二分冪次 $10^{19 \cdot 2^k}$，將大數遞迴切半：$Q, R = \operatorname{div\_qr}(A, 10^{19 \cdot 2^k})$。遞迴深度降為 $O(\log N)$，並結合 **Burnikel-Ziegler 快速分治除法**，使 64K-bit `ToString` 耗時進一步由 `710.23 µs` 降低至 **`433.02 µs`**（**1.64x 加速**），大幅超越 C++ MPIR（`1.08 ms`）。
5. **2-Digit LUT 雙位元查表加速**：
   透過靜態內聯的百位查找表（`char_table2`），將數字轉換為兩兩一組的 ASCII 字元（如 `"00"`, `"01"`, ..., `"99"`），將除法與取模指令次數直接減半，並利用 16-bit 記憶體存取就地填入。
6. **精確長度配置 (Exact Digit Allocation)**：
   在最高位區塊透過精確的常數階梯判斷實際位元長度，預先單次精確配置目標 `std::string` 容量，完全避免 `push_back` 擴容與傳統字串反轉（`std::reverse`）之雙重記憶體遍歷開銷。

#### 範例
```cpp
numeric::bigint b("-123456789012345678901234567890");
std::string s = b.to_string();
std::cout << s << "\n"; // 輸出: -123456789012345678901234567890
```

---

### 5. `to_binary_string` 方法

將 `bigint` 格式化為二進位 ASCII 字串。

#### 語法
```cpp
NUMERIC_NODISCARD std::string to_binary_string() const;
```

#### 傳回值
- `std::string`
  以 `'0'` 與 `'1'` 組成的二進位字串。若為負數，開頭包含 `'-'` 符號（例如 `-10` 表示為 `"-1010"`）。

#### 備註
此方法傳回符號-絕對值（Sign-Magnitude）之二進位表示。若需要二補數固定寬度之二進位輸出，建議搭配 `to_bitset<N>()` 使用。

#### 範例
```cpp
numeric::bigint b(42);
std::cout << b.to_binary_string() << "\n"; // 輸出: "101010"

numeric::bigint neg(-42);
std::cout << neg.to_binary_string() << "\n"; // 輸出: "-101010"
```

---

### 6. `to_bitset<N>` 泛型方法

將任意精度整數轉換為固定長度的 `std::bitset<N>`。

#### 語法
```cpp
template <size_t N>
std::bitset<N> to_bitset() const;
```

#### 型別參數
- `size_t N`: 目標位元集合之位元寬度（例如 8, 16, 32, 64, 128, 256 等）。

#### 傳回值
- `std::bitset<N>`
  包含二進位位元之集合。

#### 備註與二補數延伸規則
- **正數**：按無符號純二進位映射至低位元，其餘高位元自動補 `0`。
- **負數**：**嚴格遵循標準二補數（Two's Complement）表示法**。例如，以寬度 `N` 進行表示時，實質相當於計算 $(2^N + b) \pmod{2^N}$，高位元將自動進行符號延伸（全部填充為 `1`）。
- **寬度截斷**：若 $N$ 小於該數值所需位元寬度，僅保留最低 $N$ 個位元。

#### 範例
```cpp
#include <numeric/BigInt.hpp>
#include <iostream>

int main() {
    numeric::bigint pos(5);  // 00000101
    numeric::bigint neg(-5); // 二補數: ...11111011

    std::bitset<8> bs_pos = pos.to_bitset<8>();
    std::bitset<8> bs_neg = neg.to_bitset<8>();

    std::cout << " 5 in bitset<8>: " << bs_pos << "\n"; // 輸出: 00000101
    std::cout << "-5 in bitset<8>: " << bs_neg << "\n"; // 輸出: 11111011

    // 亦支援更大寬度 (如 16-bit)
    std::bitset<16> bs16_neg = neg.to_bitset<16>();
    std::cout << "-5 in bitset<16>: " << bs16_neg << "\n"; // 輸出: 1111111111111011
}
```

---

## 適用於
- 所有轉換方法均適用於 **C++11** 及以上版本。
- 整數與布林型別轉換運算子於 **C++20** 起支援 `constexpr`。
