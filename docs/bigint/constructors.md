# bigint 建構函式 (Constructors)

介紹 `numeric::bigint` 類別之所有建構函式與賦值運算子，包含原生整數多載、泛型整數模板、字串解析與泛型 `std::bitset<N>` 建構。

## 命名空間與標頭檔
- **命名空間**: `numeric`
- **標頭檔**: `<numeric/BigInt.hpp>`

---

## 建構函式多載清單 (Overload List)

| 建構函式宣告 | 說明 |
| :--- | :--- |
| `BasicBigInt() noexcept;` | 預設建構子：初始化為 `0`，透過 `bind_inline()` 繫結 `m_inline_storage`。 |
| `BasicBigInt(const BasicBigInt& other);`<br>`template <std::size_t OtherLimbs> BasicBigInt(const BasicBigInt<OtherLimbs>& other);` | 複製建構子：支援同容量與跨 SBO 容量深層複製。 |
| `BasicBigInt(BasicBigInt&& other) noexcept;`<br>`template <std::size_t OtherLimbs> BasicBigInt(BasicBigInt<OtherLimbs>&& other) noexcept;` | 移動建構子：支援同容量與跨 SBO 容量資源轉移。 |
| `explicit BasicBigInt(bool b) noexcept;` | 自布林值 **顯式 (`explicit`)** 建構：`true` 為 `1`，`false` 為 `0`（防止隱式 `bool` 轉 `bigint`）。 |
| `template <std::signed_integral T> requires (!std::same_as<T, bool>) BasicBigInt(T v) noexcept;` | **C++23 Concepts 有符號整數建構子**：接受所有有符號整數（`int8_t` ~ `int64_t`, `long`, `signed char` 等）。 |
| `template <std::unsigned_integral T> requires (!std::same_as<T, bool>) BasicBigInt(T v) noexcept;` | **C++23 Concepts 無符號整數建構子**：接受所有無符號整數（`uint8_t` ~ `uint64_t`, `unsigned long`, `std::size_t`, 字元整數等）。 |
| `explicit BasicBigInt(numeric::string_view sv);` | 自十進位 `std::string_view` 顯式解析建構。 |
| `explicit BasicBigInt(const char* s);` | 自 C-style 空結尾字串顯式解析建構（傳入 `nullptr` 立即拋出例外）。 |
| `explicit BasicBigInt(const std::string& s);` | 自 `std::string` 顯式解析建構。 |
| `template <std::size_t N> explicit BasicBigInt(const std::bitset<N>& bs);` | **顯式泛型 bitset 建構子**：自指定寬度之 `std::bitset<N>` 按無符號二進位數解析建構。 |
| `BasicBigInt(detail::BigIntConstantProxy proxy) noexcept;` | 自常數代理（`bigint::zero` / `bigint::one`）建構。 |
| `explicit BasicBigInt(detail::BigIntStorage storage) noexcept;` | 自底層儲存結構顯式移動接管資源建構。 |

---

## 詳細說明

### 1. 預設建構子 `BasicBigInt()`

#### 語法
```cpp
NUMERIC_CONSTEXPR_20 BasicBigInt() noexcept;
```

#### 摘要
建立並初始化數值為 `0` 的 `BasicBigInt` 實例，並呼叫 `this->bind_inline(this->m_inline_storage)` 完成 SBO 繫結。所有其他建構函式均透過建構函式委派 (`: BasicBigInt{}`) 先建立合法 SBO 狀態。

#### 備註
預設建構之物件位於 256-bit SBO 模式（預設 4 limbs 空間），不觸發任何動態記憶體配置。其 `sign()` 為 `0`，`limb_count()` 為 `0`。

#### 範例
```cpp
numeric::bigint a;
std::cout << a << "\n"; // 輸出: 0
```

---

### 2. 布林與 C++23 Concepts 整數建構子

#### 語法
```cpp
explicit NUMERIC_CONSTEXPR_20 BasicBigInt(bool b) noexcept;

template <std::signed_integral T>
    requires (!std::same_as<T, bool>)
NUMERIC_CONSTEXPR_20 BasicBigInt(T v) noexcept;

template <std::unsigned_integral T>
    requires (!std::same_as<T, bool>)
NUMERIC_CONSTEXPR_20 BasicBigInt(T v) noexcept;
```

#### 型別與參數
- `b`: `bool`（必須顯式直接初始化，如 `numeric::bigint b1(true);`；禁止隱式轉換如 `numeric::bigint b1 = true;`）。
- `v`: 受 `std::signed_integral<T>` 或 `std::unsigned_integral<T>` 約束之任意原生整數（排除 `bool`，涵蓋固定寬度整數、`long`、`char`、`wchar_t` 等）。

#### 備註
- 若數值為負數，內部透過 `-(val + 1) + 1` 正確處理極值情況（例如 `INT64_MIN`，即 `-9223372036854775808LL`），避免未定義之有號算術溢位。
- 藉由 C++23 Concepts 約束，編譯期自動拒絕浮點數、`bool` 隱式轉型與不相關自訂型別，無需繁瑣的 SFINAE 重載列舉。
- 所有 64 位元純量整數建構皆保證 `noexcept` 且於預設 SBO 內完成（0 次 Heap 配置）。

#### 範例
```cpp
numeric::bigint b1(true);                        // 需使用顯式初始化
numeric::bigint b2 = -42;                        // std::signed_integral
numeric::bigint b3(18446744073709551615ULL);     // std::unsigned_integral (UINT64_MAX)
long l = 100000L;
numeric::bigint bl = l;
```

---

### 3. 字串解析建構子

#### 語法
```cpp
explicit NUMERIC_CONSTEXPR_20 bigint(numeric::string_view sv);
explicit NUMERIC_CONSTEXPR_20 bigint(const char* s);
explicit bigint(const std::string& s);
```

#### 參數
- `sv`: 十進位字串視圖（`numeric::string_view`，即 `std::string_view` 別名）。
- `s`: C-style 字串指標或 `std::string` 物件。

#### 例外狀況
- `std::invalid_argument`:
  - 傳入空指標 `nullptr`。
  - 字串為空或只包含符號字元（如 `""`, `"+"`, `"-"`）。
  - 字串包含非十進位數字之非法字元（如 `"123a45"`）。

#### 備註
- 支援選用的前導符號字元 `'+'` 或 `'-'`。
- 支援任意數量的無效前導零（例如 `"000123"` 將正確解析為 `123`；`"-000"` 將正確正規化為 `0`）。
- 在 C++23 下，`string_view` 建構子可於 `constexpr` 編譯期常數求值。

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

### 4. 泛型 `std::bitset` 顯式建構子

#### 語法
```cpp
template <std::size_t N>
explicit BasicBigInt(const std::bitset<N>& bs);
```

#### 型別參數
- `std::size_t N`: 來源 `std::bitset` 的位元寬度（允許任意長度，如 0, 8, 16, 64, 128, 256, 1024 等）。

#### 參數
- `bs`: 傳入的 `std::bitset<N>` 物件。

#### 備註
- 標註 `explicit`，需顯式建構（或使用 [`numeric::to_bigint`](../bitset.md)）。
- 此建構子將 `std::bitset` 視為**無符號純二進位整數**進行解析，建構出的 `BasicBigInt` 恆大於等於 0。
- 內部透過 `if constexpr (N > 0U)` 與 `this->as_span()` 進行邊界安全填充；若 `N == 0` 或所有位元皆為 `0`，自動保持為零值且不執行 Heap 配置。

#### 範例
```cpp
std::bitset<8> bs8(0b10110010); // 178
numeric::bigint b(bs8);
std::cout << b << "\n"; // 輸出: 178
```

---

## 賦值運算子 (Assignment Operators)

```cpp
NUMERIC_CONSTEXPR_20 auto operator=(const BasicBigInt& other) -> BasicBigInt&;
template <std::size_t OtherLimbs>
NUMERIC_CONSTEXPR_20 auto operator=(const BasicBigInt<OtherLimbs>& other) -> BasicBigInt&;

NUMERIC_CONSTEXPR_20 auto operator=(BasicBigInt&& other) noexcept -> BasicBigInt&;
template <std::size_t OtherLimbs>
NUMERIC_CONSTEXPR_20 auto operator=(BasicBigInt<OtherLimbs>&& other) noexcept -> BasicBigInt&;

NUMERIC_CONSTEXPR_20 auto operator=(detail::BigIntConstantProxy proxy) noexcept -> BasicBigInt&;
```

#### 備註
- 支援同容量與跨 SBO 容量之複製與移動賦值，且複製賦值保證自我賦值安全。
- 移動賦值僅轉移指標或複製極少數內聯 limbs，保證 `noexcept`。

---

## 適用於
- **C++23** 嚴格標準環境（採用 `<concepts>` 約束與 `if constexpr`）。
- 純量、複製/移動與 `string_view` 建構子支援 `constexpr` 編譯期常數計算。
