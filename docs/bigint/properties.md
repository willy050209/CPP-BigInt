# bigint 屬性與狀態檢測 (Properties & Status)

介紹 `numeric::bigint` 用於檢測物件記憶體狀態、正負號、有效位元寬度與底層儲存結構之成員方法。

## 命名空間與標頭檔
- **命名空間**: `numeric`
- **標頭檔**: `<numeric/BigInt.hpp>`

---

## 方法清單 (Method List)

| 方法名稱 | 語法宣告摘要 | 說明 |
| :--- | :--- | :--- |
| `is_sbo` | `auto is_sbo() const noexcept -> bool;` | 查詢目前是否處於 SBO 內建緩衝區（預設 4 limbs / 256 位元，或依 `NUMERIC_BIGINT_SBO_LIMBS` 配置；0 次 Heap 配置）。 |
| `is_small` | `auto is_small() const noexcept -> bool;` | `is_sbo()` 之別名方法。 |
| `is_zero` | `auto is_zero() const noexcept -> bool;` | 判斷數值是否等於零。 |
| `sign` | `auto sign() const noexcept -> std::int8_t;` | 取得數值正負符號代碼（`-1`, `0`, `1`）。 |
| `limb_count` | `auto limb_count() const noexcept -> std::size_t;` | 取得內部有效 64-bit limbs 陣列長度。 |
| `limbs` | `auto limbs() const noexcept -> const std::uint64_t*;` | 取得指向內部 64-bit limbs 陣列首項之唯讀指標（標註 `NUMERIC_LIFETIMEBOUND`）。 |
| `as_span` | `auto as_span() noexcept -> std::span<std::uint64_t>;`<br>`auto as_span() const noexcept -> std::span<const std::uint64_t>;` | 取得有效 limbs 區間之邊界安全 `std::span` 視圖（標註 `NUMERIC_LIFETIMEBOUND`）。 |
| `capacity_span` | `auto capacity_span() noexcept -> std::span<std::uint64_t>;` | 取得完整已配置容量區間之 `std::span` 視圖（標註 `NUMERIC_LIFETIMEBOUND`）。 |
| `inline_span` | `auto inline_span() noexcept -> std::span<std::uint64_t>;` | 取得內聯 SBO 空間區間之 `std::span` 視圖（標註 `NUMERIC_LIFETIMEBOUND`）。 |
| `storage` | `auto storage() noexcept -> BigIntBase&;`<br>`auto storage() const noexcept -> const BigIntBase&;` | 存取底層基底類別 `BigIntBase`（標註 `NUMERIC_LIFETIMEBOUND`）。 |

---

## 詳細方法說明

### 1. `is_sbo` / `is_small` 方法

查詢物件當前是否正使用類別內建的 SBO 緩衝區（即無動態記憶體分配狀態）。

#### 語法
```cpp
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 bool is_sbo() const noexcept;
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 bool is_small() const noexcept;
```

#### 傳回值
- `bool`
  - 若數值在 SBO 容量以內（有效 limbs 數量 $\le$ `NUMERIC_BIGINT_SBO_LIMBS`，預設 4 limbs / 256 位元），使用內建靜態緩衝區，回傳 `true`。
  - 若數值超出 SBO 容量，內部使用 Heap 動態記憶體分配，回傳 `false`。

#### 備註
此方法對於極致效能敏感之系統或即時系統（Real-time systems）極具價值，可用於診斷是否發生非預期的 Heap 配置。當定義了 `NUMERIC_BIGINT_SBO_LIMBS=8` 時，SBO 上限將相應擴展至 512 位元（$\le 8$ limbs）。

#### 範例
```cpp
numeric::bigint val("115792089237316195423570985008687907853269984665640564039457584007913129639935"); // 2^256 - 1
std::cout << std::boolalpha << val.is_sbo() << "\n"; // 輸出: true

val += 1; // 2^256 (需 5 個 limbs，晉升 Heap)
std::cout << val.is_sbo() << "\n";                   // 輸出: false

val -= 1; // 回縮至 256 位元以內
std::cout << val.is_sbo() << "\n";                   // 輸出: true
```

---

### 2. `is_zero` 方法

判斷該整數是否等於零。

#### 語法
```cpp
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 bool is_zero() const noexcept;
```

#### 傳回值
- `bool`
  若數值為 0，傳回 `true`；否則傳回 `false`。

#### 備註
比對 `is_zero()` 之執行代價為 $O(1)$，效率等同於直接檢查符號或內部 size，效能高於建立暫存物件進行 `val == 0` 比對。

#### 範例
```cpp
numeric::bigint a = 0;
numeric::bigint b = -100;

if (a.is_zero()) {
    std::cout << "a is zero\n";
}
```

---

### 3. `sign` 方法

取得整數的正負符號。

#### 語法
```cpp
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 int8_t sign() const noexcept;
```

#### 傳回值
- `int8_t`
  - 若數值小於 0，回傳 `-1`。
  - 若數值等於 0，回傳 `0`。
  - 若數值大於 0，回傳 `1`。

#### 範例
```cpp
numeric::bigint pos(42);
numeric::bigint neg(-42);
numeric::bigint zero(0);

std::cout << (int)pos.sign()  << "\n"; // 輸出: 1
std::cout << (int)neg.sign()  << "\n"; // 輸出: -1
std::cout << (int)zero.sign() << "\n"; // 輸出: 0
```

---

### 4. `limb_count` 方法

取得目前有效 64 位元區塊 (Limb) 的數量。

#### 語法
```cpp
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 auto limb_count() const noexcept -> std::size_t;
```

#### 傳回值
- `std::size_t`
  - 若數值為 0，回傳 `0`。
  - 其餘回傳表示該數值絕對值所需之 64-bit limbs 總數（以小端序儲存）。

#### 範例
```cpp
numeric::bigint a = 0;
std::cout << a.limb_count() << "\n"; // 輸出: 0

numeric::bigint b = 1000;
std::cout << b.limb_count() << "\n"; // 輸出: 1 (<= 64-bit)

numeric::bigint c("18446744073709551616"); // 2^64
std::cout << c.limb_count() << "\n"; // 輸出: 2
```

---

### 5. `limbs` 方法

取得內部 limbs 陣列之原生唯讀常數指標。

#### 語法
```cpp
NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto limbs() const noexcept -> const std::uint64_t*;
```

#### 傳回值
- `const std::uint64_t*`
  指向內部連續 `std::uint64_t` 儲存陣列起始位址的指標（低位 limb 在前，小端序排列）。

#### 注意事項
> [!CAUTION]
> 此方法標註 `NUMERIC_LIFETIMEBOUND`（MSVC 展開為 `[[msvc::lifetimebound]]`）。回傳之指標生命週期與該 `bigint` 物件生命週期繫結。當 `bigint` 執行任何可能擴充或縮減容量的非 const 運算（如 `+=`, `*=`）時，動態儲存區可能重新分配，原指標將失效。

#### 範例
```cpp
numeric::bigint b("18446744073709551616"); // 2^64
const std::uint64_t* ptr = b.limbs();
std::cout << "Limb 0: " << ptr[0] << "\n"; // 輸出: 0
std::cout << "Limb 1: " << ptr[1] << "\n"; // 輸出: 1
```

---

### 6. `as_span` / `capacity_span` / `inline_span` 方法

取得內部儲存區之 C++23 邊界安全 `std::span` 視圖（繼承自 `BigIntBase`）。

#### 語法
```cpp
NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto as_span() noexcept -> std::span<std::uint64_t>;
NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto as_span() const noexcept -> std::span<const std::uint64_t>;
NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto capacity_span() noexcept -> std::span<std::uint64_t>;
NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto inline_span() noexcept -> std::span<std::uint64_t>;
```

#### 傳回值與生命週期規則
- **`as_span()`**：涵蓋 `[m_data, m_data + m_size)` 之有效 limbs 檢視。推薦優先使用 `b.as_span()` 取代手動指標算術，可搭配標準演算法（如 `std::ranges::equal`）與邊界檢查。
- **`capacity_span()`**：涵蓋 `[m_data, m_data + m_capacity)` 目前所指向緩衝區（SBO 或 Heap）之總容量檢視。
- **`inline_span()`**：涵蓋 `[m_inline_data, m_inline_data + m_sbo_capacity)` 之內聯 SBO 空間檢視。
- 以上皆標註 `NUMERIC_LIFETIMEBOUND`，不可保存並引用已銷毀之暫時 `BasicBigInt` 物件。

---

### 7. `storage` 方法

存取底層儲存基底類別 `BigIntBase`。

#### 語法
```cpp
NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto storage() noexcept -> BigIntBase&;
NUMERIC_NODISCARD NUMERIC_LIFETIMEBOUND NUMERIC_CONSTEXPR_20 auto storage() const noexcept -> const BigIntBase&;
```

#### 傳回值
- `BigIntBase&` / `const BigIntBase&`
  底層儲存基底參考（標註 `NUMERIC_LIFETIMEBOUND`）。供核心演算法模組或零拷貝低階操作使用。

---

## 適用於
- **C++23** 嚴格標準環境（全面支援 `constexpr` 編譯期求值、`std::span` 與 `NUMERIC_LIFETIMEBOUND`）。
