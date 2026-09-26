# CPP-BigInt

高效、跨標準、零外部依賴的現代 C++ 任意精度整數函式庫 (Arbitrary-Precision Integer Library)。

向下相容至 **C++11**，並於 **C++20+** 全面支援編譯期常數求值 (`constexpr`)。

---

## 核心特性

- **可配置 Small Buffer Optimization (SBO)**：
  - 預設 256 位元（4 個 64-bit limbs），結構大小為 64 位元組剛好對齊單一 L1 快取行，享有 **0 次 Heap 動態記憶體配置**。
  - 可透過編譯旗標 `-DNUMERIC_BIGINT_SBO_LIMBS=8` 擴展至 512 位元（8 limbs），為 256 位元與 512 位元密碼學運算提供完全零堆疊配置的暫存器/棧上運算保障。
  - 數值擴展時自動平滑晉升至動態陣列；運算回縮至 SBO 容量以內時自動退回 SBO，維持極高快取局部性。
- **高效數值運算法與硬體原語加速**：
  - **對稱加減法 (~5 ns)**：直接編譯為硬體進位/借位指令（MSVC `_addcarry_u64` / `_subborrow_u64`，GCC/Clang `__builtin_addcll` / `__builtin_subcll`），配合 SBO 4-limb 完全無分支展開快速路徑（`add_unsigned_sbo4` / `sub_unsigned_sbo4`），於暫存器內以單週期管線執行；雙目運算子採 Direct-Result 零深拷貝與 Move-Reuse 機制。
  - **三層階梯式乘法**：
    1. 小整數（$N \le 16$ limbs / 1024 bits）：高度展開與向量化之 **Schoolbook 乘法**（當長度總和 $\le$ SBO 容量時，使用純棧上 SBO 緩衝區，0 Heap 配置，~12 ns）。
    2. 中整數（$16 < N \le 64$ limbs / 1024 ~ 4096 bits）：**Karatsuba 分治演算法** ($O(N^{1.585})$)。
    3. 大整數（$N > 64$ limbs / > 4096 bits）：**Toom-Cook 3 (Toom-3) 分治演算法** ($O(N^{1.465})$)。
    - 遞迴運算由線程局部無鎖 RAII `ScratchArena` 管理，深度內達成 0 次 Heap 動態配置。
  - **除法與取模**：**雙階派發體系**，小於 128 limbs（8,192 位元）採用改良版 Knuth Algorithm D（棧上 512-limb 工作緩衝區，0 Heap 配置）；大於等於 128 limbs 自動啟用 **Burnikel-Ziegler $D_{2n,n} / D_{3n,2n}$ 分治除法** ($O(M(N) \log N)$)，64K 位元除法加速達 **2.59x**。
- **極致字串序列化與解析 (0-Heap & 分治轉換)**：
  - **十進位格式化**：
    - 單肢極速路徑（$N = 1$）：結合 `digits10_u64` 二分搜尋常數求長度、32 位元倒數除法 chunking 與 2-Digit LUT 雙字元寫入，延遲僅 **~36.1 ns**，超越 .NET 10 Native AOT（~62.4 ns）。
    - 中小數值（$\le$ 1024 位元）：直接棧上 512-byte 逆向格式化 + 2-Digit LUT，0 初步 Heap 配置。
    - 超大數值（$> 1024$ 位元）：採用 $10^{19}$ 乘法求逆與分治冪次切分，64K-bit 序列化僅需 **0.42 ms**（領先 MPIR 2.49x、.NET 10 AOT 8.4x）。
  - **十進位解析**：採用 19-digit 區塊 Horner 累積與二分樹狀平衡折疊（`Pow10Cache` 快取），兼顧中小字串零配置與超大數極速解析。
- **豐富的運算子支援**：
  - 算術運算：`+`, `-`, `*`, `/`, `%`, `++`, `--` 及複合賦值運算子（全面支援自我別名安全性、4-overload Direct-Result 與 Move-Reuse 零拷貝）。
  - 位元運算：`&`, `|`, `^`, `~`, `<<`, `>>`（完整二補數 Two's Complement 語意，負數移位拋出 `std::invalid_argument`）。
  - 比較與邏輯：`==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `||`, `!`（支援 C 語言「非零即真」語意）。
  - 跨型別混算：無縫與所有原生整數型態（`int8_t` ~ `uint64_t`）混合運算與比較。
- **進階擴充支援**：
  - **整數數學函式 (`numeric/BigIntMath.hpp`)**：`abs`, `isqrt`, `sqrt`, `icbrt`, `cbrt`, `pow`, `gcd`, `lcm`（同時支援 `numeric::` 與 `std::` 命名空間）。
  - **位元集合互轉 (`numeric/Bitset.hpp`)**：支援任意寬度 `std::bitset<N>` 雙向轉換，負數嚴格遵循標準二補數延伸。
  - **現代格式化與容器**：支援 C++20 `std::format` 及 `std::hash<numeric::bigint>`（可直接作為 `std::unordered_set` / `std::unordered_map` 的 Key）。

---

## 效能基準評測 (Benchmark Highlights)

專案內建跨語言/函式庫分層基準測試套件（詳見 [BENCHMARK_REPORT.md](benchmarks/results/BENCHMARK_REPORT.md)），在 Windows 11 x64 (MSVC 19.51) 經 7 樣本中位數（7-Sample Median）穩定實測高階語法（Layer 2: Idiomatic Syntax）：

| 運算項目 (Operation) | 位數 (Bits) | CPP-BigInt (ns/op) | C++ GMP / MPIR (ns/op) | .NET 10 AOT (ns/op) | .NET 10 JIT (ns/op) | Python 3.13 (ns/op) | 加速比 (vs .NET AOT) | 加速比 (vs GMP) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Add** | 64 | **5.1** | 65.0 | 24.7 | 59.2 | 46.1 | 🚀 **4.88x** | 🚀 **12.85x** |
| **Sub** | 64 | **5.1** | 73.7 | 28.4 | 51.6 | 43.5 | 🚀 **5.55x** | 🚀 **14.41x** |
| **Mul** | 64 | **2.2** | 62.5 | 36.4 | 111.7 | 53.9 | 🚀 **16.28x** | 🚀 **27.97x** |
| **ToString_10** | 64 | **36.3** | 81.0 | 63.0 | 142.6 | 64.7 | 🚀 **1.73x** | 🚀 **2.23x** |
| **Add** | 128 | **5.0** | 65.9 | 37.7 | 70.1 | 68.6 | 🚀 **7.54x** | 🚀 **13.18x** |
| **Sub** | 128 | **4.7** | 67.7 | 36.7 | 87.3 | 47.3 | 🚀 **7.76x** | 🚀 **14.32x** |
| **Mul** | 128 | **24.7** | 64.2 | 31.1 | 79.8 | 67.1 | 🚀 **1.26x** | 🚀 **2.59x** |
| **Add** | 256 | **8.3** | 69.3 | 35.8 | 57.7 | 50.5 | 🚀 **4.30x** | 🚀 **8.32x** |
| **Sub** | 256 | **10.4** | 74.9 | 33.5 | 107.9 | 57.4 | 🚀 **3.23x** | 🚀 **7.22x** |
| **Mul** | 256 | **55.5** | 74.3 | 71.1 | 160.8 | 101.4 | 🚀 **1.28x** | 🚀 **1.34x** |
| **ToString_10** | 256 | **97.8** | 185.4 | 128.7 | 361.7 | 158.3 | 🚀 **1.32x** | 🚀 **1.90x** |
| **Div (BZ)** | 65536 | **121.5 µs** | 42.1 µs | 378.8 µs | 349.3 µs | 389.3 µs | 🚀 **3.12x** | 0.35x |
| **Mod (BZ)** | 65536 | **127.9 µs** | 42.8 µs | 295.6 µs | 377.3 µs | 744.7 µs | 🚀 **2.31x** | 0.33x |
| **ToString_10** | 65536 | **424.0 µs** | 180.7 µs | 3564.1 µs | 4706.8 µs | 1522.8 µs | 🚀 **8.41x** | 0.43x |
| **FromString_10** | 65536 | **246.7 µs** | 249.2 µs | 623.6 µs | 734.8 µs | 671.1 µs | 🚀 **2.53x** | 🚀 **1.01x** |
| **Mul** | 65536 | **177.4 µs** | 63.8 µs | 451.8 µs | 544.2 µs | 716.9 µs | 🚀 **2.55x** | 0.36x |

> **核心優勢**：在密碼學最常見的 64 ~ 256 位元區間，`CPP-BigInt` 憑藉可配置 SBO 與棧上無除法格式化達成 0 次堆積記憶體配置，加減法在暫存器中僅需 **~5 ns**，全面超越 .NET 10 Native AOT (4x ~ 7x) 與 GMP (8x ~ 14x)；而在 65,536 超大規模位元下，Burnikel-Ziegler 分治除法、分治十進位轉換與 Karatsuba/Toom-3 乘法展現極佳擴展性，大幅超越 .NET 10 與 Python。

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
