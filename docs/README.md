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
| `numeric` / `C` | `<numeric/c_api.h>` / `dist/bigint_c.h` | C-ABI (`extern "C"`) 介面，供純 C、Python、Rust、.NET 等 FFI 呼叫。 |
| `Numeric` (.NET) | [`dotnet/src/Numeric.BigInt/`](../dotnet/src/Numeric.BigInt/) | 正式 .NET 10 跨語言（C# 14、F# 10、VB.NET 10）封裝函式庫。 |
| `std` | `<numeric/BigInt.hpp>` | 標準庫特化：支援 `std::hash<numeric::BasicBigInt<SboLimbs>>` 與 `std::formatter<numeric::BasicBigInt<SboLimbs>, CharT>`。 |

### 內部模組化架構 (`numeric::detail`)

為了便於維護與高內聚管理，核心演算法庫拆分為獨立子模組，並透過單一標頭打包器 (`scripts/bundle_header.py`) 自動拓撲排序與重複去重：

| 模組標頭檔 (Detail Header) | 基礎類別 (Class / Struct) | 職責與關鍵演算法 |
| :--- | :--- | :--- |
| `numeric/detail/Intrinsics.hpp` | `BigIntIntrinsics` | 硬體級原語：`adc64`, `sbb64`, `std::countl_zero` (`<bit>`), `mul64_wide`, `div128_64`, 倒數除法加速 |
| `numeric/detail/Storage.hpp` | `BigIntBase` | LLVM SmallVector 雙層解耦架構（`bind_inline`、`as_span`/`capacity_span`、`std::unique_ptr` RAII 管理） |
| `numeric/detail/Arithmetic.hpp` | `BigIntArithmetic` | 無符號與有符號加減法（對稱 SBO 快速路徑、一般性向量運算） |
| `numeric/detail/Bitwise.hpp` | `BigIntBitwise` | 整肢移位、無分支 `shld64`/`shrd64` 移位、4x 展開 `NUMERIC_RESTRICT` 就地位元運算 |
| `numeric/detail/Multiplication.hpp` | `BigIntMultiplication` | Schoolbook、Block Karatsuba、Toom-3（門檻 64 limbs）與指標穩定 `ScratchArena` 記憶體池 |
| `numeric/detail/Division.hpp` | `BigIntDivision` | Knuth Algorithm D（含短商數快路徑）與 Burnikel-Ziegler（門檻 32 limbs）零堆積配置分治除法 |
| `numeric/detail/StringConversion.hpp` | `BigIntStringConversion` | 2-Digit LUT、分治十進位解析與格式化、`Pow10Cache` 冪次快取、constexpr 求值 |
| `numeric/detail/BigIntCore.hpp` | `BigIntCore` | 核心匯聚類別，完整繼承各子模組，保證 100% 原始 API 相容性 |

---

## 文件導覽目錄 (Table of Contents)

### 1. [numeric::bigint 類別](bigint/index.md)
任意精度有符號整數核心型別，具備 **可配置 Small Buffer Optimization (SBO)**、硬體 ADC/SBB 加速與三層乘法架構之高效模型。

- **[建構函式 (Constructors)](bigint/constructors.md)**
  - 預設建構子、複製/移動建構子
  - `explicit` 布林值建構子、C++23 Concepts 約束之任意有符號/無符號整數建構子 (`std::signed_integral` / `std::unsigned_integral`)
  - `explicit` 字串解析建構子 (`numeric::string_view`, `const char*`, `std::string`)
  - `explicit` 泛型位元集合建構子 (`template <std::size_t N> explicit BasicBigInt(const std::bitset<N>&)`)
- **[屬性與狀態檢測 (Properties & Status)](bigint/properties.md)**
  - `is_sbo()` / `is_small()`：SBO 內建緩衝區使用狀態查詢（預設 4 limbs / 256 位元，可擴展至 8 limbs / 512 位元）
  - `is_zero()`：數值為零檢查
  - `sign()`：正負符號檢查 (-1, 0, 1)
  - `limb_count()` / `limbs()` / `as_span()` / `capacity_span()` / `inline_span()`：64-bit 區塊計數、邊界安全 `std::span` 視圖與 `NUMERIC_LIFETIMEBOUND` 生命週期綁定
  - `storage()`：底層儲存結構存取 (`BigIntBase`)
- **[型別轉換與字串化 (Conversions)](bigint/conversions.md)**
  - 明確型別轉換運算子：`bool`, `int64_t`, `uint64_t`, `int32_t`, `uint32_t`, `double`
  - 無例外安全轉換：`try_to_int64()` / `try_to_uint64()`（回傳 `std::expected<..., BigIntError>`）
  - 分治十進位字串轉換：`to_string()`（單肢 `digits10_u64` + 32-bit chunking + 2-Digit LUT 極速路徑 ~36 ns；中大數 $10^{19}$ 乘法求逆除法 + 分治切分）
  - 二進位字串轉換：`to_binary_string()`
  - 泛型位元集合轉換：`to_bitset<N>()`
- **[運算子重載 (Operators)](bigint/operators.md)**
  - 單元運算子：`+`, `-`, 前置/後置 `++`, 前置/後置 `--`, `~`, `!`
  - 算術二元運算子：`+`, `-`, `*`, `/`, `%`（對稱 4-limb SBO 加減法 ~5 ns、三階乘法：Schoolbook → Karatsuba → Toom-Cook 3、常數參考傳遞零深拷貝、棧上 Scratch Buffer 與商餘分離）
  - 複合賦值運算子：`+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`（完整自我別名安全，商餘解耦求值，包含極小 SBO 容量防衛）
  - 位元運算與 Concepts 約束位移運算子 (`std::integral`)：`&`, `|`, `^`, `<<`, `>>`
  - 比較運算子與跨型別混合運算：`==`, `!=`, `<`, `<=`, `>`, `>=`
  - 邏輯運算子 (`std::integral` 約束)：`&&`, `||`
  - 輸出串流運算子：`operator<<`
- **[靜態解析方法與常數代理 (Parsing & Constants)](bigint/parsing.md)**
  - 靜態解析：`from_string()` / `try_from_string()`、`from_binary_string()` / `try_from_binary_string()`（回傳 `std::expected<BasicBigInt, BigIntError>`）
  - 強型別錯誤列舉：`enum class BigIntError : std::uint8_t`
  - 常數代理：`bigint::zero` / `bigint::zero()`, `bigint::one` / `bigint::one()`

### 2. [數學與數論函式 (Mathematics)](math.md)
收錄於 `<numeric/BigIntMath.hpp>`，同時支援 `numeric::` 與 `std::` 命名空間。
- `abs`：絕對值計算
- `try_isqrt`：無例外 `std::expected<BasicBigInt<N>, BigIntError>` 整數平方根
- `isqrt` / `sqrt`：整數平方根（牛頓迭代整數求值，負數拋出例外）
- `icbrt` / `cbrt`：整數立方根（支援負數奇函數）
- `pow`：整數快速冪運算（`detail::pow_tail` 尾遞迴，支援 `std::unsigned_integral`、`std::signed_integral` 與 `BasicBigInt` 指數）
- `gcd`：歐幾里得最大公因數（`detail::gcd_tail` 尾遞迴）
- `lcm`：最小公倍數（防溢位除後乘算法）

### 3. [位元集合互轉 (Bitset Interop)](bitset.md)
收錄於 `<numeric/Bitset.hpp>`，提供任意寬度 `std::bitset<N>` 與 `BasicBigInt<SboLimbs>` 的泛型互轉介面。
- `to_bitset<N>(const BasicBigInt<SboLimbs>&)`：轉換至指定寬度之 bitset（嚴格二補數延伸）
- `to_bigint<N, SboLimbs>(const std::bitset<N>&)`：自 bitset 轉換至非負 `BasicBigInt<SboLimbs>`

### 4. [標準庫擴充特化 (Standard Library Extensions)](extensions.md)
- `std::hash<numeric::BasicBigInt<SboLimbs>>`：FNV-1a 結合 MurmurHash3 Avalanche Finalizer，支援無縫作為無序關聯容器 Key。
- `std::formatter<numeric::BasicBigInt<SboLimbs>, CharT>`：`std::format` 格式化規範與擴充支援。

### 5. [編譯設定與巨集 (Configuration & Macros)](config.md)
收錄於 `<numeric/Config.hpp>`，定義 C++23 基準屬性、`NUMERIC_LIFETIMEBOUND`、`NUMERIC_CONSTEXPR_20_FORCEINLINE`、無例外 (`-fno-exceptions`) 安全回退機制、`std::string_view` 別名及原生硬體加速檢測。

### 6. [C-ABI 介面與跨語言 FFI (C-API)](c_api.md)
收錄於 `<numeric/c_api.h>` 與 `dist/bigint_c.h`，提供 57 個標準 `extern "C"` 介面，支援 Opaque Handle 模式與 256/512 位元棧上零配置 SBO 模式，無縫對接 Python、Rust、.NET 10 (`[DllImport]` / `[LibraryImport]`) 及 Go。

### 7. [.NET 跨語言封裝函式庫 (`Numeric.BigInt`)](dotnet.md)
完整收錄正式 .NET 10 封裝函式庫，提供不可變 `BigInt`（完整運算子重載，支援 **C# 14**、**F# 10** 與 **VB.NET 10**）與零 GC 配置之 `ref struct BigIntScope` / `BigIntSbo256` 堆疊運算介面。

### 8. [跨語言效能評測報告 (Benchmark Report)](../benchmarks/results/BENCHMARK_REPORT.md)
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

## C++23 基準與支援矩陣

本專案嚴格要求 **C++23** 標準（MSVC `/std:c++latest`、GCC/Clang `-std=c++23`）：

| 特性 | C++23 實作與標準函式庫依賴 | 支援度 |
| :--- | :--- | :---: |
| 基礎與 SBO 運算 | `BasicBigInt<SboLimbs>` + `BigIntBase` (0-Heap 256-bit) | ✔ |
| 安全邊界與生命週期 | `std::span`、`std::unique_ptr` RAII、`NUMERIC_LIFETIMEBOUND` | ✔ |
| 型別約束與防誤轉 | `<concepts>` (`std::signed_integral` / `std::unsigned_integral`)、`explicit`、`[[nodiscard]]` | ✔ |
| 無例外單子式錯誤處理 | `std::expected<T, BigIntError>` (`try_*` 介面) | ✔ |
| 字串視圖 `string_view` | `std::string_view` 型別別名 | ✔ |
| `constexpr` 編譯期求值 | 核心構造、四則、比較、位元運算與 `<bit>` 原語 | ✔ |
| `std::format` 格式化輸出 | `std::formatter<numeric::BasicBigInt<SboLimbs>, CharT>`（依 `<version>` 探測） | ✔ |
