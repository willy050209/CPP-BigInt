# C++ 標準庫擴充特化 (Standard Library Extensions)

介紹 `CPP-BigInt` 針對 C++ 標準函式庫提供的樣板特化（Template Specializations），包含容器雜湊特化 `std::hash` 與現代格式化輸出 `std::formatter`。

## 命名空間與標頭檔
- **命名空間**: `std`
- **標頭檔**: `<numeric/BigInt.hpp>`

---

## 特化項目總覽

| 特化類別 | 支援版本 | 說明 |
| :--- | :--- | :--- |
| `std::hash<numeric::bigint>` | C++11+ | 提供雜湊函式，使 `bigint` 可直接作為無序容器（如 `std::unordered_map`）的 Key。 |
| `std::formatter<numeric::bigint, CharT>` | C++20+ | 特化 `std::formatter`，支援 C++20 `std::format` 與格式規格化輸出。 |

---

## 詳細說明

### 1. `std::hash<numeric::bigint>` 特化

為 `numeric::bigint` 提供高效、防碰撞的雜湊演算法。

#### 語法
```cpp
namespace std {
    template <>
    struct hash<numeric::bigint> {
        size_t operator()(const numeric::bigint& val) const noexcept;
    };
}
```

#### 雜湊演算法剖析
採用 **FNV-1a 結合 MurmurHash3 64-bit 雪崩混合器 (Avalanche Finalizer)**：
1. **基底混合**：以 FNV-1a 偏移常數 `14695981039346656037ULL` 起始，先後將符號 `sign` 與所有 64-bit limbs 依序吸納。
2. **雪崩混淆**：
   ```cpp
   h ^= h >> 33;
   h *= 0xff51afd7ed558ccdULL;
   h ^= h >> 33;
   h *= 0xc4ceb9fe1a85ec53ULL;
   h ^= h >> 33;
   ```
   確保任何 1 個 bit 的微小擾動均能在最終 hash 值引發 50% 機率的均勻雪崩跳變，極大化在 `std::unordered_map` 與 `std::unordered_set` 內的桶位分散性。

#### 範例
```cpp
#include <numeric/BigInt.hpp>
#include <unordered_map>
#include <unordered_set>
#include <iostream>

int main() {
    // 1. 作為 unordered_set 元素
    std::unordered_set<numeric::bigint> unique_numbers;
    unique_numbers.insert(numeric::bigint("12345678901234567890"));
    unique_numbers.insert(42);

    // 2. 作為 unordered_map 的 Key
    std::unordered_map<numeric::bigint, std::string> accounts;
    accounts[numeric::bigint("999999999999999999999999")] = "Alice";

    std::cout << accounts[numeric::bigint("999999999999999999999999")] << "\n";
}
```

---

### 2. `std::formatter<numeric::bigint>` 特化

支援 C++20 `std::format` 規格化字串輸出。

#### 語法
```cpp
#if NUMERIC_HAS_STD_FORMAT
namespace std {
    template <typename CharT>
    struct formatter<numeric::bigint, CharT> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) -> typename ParseContext::iterator;

        template <typename FormatContext>
        auto format(const numeric::bigint& val, FormatContext& ctx) const -> typename FormatContext::iterator;
    };
}
#endif
```

#### 格式規格支援
- 支援對齊與填補字元（`<`, `>`, `^`）。
- 支援最小欄位寬度（Field Width）。
- 格式化輸出十進位數字。

#### 範例 (C++20)
```cpp
#include <numeric/BigInt.hpp>
#include <iostream>

#if NUMERIC_HAS_STD_FORMAT
#include <format>

int main() {
    numeric::bigint num = 42;

    // 基本格式化
    std::string s1 = std::format("{}", num);
    std::cout << s1 << "\n"; // 輸出: 42

    // 指定寬度 10、靠右對齊、以 '*' 填補
    std::string s2 = std::format("{:*>10}", num);
    std::cout << s2 << "\n"; // 輸出: ********42

    // 指定寬度 10、置中對齊、以 '-' 填補
    std::string s3 = std::format("{:-^10}", num);
    std::cout << s3 << "\n"; // 輸出: ----42----
}
#endif
```

#### 注意事項
> [!NOTE]
> `std::formatter` 特化需在支援 C++20 `<format>` 函式庫的編譯器環境下方會啟用（由 `NUMERIC_HAS_STD_FORMAT` 巨集自動偵測）。若在 C++11~C++17 環境下，請使用 `val.to_string()` 或 `operator<<`。
