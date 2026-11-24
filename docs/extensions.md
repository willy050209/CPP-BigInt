# C++ 標準庫擴充特化 (Standard Library Extensions)

介紹 `CPP-BigInt` 針對 C++ 標準函式庫提供的樣板特化（Template Specializations），包含容器雜湊特化 `std::hash` 與現代格式化輸出 `std::formatter`。

## 命名空間與標頭檔
- **命名空間**: `std`
- **標頭檔**: `<numeric/BigInt.hpp>`

---

## 特化項目總覽

| 特化類別 | 支援版本 | 說明 |
| :--- | :--- | :--- |
| `std::hash<numeric::BasicBigInt<SboLimbs>>` | C++23 | 提供雜湊函式，使任意 SBO 容量之 `BasicBigInt` 皆可直接作為無序容器（如 `std::unordered_map`）的 Key。 |
| `std::formatter<numeric::BasicBigInt<SboLimbs>, CharT>` | C++23 | 特化 `std::formatter`，支援 `std::format` 格式化輸出（依 `<version>` 探測啟用）。 |

---

## 詳細說明

### 1. `std::hash<numeric::BasicBigInt<SboLimbs>>` 特化

為 `numeric::BasicBigInt<SboLimbs>` 提供高效、防碰撞的雜湊演算法。

#### 語法
```cpp
namespace std {
    template <std::size_t SboLimbs>
    struct hash<numeric::BasicBigInt<SboLimbs>> {
        NUMERIC_NODISCARD auto operator()(const numeric::BasicBigInt<SboLimbs>& val) const noexcept -> std::size_t;
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

### 2. `std::formatter<numeric::BasicBigInt<SboLimbs>, CharT>` 特化

支援 C++23 `std::format` 格式化字串輸出。

#### 語法
```cpp
#if NUMERIC_HAS_STD_FORMAT
namespace std {
    template <std::size_t SboLimbs, typename CharT>
    struct formatter<numeric::BasicBigInt<SboLimbs>, CharT> {
        template <typename ParseContext>
        constexpr auto parse(ParseContext& ctx) -> decltype(ctx.begin());

        template <typename FormatContext>
        auto format(const numeric::BasicBigInt<SboLimbs>& val, FormatContext& ctx) const -> decltype(ctx.out());
    };
}
#endif
```

#### 格式規格支援
- 預設以十進位字串序列化數值（不接受自訂格式規範，非法格式字元將拋出 `std::format_error`）。

#### 範例 (C++23)
```cpp
#include <numeric/BigInt.hpp>
#include <iostream>

#if NUMERIC_HAS_STD_FORMAT
#include <format>

int main() {
    numeric::bigint num = 42;
    std::string s1 = std::format("Value = {}", num);
    std::cout << s1 << "\n"; // 輸出: Value = 42
}
#endif
```

#### 注意事項
> [!NOTE]
> `std::formatter` 特化由 `NUMERIC_HAS_STD_FORMAT` 巨集依據 `<version>` (`__cpp_lib_format`) 自動偵測啟用；另亦支援 `val.to_string()` 與 `operator<<`。
