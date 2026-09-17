# CPP-BigInt

高效、跨標準、零外部依賴的現代 C++ 任意精度整數函式庫 (Arbitrary-Precision Integer Library)。

向下相容至 **C++11**，並於 **C++20+** 全面支援編譯期常數求值 (`constexpr`)。

---

## 核心特性

- **128-bit Small Buffer Optimization (SBO)**：
  - 數值在 128 位元（2 個 64-bit limbs）以內時，**0 次 Heap 動態記憶體配置**。
  - 當數值超過 128 位元時自動平滑晉升至動態陣列；運算回縮至 128 位元以內時自動退回 SBO，節省記憶體並極大化快取局部性。
- **高效數值運算法**：
  - 加減法：批次 64-bit 溢位進位傳遞（MSVC `_addcarry_u64` / GCC `__builtin_addcll`）。
  - 乘法：小規模學校乘法 (Schoolbook) + 大整數 Karatsuba 分治演算法 ($O(N^{\log_2 3})$)。
  - 除法與取模：Knuth Algorithm D 規格化長除法演算法。
- **豐富的運算子支援**：
  - 算術運算：`+`, `-`, `*`, `/`, `%`, `++`, `--` 及複合賦值運算子。
  - 位元運算：`&`, `|`, `^`, `~`, `<<`, `>>`（完整二補數 Two's Complement 語意）。
  - 比較與邏輯：`==`, `!=`, `<`, `<=`, `>`, `>=`, `&&`, `||`, `!`（支援 C 語言「非零即真」語意）。
  - 跨型別混算：無縫與所有原生整數型態（`int8_t` ~ `uint64_t`）混合運算與比較。
- **進階擴充支援**：
  - **整數數學函式 (`numeric/BigIntMath.hpp`)**：`abs`, `isqrt`, `sqrt`, `icbrt`, `cbrt`, `pow`, `gcd`, `lcm`（同時支援 `numeric::` 與 `std::` 命名空間）。
  - **位元集合互轉 (`numeric/Bitset.hpp`)**：支援任意寬度 `std::bitset<N>` 雙向轉換，負數嚴格遵循標準二補數延伸。
  - **現代格式化與容器**：支援 C++20 `std::format` 及 `std::hash<numeric::bigint>`（可直接作為 `std::unordered_set` / `std::unordered_map` 的 Key）。

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
    // 1. SBO 支援（128-bit 內 0 次 Heap 配置）
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

## 授權條款

本專案採用 MIT 授權條款。詳見 [LICENSE](LICENSE)。
