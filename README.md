# CPP-BigInt

高效、跨標準、零外部依賴的現代 C++ 任意精度整數函式庫 (Arbitrary-Precision Integer Library)。

向下相容至 **C++11**，並於 **C++20+** 全面支援編譯期常數求值 (`constexpr`)。

---

## 核心特性

- **256-bit Small Buffer Optimization (SBO)**：
  - 數值在 256 位元（4 個 64-bit limbs）以內時，**0 次 Heap 動態記憶體配置**。
  - 當數值超過 256 位元時自動平滑晉升至動態陣列；運算回縮至 256 位元以內時自動退回 SBO，極大化快取局部性。
- **高效數值運算法與硬體原語加速**：
  - 加減法：直接編譯為硬體進位/借位指令（MSVC `_addcarry_u64` / `_subborrow_u64`，GCC/Clang `_addcarry_u64` / `__builtin_addcll`），配合 SBO 4-limb 完全展開迴圈與進位斷鏈提早跳出（Early-Exit）。
  - 乘法：單肢段快速分派 + 小規模學校乘法 (Schoolbook) + 大整數 Karatsuba 分治演算法 ($O(N^{\log_2 3})$)，內建外置刮痕緩衝區（Scratchpad）以避免遞迴分配，雙目運算子採常數參考傳入杜絕非必要堆積深拷貝。
  - 除法與取模：Knuth Algorithm D 規格化長除法演算法，支援 16K-bit 棧上 Scratch Buffer（0 Heap 配置）以及商餘解耦求值（`div_q` / `div_r`）。
- **極致字串序列化與解析 (0-Heap & 分治轉換)**：
  - 十進位格式化：結合 $10^{19}$ 乘法求逆除法（Reciprocal Division）與二分分治切分（Divide-and-Conquer），徹底消滅 $O(N^2)$ 瓶頸。中小數值直接棧上展開，大整數序列化速度提升達 64.2%（64K-bit 僅需 0.71 ms，超越 MPIR 1.52x）。
  - 十進位解析：採用 16-chunk 原地 Horner 累積與二分樹狀平衡折疊（`Pow10Cache` 快取），兼顧中小字串零配置與超大數亞二次方極速解析。
- **豐富的運算子支援**：
  - 算術運算：`+`, `-`, `*`, `/`, `%`, `++`, `--` 及複合賦值運算子（全面支援自我別名安全性與雙目常數參考傳參）。
  - 位元運算：`&`, `|`, `^`, `~`, `<<`, `>>`（完整二補數 Two's Complement 語意）。
  - 比較與邏輯：`==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `||`, `!`（支援 C 語言「非零即真」語意）。
  - 跨型別混算：無縫與所有原生整數型態（`int8_t` ~ `uint64_t`）混合運算與比較。
- **進階擴充支援**：
  - **整數數學函式 (`numeric/BigIntMath.hpp`)**：`abs`, `isqrt`, `sqrt`, `icbrt`, `cbrt`, `pow`, `gcd`, `lcm`（同時支援 `numeric::` 與 `std::` 命名空間）。
  - **位元集合互轉 (`numeric/Bitset.hpp`)**：支援任意寬度 `std::bitset<N>` 雙向轉換，負數嚴格遵循標準二補數延伸。
  - **現代格式化與容器**：支援 C++20 `std::format` 及 `std::hash<numeric::bigint>`（可直接作為 `std::unordered_set` / `std::unordered_map` 的 Key）。

---

## 效能基準評測 (Benchmark Highlights)

專案內建跨語言/函式庫基準測試套件（詳見 [BENCHMARK_REPORT.md](benchmarks/results/BENCHMARK_REPORT.md)），在 Windows 11 x64 (MSVC 2022) 經多樣本中位數（7-Sample Median）穩定實測結果：

| 運算項目 (Operation) | 位數 (Bits) | CPP-BigInt (ns/op) | C++ GMP / MPIR (ns/op) | .NET 10 (ns/op) | Python 3.12 (int) (ns/op) | 加速比 (vs MPIR) | 加速比 (vs .NET) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Add** | 64 | **12.5** | 63.2 | 224.1 | 58.5 | 🚀 **5.04x** | 🚀 **17.89x** |
| **Mul** | 64 | **7.0** | 70.5 | 363.9 | 96.6 | 🚀 **10.04x** | 🚀 **51.78x** |
| **MemPressure** | 64 | **63.5** | 240.8 | 349.4 | 144.3 | 🚀 **3.79x** | 🚀 **5.50x** |
| **Add** | 128 | **12.0** | 67.4 | 217.1 | 72.3 | 🚀 **5.64x** | 🚀 **18.16x** |
| **Mul** | 128 | **41.4** | 84.1 | 336.3 | 120.1 | 🚀 **2.03x** | 🚀 **8.13x** |
| **Mul** | 1024 | **402.5** | 460.9 | 1253.1 | 769.7 | 🚀 **1.15x** | 🚀 **3.11x** |
| **Mul** | 4096 | **5638.2** | 7775.0 | 13370.8 | 13558.4 | 🚀 **1.38x** | 🚀 **2.37x** |
| **ToString_10** | 4096 | **10030.0** | 13434.0 | 47547.5 | 22903.5 | 🚀 **1.34x** | 🚀 **4.74x** |
| **ToString_10** | 65536 | **708.8 µs** | 1079.6 µs | 5124.4 µs | 2026.5 µs | 🚀 **1.52x** | 🚀 **7.23x** |
| **FromString_10** | 65536 | **329.0 µs** | 622.3 µs | 958.2 µs | 1066.7 µs | 🚀 **1.89x** | 🚀 **2.91x** |
| **Mul** | 65536 | **272.3 µs** | 420.8 µs | 1228.0 µs | 1127.8 µs | 🚀 **1.55x** | 🚀 **4.51x** |

> **核心優勢**：在密碼學最常見的 64 ~ 256 位元區間，`CPP-BigInt` 憑藉 256-bit SBO 達成 0 次堆積記憶體配置，四則運算全面領先 MPIR (2x ~ 10x) 與 .NET 10 (8x ~ 51x)；而在 65,536 超大規模位元下，分治十進位轉換與 Karatsuba 亦全面超越 MPIR。

---

## 快速開始

### 引入方式

#### 1. CMake 目錄引入 (Header-only)
```cmake
add_subdirectory(CPP-BigInt)
target_link_libraries(your_target PRIVATE bigint)
```

#### 2. 單一標頭檔引入 (Single Header)
直接將 `dist/bigint.hpp` 複製至專案中引入即可：
```cpp
#include "dist/bigint.hpp"
```

#### 3. C++20 模組引入 (C++20 Module)
```cpp
import bigint;
```

---

### 範例程式碼

```cpp
#include <numeric/BigInt.hpp>
#include <numeric/BigIntMath.hpp>
#include <numeric/Bitset.hpp>
#include <iostream>

int main() {
    // 1. SBO 支援（256-bit / 4 limbs 內 0 次 Heap 配置）
    numeric::bigint a = 42;
    numeric::bigint b("123456789012345678901234567890");

    // 2. 混合四則運算
    numeric::bigint c = a * b + 1000;
    std::cout << "c = " << c << std::endl;

    // 3. 常數代理
    numeric::bigint zero = numeric::bigint::zero;
    numeric::bigint one  = numeric::bigint::one();

    // 4. 數學函式
    numeric::bigint root = numeric::isqrt(numeric::bigint("100000000000000000000")); // 10^10
    numeric::bigint g = numeric::gcd(numeric::bigint(48), numeric::bigint(18));     // 6
    std::cout << "isqrt = " << root << ", gcd = " << g << std::endl;

    // 5. bitset 轉換
    numeric::bigint neg(-42);
    std::bitset<16> bs16 = numeric::to_bitset<16>(neg); // 二補數
    std::cout << "-42 in 16-bit: " << bs16 << std::endl;

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
| `[[nodiscard]]` 屬性檢查 | 模擬 | 模擬 | 原生 | 原生 | 原生 |
| 字串視圖 `string_view` | 內建相容 | 內建相容 | `std::string_view` | `std::string_view` | `std::string_view` |
| `constexpr` 編譯期求值 | ✕ | ✕ | ✕ | ✔ | ✔ |
| `std::format` 格式化輸出 | ✕ | ✕ | ✕ | ✔ (若編譯器支援) | ✔ |

---

## API 文件

API 技術參考手冊已收錄於 [docs/](docs/README.md)：

- [API 總覽與目錄導覽 (Table of Contents)](docs/README.md)
- [numeric::bigint 類別手冊](docs/bigint/index.md)
  - [建構函式全覽](docs/bigint/constructors.md)
  - [屬性與狀態檢測](docs/bigint/properties.md)
  - [型別轉換與字串化](docs/bigint/conversions.md)
  - [運算子重載](docs/bigint/operators.md)
  - [靜態解析與常數代理](docs/bigint/parsing.md)
- [高精度數論與數學函式 (`numeric/BigIntMath.hpp`)](docs/math.md)
- [std::bitset 泛型互轉 (`numeric/Bitset.hpp`)](docs/bitset.md)
- [C++ 標準庫擴充特化 (std::hash, std::formatter)](docs/extensions.md)
- [編譯設定與巨集環境 (`numeric/Config.hpp`)](docs/config.md)
- [跨語言效能評測報告 (Benchmark Report)](benchmarks/results/BENCHMARK_REPORT.md)

---

## 授權條款

本專案採用 MIT 授權條款。詳見 [LICENSE](LICENSE)。
