# bigint 建構函式 (Constructors)

介紹 `numeric::bigint` 類別之所有建構函式與賦值運算子，包含原生整數多載、泛型整數模板、字串解析與泛型 `std::bitset<N>` 建構。

## 命名空間與標頭檔
- **命名空間**: `numeric`
- **標頭檔**: `<numeric/BigInt.hpp>`

---

## 建構函式多載清單 (Overload List)

| 建構函式宣告 | 說明 |
| :--- | :--- |
| `bigint() noexcept = default;` | 預設建構子：初始化為數值 `0`。 |
| `bigint(const bigint& other);` | 複製建構子：深層複製來源物件。 |
| `bigint(bigint&& other) noexcept;` | 移動建構子：轉移來源物件資源。 |
| `bigint(bool b) noexcept;` | 自布林值建構：`true` 為 1，`false` 為 0。 |
| `bigint(int8_t v) noexcept;`<br>`bigint(int16_t v) noexcept;`<br>`bigint(int32_t v) noexcept;`<br>`bigint(int64_t v) noexcept;` | 自特定寬度的有符號整數建構。 |
| `bigint(uint8_t v) noexcept;`<br>`bigint(uint16_t v) noexcept;`<br>`bigint(uint32_t v) noexcept;`<br>`bigint(uint64_t v) noexcept;` | 自特定寬度的無符號整數建構。 |
| `template <typename T> bigint(T v) noexcept;` | **泛型模板建構子**：自其他原生整數型別（如 `long`, `char`, `long long` 等）建構。 |
| `explicit bigint(numeric::string_view sv);` | 自十進位字串視圖解析建構。 |
| `explicit bigint(const char* s);` | 自 C-style 空結尾字串解析建構。 |
| `explicit bigint(const std::string& s);` | 自 `std::string` 解析建構。 |
| `template <size_t N> bigint(const std::bitset<N>& bs);` | **泛型 bitset 建構子**：自指定寬度之 `std::bitset<N>` 按無符號二進位數解析建構。 |
| `explicit bigint(detail::BigIntStorage storage) noexcept;` | 自底層儲存結構直接接管資源建構。 |

---

## 詳細說明

### 1. 預設建構子 `bigint()`

#### 語法
```cpp
bigint() noexcept = default;
```

#### 摘要
建立並初始化數值為 `0` 的 `bigint` 實例。

#### 備註
預設建構之物件位於 256-bit SBO 模式（4 limbs 空間），不觸發任何動態記憶體配置。其 `sign()` 為 `0`，`limb_count()` 為 `0`。

#### 範例
```cpp
numeric::bigint a;
std::cout << a << "\n"; // 輸出: 0
```

---

### 2. 原生整數建構子

#### 語法
```cpp
NUMERIC_CONSTEXPR_20 bigint(bool b) noexcept;
NUMERIC_CONSTEXPR_20 bigint(int8_t v) noexcept;
NUMERIC_CONSTEXPR_20 bigint(int16_t v) noexcept;
NUMERIC_CONSTEXPR_20 bigint(int32_t v) noexcept;
NUMERIC_CONSTEXPR_20 bigint(int64_t v) noexcept;
NUMERIC_CONSTEXPR_20 bigint(uint8_t v) noexcept;
NUMERIC_CONSTEXPR_20 bigint(uint16_t v) noexcept;
NUMERIC_CONSTEXPR_20 bigint(uint32_t v) noexcept;
NUMERIC_CONSTEXPR_20 bigint(uint64_t v) noexcept;
```

#### 參數
- `v`: 原生純量數值。

#### 備註
- 若數值為負數，內部將正確處理極值情況（例如 `INT64_MIN`，即 `-9223372036854775808LL`），避免未定義之算術溢位。
- 所有 64 位元純量整數建構皆完全落在 256-bit SBO 內，保證 `noexcept` 且 0 次 Heap 動態配置。

#### 範例
```cpp
numeric::bigint b1(true);
numeric::bigint b2(-42);
numeric::bigint b3(18446744073709551615ULL); // uint64_t 最大值
```

---

### 3. 泛型原生整數模板建構子

#### 語法
```cpp
template <typename T, typename std::enable_if<
    std::is_integral<T>::value &&
    !std::is_same<T, bool>::value &&
    !std::is_same<T, int8_t>::value &&
    !std::is_same<T, int16_t>::value &&
    !std::is_same<T, int32_t>::value &&
    !std::is_same<T, int64_t>::value &&
    !std::is_same<T, uint8_t>::value &&
    !std::is_same<T, uint16_t>::value &&
    !std::is_same<T, uint32_t>::value &&
    !std::is_same<T, uint64_t>::value, int>::type = 0>
NUMERIC_CONSTEXPR_20 bigint(T v) noexcept;
```

#### 型別參數
- `T`: 原生整數型別。受 `std::is_integral<T>::value` 約束，並排除了已有專屬重載之固定寬度型別。涵蓋：
  - `long`, `unsigned long`
  - `char`, `signed char`, `unsigned char`
  - `wchar_t`, `char16_t`, `char32_t`
  - 平台的擴充整數型別（若有）

#### 參數
- `v`: 型別為 `T` 的整數值。

#### 備註
透過 SFINAE 機制防止浮點數或自訂類別發生非預期的隱式轉換。

#### 範例
```cpp
long l = 100000L;
unsigned long ul = 200000UL;
char ch = 'A';

numeric::bigint bl(l);
numeric::bigint bul(ul);
numeric::bigint bch(ch);
```

---

### 4. 字串解析建構子

#### 語法
```cpp
explicit NUMERIC_CONSTEXPR_20 bigint(numeric::string_view sv);
explicit NUMERIC_CONSTEXPR_20 bigint(const char* s);
explicit bigint(const std::string& s);
```

#### 參數
- `sv`: 十進位字串視圖（支援 `numeric::string_view` 或 C++17 `std::string_view`）。
- `s`: C-style 字串指標或 `std::string` 物件。

#### 例外狀況
- `std::invalid_argument`:
  - 傳入空指標 `nullptr`。
  - 字串為空或只包含符號字元（如 `""`, `"+"`, `"-"`）。
  - 字串包含非十進位數字之非法字元（如 `"123a45"`）。

#### 備註
- 支援選用的前導符號字元 `'+'` 或 `'-'`。
- 支援任意數量的無效前導零（例如 `"000123"` 將正確解析為 `123`；`"-000"` 將正確正規化為 `0`）。
- 在 C++20 下，`string_view` 建構子可於 `constexpr` 編譯期常數求值。

#### 範例
```cpp
numeric::bigint n1("123456789012345678901234567890");
numeric::bigint n2("-987654321098765432109876543210");
numeric::bigint n3("+999");
```

#### 注意事項
> [!WARNING]
> 字串建構子預設僅解析**十進位**表示法。若欲自二進位字串解析（如包含 `"0b"` 前綴），請改用 [`bigint::from_binary_string`](parsing.md)。

---

### 5. 泛型 std::bitset 建構子

#### 語法
```cpp
template <size_t N, typename std::enable_if<(N == 0), int>::type = 0>
bigint(const std::bitset<N>&);

template <size_t N, typename std::enable_if<(N > 0), int>::type = 0>
bigint(const std::bitset<N>& bs);
```

#### 型別參數
- `size_t N`: 來源 `std::bitset` 的位元寬度（允許任意長度，如 8, 16, 64, 128, 256, 1024 等）。

#### 參數
- `bs`: 傳入的 `std::bitset<N>` 物件。

#### 備註
- 此建構子將 `std::bitset` 視為**無符號純二進位整數**進行解析，建構出的 `bigint` 恆大於等於 0。
- 若 `N == 0`，建構出的物件數值為 0。
- 若所有位元皆為 0，則自動正規化為零值並維持 SBO 狀態。

#### 範例
```cpp
std::bitset<8> bs8(0b10110010); // 178
numeric::bigint b(bs8);
std::cout << b << "\n"; // 輸出: 178
```

---

## 賦值運算子 (Assignment Operators)

```cpp
bigint& operator=(const bigint& other) = default;
bigint& operator=(bigint&& other) noexcept = default;
```

#### 備註
- 複製賦值支援自我賦值安全。
- 移動賦值釋放原先持有的動態記憶體並接收來源物件指標，保證不拋出例外。

---

## 適用於
- 所有建構子皆相容於 **C++11** 及以上版本。
- 純量與字串視圖建構子於 **C++20** 起支援 `constexpr` 編譯期常數計算。
