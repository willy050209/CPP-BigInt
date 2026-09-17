# bigint 屬性與狀態檢測 (Properties & Status)

介紹 `numeric::bigint` 用於檢測物件記憶體狀態、正負號、有效位元寬度與底層儲存結構之成員方法。

## 命名空間與標頭檔
- **命名空間**: `numeric`
- **標頭檔**: `<numeric/BigInt.hpp>`

---

## 方法清單 (Method List)

| 方法名稱 | 語法宣告摘要 | 說明 |
| :--- | :--- | :--- |
| `is_sbo` | `bool is_sbo() const noexcept;` | 查詢目前是否處於 128-bit SBO 內建緩衝區（0 次 Heap 配置）。 |
| `is_small` | `bool is_small() const noexcept;` | `is_sbo()` 之別名方法。 |
| `is_zero` | `bool is_zero() const noexcept;` | 判斷數值是否等於零。 |
| `sign` | `int8_t sign() const noexcept;` | 取得數值正負符號代碼（`-1`, `0`, `1`）。 |
| `limb_count` | `size_t limb_count() const noexcept;` | 取得內部有效 64-bit limbs 陣列長度。 |
| `limbs` | `const uint64_t* limbs() const noexcept;` | 取得指向內部 64-bit limbs 陣列首項之唯讀指標。 |
| `storage` | `detail::BigIntStorage& storage() noexcept;`<br>`const detail::BigIntStorage& storage() const noexcept;` | 存取內部底層儲存層物件。 |

---

## 詳細方法說明

### 1. `is_sbo` / `is_small` 方法

查詢物件當前是否正使用類別內建的 128 位元緩衝區（即無動態記憶體分配狀態）。

#### 語法
```cpp
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 bool is_sbo() const noexcept;
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 bool is_small() const noexcept;
```

#### 傳回值
- `bool`
  - 若數值在 128 位元以內（有效 limbs 數量 $\le 2$），使用內建靜態緩衝區，回傳 `true`。
  - 若數值超出 128 位元，內部使用 Heap 動態記憶體分配，回傳 `false`。

#### 備註
此方法對於極致效能敏感之系統或即時系統（Real-time systems）極具價值，可用於診斷是否發生非預期的 Heap 配置。

#### 範例
```cpp
numeric::bigint val("340282366920938463463374607431768211455"); // 2^128 - 1
std::cout << std::boolalpha << val.is_sbo() << "\n"; // 輸出: true

val += 1; // 2^128 (需 3 個 limbs，晉升 Heap)
std::cout << val.is_sbo() << "\n";                   // 輸出: false

val -= 1; // 回縮至 128 位元以內
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
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 size_t limb_count() const noexcept;
```

#### 傳回值
- `size_t`
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
NUMERIC_NODISCARD NUMERIC_CONSTEXPR_20 const uint64_t* limbs() const noexcept;
```

#### 傳回值
- `const uint64_t*`
  指向內部連續 `uint64_t` 儲存陣列起始位址的指標（低位 limb 在前，小端序排列）。

#### 注意事項
> [!CAUTION]
> 回傳之指標生命週期與該 `bigint` 物件生命週期繫結。當 `bigint` 執行任何可能擴充或縮減容量的非 const 運算（如 `+=`, `*=`）時，動態儲存區可能重新分配，原指標將失效。

#### 範例
```cpp
numeric::bigint b("18446744073709551616"); // 2^64
const uint64_t* ptr = b.limbs();
std::cout << "Limb 0: " << ptr[0] << "\n"; // 輸出: 0
std::cout << "Limb 1: " << ptr[1] << "\n"; // 輸出: 1
```

---

### 6. `storage` 方法

存取內部儲存結構層 `detail::BigIntStorage`。

#### 語法
```cpp
NUMERIC_CONSTEXPR_20 detail::BigIntStorage& storage() noexcept;
NUMERIC_CONSTEXPR_20 const detail::BigIntStorage& storage() const noexcept;
```

#### 傳回值
- `BigIntStorage&` / `const BigIntStorage&`
  底層儲存層實例參考。供內部演算法或高度特化之效能調試使用。

---

## 適用於
- C++11 及以上版本。
- C++20 起支援 `constexpr` 編譯期求值。
