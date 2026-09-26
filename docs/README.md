# CPP-BigInt API 技術參考手冊

歡迎查閱 **CPP-BigInt** 官方 API 技術參考文件。本手冊仿照 **Microsoft Learn / MSDN** 風格編纂，完整收錄任意精度整數類別、泛型模板方法、數論演算法、位元集合互轉以及 C++ 標準庫延伸特化之詳細說明。

---

## 命名空間與組織架構

| 命名空間 (Namespace) | 標頭檔 (Header) | 說明 |
| :--- | :--- | :--- |
| `numeric` | `<numeric/BigInt.hpp>` | 核心模組，提供 `bigint` 任意精度整數類別與運算子。 |
| `numeric` / `std` | `<numeric/BigIntMath.hpp>` | 數論與高精度數學演算法函式庫（平方根、立方根、快速冪、GCD、LCM）。 |
| `numeric` | `<numeric/Bitset.hpp>` | `bigint` 與任意寬度 `std::bitset<N>` 雙向互轉介面。 |
| `numeric` | `<numeric/Config.hpp>` | 編譯期環境檢測、屬性與相容性巨集定義。 |
| `std` | `<numeric/BigInt.hpp>` | 標準庫特化：支援 `std::hash` 與 C++20 `std::formatter`。 |

---

## 文件導覽目錄 (Table of Contents)

### 1. [numeric::bigint 類別](bigint/index.md)
任意精度有符號整數核心型別，具備 **可配置 Small Buffer Optimization (SBO)**、硬體 ADC/SBB 加速與三層乘法架構之高效模型。

- **[建構函式 (Constructors)](bigint/constructors.md)**
  - 預設建構子、複製/移動建構子
  - 原生布林值與整數建構子 (`int8_t` ~ `uint64_t`)
  - 泛型整數模板建構子 (`template <typename T> bigint(T)`)
  - 字串解析建構子 (`string_view`, `const char*`, `std::string`)
  - 泛型位元集合建構子 (`template <size_t N> bigint(const std::bitset<N>&)`)
- **[屬性與狀態檢測 (Properties & Status)](bigint/properties.md)**
  - `is_sbo()` / `is_small()`：SBO 內建緩衝區使用狀態查詢（預設 4 limbs / 256 位元，可擴展至 8 limbs / 512 位元）
  - `is_zero()`：數值為零檢查
  - `sign()`：正負符號檢查 (-1, 0, 1)
  - `limb_count()` / `limbs()`：64-bit 區塊計數與內部陣列存取
  - `storage()`：底層儲存結構存取
- **[型別轉換與字串化 (Conversions)](bigint/conversions.md)**
  - 明確型別轉換運算子：`bool`, `int64_t`, `uint64_t`, `int32_t`, `uint32_t`, `double`
  - 分治十進位字串轉換：`to_string()`（單肢 `digits10_u64` + 32-bit chunking + 2-Digit LUT 極速路徑 ~36 ns；中大數 $10^{19}$ 乘法求逆除法 + 分治切分）
  - 二進位字串轉換：`to_binary_string()`
  - 泛型位元集合轉換：`to_bitset<N>()`
- **[運算子重載 (Operators)](bigint/operators.md)**
  - 單元運算子：`+`, `-`, 前置/後置 `++`, 前置/後置 `--`, `~`, `!`
  - 算術二元運算子：`+`, `-`, `*`, `/`, `%`（對稱 4-limb SBO 加減法 ~5 ns、三階乘法：Schoolbook $\to$ Karatsuba $\to$ Toom-Cook 3、常數參考傳遞零深拷貝、棧上 Scratch Buffer 與商餘分離）
  - 複合賦值運算子：`+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`（完整自我別名安全，商餘解耦求值）
  - 位元運算與位移運算子（含泛型位移）：`&`, `|`, `^`, `<<`, `>>`
  - 比較運算子與跨型別混合運算：`==`, `!=`, `<`, `<=`, `>`, `>=`
  - 邏輯運算子：`&&`, `||`
  - 輸出串流運算子：`operator<<`
- **[靜態解析方法與常數代理 (Parsing & Constants)](bigint/parsing.md)**
  - 靜態解析：`bigint::from_string()`（16-chunk 線性原位 Horner 累積 + 分治平衡樹與 `Pow10Cache` 加速）、`bigint::from_binary_string()`
  - 常數代理：`bigint::zero` / `bigint::zero()`, `bigint::one` / `bigint::one()`

### 2. [數學與數論函式 (Mathematics)](math.md)
收錄於 `<numeric/BigIntMath.hpp>`，同時支援 `numeric::` 與 `std::` 命名空間。
- `abs`：絕對值計算
- `isqrt` / `sqrt`：整數平方根（牛頓迭代整數求值）
- `icbrt` / `cbrt`：整數立方根（支援負數奇函數）
- `pow`：整數快速冪運算（支援 `unsigned int` 與 `bigint` 指數）
- `gcd`：歐幾里得最大公因數
- `lcm`：最小公倍數（防溢位除後乘算法）

### 3. [位元集合互轉 (Bitset Interop)](bitset.md)
收錄於 `<numeric/Bitset.hpp>`，提供任意寬度 `std::bitset<N>` 的泛型互轉介面。
- `to_bitset<N>(const bigint&)`：轉換至指定寬度之 bitset（嚴格二補數延伸）
- `to_bigint<N>(const std::bitset<N>&)`：自 bitset 轉換至非負 bigint

### 4. [標準庫擴充特化 (Standard Library Extensions)](extensions.md)
- `std::hash<numeric::bigint>`：FNV-1a 結合 MurmurHash Avalanche Finalizer，支援無縫作為無序關聯容器 Key。
- `std::formatter<numeric::bigint>`：C++20 `std::format` 格式化規範與客製化寬度/對齊支援。

### 5. [編譯設定與巨集 (Configuration & Macros)](config.md)
收錄於 `<numeric/Config.hpp>`，定義 C++ 版本條件編譯、`NUMERIC_CONSTEXPR_20_FORCEINLINE`、無例外 (`-fno-exceptions`) 安全回退機制、constexpr 支援等級及原生硬體加速檢測。

### 6. [跨語言效能評測報告 (Benchmark Report)](../benchmarks/results/BENCHMARK_REPORT.md)
收錄 `CPP-BigInt`、`C++ GMP (MPIR)`、`.NET 10`、`Python 3.13` 及 `gmpy2` 之 64 至 65,536 bits 完整對比矩陣與效能比值。

---

## 快速開始範例 (Quick Start)

```cpp
#include <numeric/BigInt.hpp>
#include <numeric/BigIntMath.hpp>
#include <numeric/Bitset.hpp>
#include <iostream>

int main() {
    // 1. 初始化（256-bit / 4 limbs 內 0 次 Heap 動態配置）
    numeric::bigint a = 123456789;
    numeric::bigint b("987654321987654321987654321");

    // 2. 算術混算
    numeric::bigint c = a * b + 42;
    std::cout << "c = " << c << "\n";

    // 3. 數論函式
    numeric::bigint root = numeric::isqrt(c);
    std::cout << "isqrt(c) = " << root << "\n";

    // 4. 二進位與 bitset 轉換
    numeric::bigint neg = -42;
    std::bitset<16> bs = numeric::to_bitset<16>(neg);
    std::cout << "-42 in bitset<16>: " << bs << "\n";

    return 0;
}
```

---

## C++ 標準支援矩陣

| 特性 | C++11 | C++14 | C++17 | C++20 | C++23 |
| :--- | :---: | :---: | :---: | :---: | :---: |
| 基礎任意精度運算 | ✔ | ✔ | ✔ | ✔ | ✔ |
| 256-bit SBO 緩衝區 (0 Heap) | ✔ | ✔ | ✔ | ✔ | ✔ |
| ADC / SBB 硬體原語加速 | ✔ | ✔ | ✔ | ✔ | ✔ |
| 8192-bit 0-Heap 字串轉換 | ✔ | ✔ | ✔ | ✔ | ✔ |
| [[nodiscard]] 屬性檢查 | 模擬 | 模擬 | 原生 | 原生 | 原生 |
| 字串視圖 `string_view` | 內建實作 | 內建實作 | `std::string_view` | `std::string_view` | `std::string_view` |
| `constexpr` 編譯期求值 | ✕ | ✕ | ✕ | ✔ | ✔ |
| `std::format` 格式化輸出 | ✕ | ✕ | ✕ | ✔ (若編譯器支援) | ✔ |
