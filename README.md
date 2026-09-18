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
  - 乘法：單肢段快速分派 + 小規模學校乘法 (Schoolbook) + 大整數 Karatsuba 分治演算法 ($O(N^{\log_2 3})$)，內建外置刮痕緩衝區（Scratchpad）以避免遞迴分配。
  - 除法與取模：Knuth Algorithm D 規格化長除法演算法。
- **極致字串序列化與解析 (0-Heap)**：
  - 十進位格式化：針對 8,192 位元以內大整數（約 2,466 位十進位數）採用**全棧緩衝區 0-Heap 配置**，搭配 2-Digit 快速查詢表（LUT）與單次預先長度精準計算，由高位直通寫入。
  - 十進位解析：採用分治平衡樹聚合（Divide-and-Conquer）與線程安全延遲求值之 `Pow10Cache`（$10^{19 \cdot 2^k}$ 權重表），零冷啟動延遲並展現亞二次方高速解析。
- **豐富的運算子支援**：
  - 算術運算：`+`, `-`, `*`, `/`, `%`, `++`, `--` 及複合賦值運算子（全面支援自我別名安全性）。
  - 位元運算：`&`, `|`, `^`, `~`, `<<`, `>>`（完整二補數 Two's Complement 語意）。
  - 比較與邏輯：`==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `||`, `!`（支援 C 語言「非零即真」語意）。
  - 跨型別混算：無縫與所有原生整數型態（`int8_t` ~ `uint64_t`）混合運算與比較。
- **進階擴充支援**：
  - **整數數學函式 (`numeric/BigIntMath.hpp`)**：`abs`, `isqrt`, `sqrt`, `icbrt`, `cbrt`, `pow`, `gcd`, `lcm`（同時支援 `numeric::` 與 `std::` 命名空間）。
  - **位元集合互轉 (`numeric/Bitset.hpp`)**：支援任意寬度 `std::bitset<N>` 雙向轉換，負數嚴格遵循標準二補數延伸。
  - **現代格式化與容器**：支援 C++20 `std::format` 及 `std::hash<numeric::bigint>`（可直接作為 `std::unordered_set` / `std::unordered_map` 的 Key）。

---

## 效能基準評測 (Benchmark Highlights)

專案內建跨語言/函式庫基準測試套件（詳見 [BENCHMARK_REPORT.md](benchmarks/results/BENCHMARK_REPORT.md)），在 Windows 11 x64 (MSVC / MinGW) 與 Linux (WSL2 Ubuntu GCC 14.2) 實測結果：

| 運算項目 (Operation) | 位數 (Bits) | CPP-BigInt (ns/op) | C++ GMP / MPIR (ns/op) | .NET 10 (ns/op) | Python 3.13 (int) (ns/op) | 加速比 (vs GMP) | 加速比 (vs .NET) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Add** | 64 | **8.0** | 56.0 | 155.4 | 44.1 | 🚀 **7.04x** | 🚀 **19.52x** |
| **Add** (Linux) | 64 | **3.6** | 56.0 | 155.4 | 44.1 | 🚀 **15.5x** | 🚀 **43.1x** |
| **Add** | 128 | **6.7** | 55.7 | 220.5 | 46.5 | 🚀 **8.36x** | 🚀 **33.09x** |
| **Mul** | 128 | **18.9** | 126.2 | 163.4 | 67.2 | 🚀 **6.68x** | 🚀 **8.66x** |
| **MemPressure** | 128 | **22.1** | 166.5 | 356.6 | 98.5 | 🚀 **7.53x** | 🚀 **16.13x** |
| **Mul** | 1024 | **200.4** | 402.9 | 848.0 | 648.1 | 🚀 **2.01x** | 🚀 **4.23x** |
| **Mul** | 4096 | **1987.6** | 4939.8 | 6787.0 | 7642.0 | 🚀 **2.49x** | 🚀 **3.41x** |
| **ToString_10** | 4096 | **7605.0** | 8517.0 | 41159.5 | 16617.5 | 🚀 **1.12x** | 🚀 **5.41x** |
| **Mul** | 65536 | **173.6 µs** | 262.1 µs | 474.1 µs | 700.5 µs | 🚀 **1.51x** | 🚀 **2.73x** |

> **核心優勢**：在密碼學最常見的 64 ~ 256 位元區間，`CPP-BigInt` 憑藉 256-bit SBO 達成 0 次堆積記憶體配置，四則運算全面大幅領先 GMP (4x ~ 8x) 與 .NET 10 (8x ~ 33x)。

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
