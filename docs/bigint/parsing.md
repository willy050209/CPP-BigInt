# bigint 靜態解析方法與常數代理 (Parsing & Constants)

介紹 `numeric::bigint` 類別之靜態字串解析工廠方法（十進位與二進位）以及常數代理結構（`zero` 與 `one`）。

## 命名空間與標頭檔
- **命名空間**: `numeric`
- **標頭檔**: `<numeric/BigInt.hpp>`

---

## 靜態成員與常數總覽

| 成員名稱 | 類型 | 說明 |
| :--- | :--- | :--- |
| `from_string` | 靜態方法 | 自十進位字串視圖解析並回傳 `bigint`。 |
| `from_binary_string` | 靜態方法 | 自二進位字串視圖解析並回傳 `bigint`（支援 `0b`/`0B` 前綴與符號）。 |
| `zero` | 常數代理 | 代表數值 `0`。支援以屬性或函式呼叫方式使用：`bigint::zero` 或 `bigint::zero()`。 |
| `one` | 常數代理 | 代表數值 `1`。支援以屬性或函式呼叫方式使用：`bigint::one` 或 `bigint::one()`。 |

---

## 詳細說明

### 1. `from_string` 靜態方法

自十進位字串視圖解析並建立 `bigint` 物件。

#### 語法
```cpp
static NUMERIC_CONSTEXPR_20 bigint from_string(numeric::string_view sv);
```

#### 參數
- `sv`: `numeric::string_view`
  包含十進位整數字符之字串視圖。支援 `'+'` 或 `'-'` 前綴。

#### 傳回值
- `bigint`
  解析完成之任意精度整數實例。

#### 例外狀況
- `std::invalid_argument`
  若字串為空、僅包含符號、或包含非十進位數字字符（`'0'`~`'9'`）時拋出。

#### 備註與分治演算法架構
`from_string` 捨棄了傳統逐位數相加的 $O(N^2)$ 樸素實作，引入經嚴格基準調校之二階分治聚合架構：
1. **$10^{19}$ 純量區塊化 (Chunking)**：
   利用 64 位元整數可容納的最大十進位冪次 $10^{19} < 2^{64}-1$，以 19 位數為一組在單次前向迴圈中直接轉化為 `uint64_t` 原生整數，完全消除暫存 `bigint` 配置。
2. **中小型字串原位單肢段乘加累積 (`parse_chunks_linear`)**：
   對於小於等於 16 個 chunks（約 304 個十進位數字 / 1010 bits，涵蓋常見金鑰、雜湊值與中型數字）的輸入，直接以底層肢段陣列的原地單 limb 乘加 Horner 演算法進行累積，達成 **0 個臨時 `BigIntStorage` 節點配置**，大幅消滅小規模分治遞迴所導致的堆疊與動態配置負擔。
3. **大型字串二分樹狀折疊聚合 (Divide-and-Conquer Tree Aggregation)**：
   對於大於 16 chunks 的龐大字串，採用兩兩二分合併策略：$V_{\text{merged}} = V_{\text{high}} \times 10^{19 \cdot 2^k} + V_{\text{low}}$。結合 Karatsuba 快速乘法，將超大數之解析複雜度大幅降低。65,536-bit 解析延遲達到 **0.329 ms**，領先 C++ MPIR（0.622 ms）達 1.89x。
4. **執行緒安全動態權重快取 (`Pow10Cache`)**：
   聚合所需的巨大冪次常數（$10^{19 \cdot 2^k}$）由內建的延遲求值快取管理，同一行程中重複解析大數時可直接複用快取權重，極大化批次輸入效能。

#### 範例
```cpp
numeric::bigint val = numeric::bigint::from_string("9876543210123456789");
std::cout << val << "\n";
```

---

### 2. `from_binary_string` 靜態方法

自二進位格式字串解析任意精度整數。

#### 語法
```cpp
static bigint from_binary_string(numeric::string_view sv);
```

#### 參數
- `sv`: `numeric::string_view`
  二進位字串視圖。格式規範：
  - 選用的正負符號：`'+'` 或 `'-'`。
  - 選用的二進位前綴：`"0b"` 或 `"0B"`。
  - 二進位有效數字字符：僅允許 `'0'` 與 `'1'`。

#### 傳回值
- `bigint`
  解析完成之物件。

#### 例外狀況
- `std::invalid_argument`：
  - 字串為空或僅包含前綴/符號而無任何二進位數字。
  - 含有非 `'0'` 與 `'1'` 之非法字元。

#### 備註
此方法會自動去除有效數字前的前導零（如 `"00001010"` 解釋為 `10`；`"-0b000"` 正確解析為 `0`）。

#### 範例
```cpp
numeric::bigint b1 = numeric::bigint::from_binary_string("101010");      // 42
numeric::bigint b2 = numeric::bigint::from_binary_string("0b11111111");  // 255
numeric::bigint b3 = numeric::bigint::from_binary_string("-0b1000");     // -8

std::cout << b1 << ", " << b2 << ", " << b3 << "\n";
```

---

### 3. 常數代理 `bigint::zero` 與 `bigint::one`

透過 `detail::BigIntConstantProxy` 實現的常數機制，兼具常數屬性與函式語意，並解決跨 C++ 標準與鏈結時常數定義之衝突。

#### 語法宣告
```cpp
namespace detail {
    struct BigIntConstantProxy {
        constexpr explicit BigIntConstantProxy(int64_t v) noexcept;
        NUMERIC_CONSTEXPR_20 operator bigint() const;
        NUMERIC_CONSTEXPR_20 bigint operator()() const;
    };
}

class bigint : public detail::BigIntConstants<> {
    // 繼承自 BigIntConstants:
    // static constexpr detail::BigIntConstantProxy zero{0};
    // static constexpr detail::BigIntConstantProxy one{1};
};
```

#### 使用方式
常數代理支援兩種等價用法：
1. **直接當作常數值或比對**（透過隱式轉型）：
   ```cpp
   numeric::bigint z = numeric::bigint::zero;
   if (x == numeric::bigint::zero) { ... }
   ```
2. **作為工廠函式呼叫**（呼叫 `operator()`）：
   ```cpp
   numeric::bigint o = numeric::bigint::one();
   ```

#### 備註
代理結構已完整多載 `operator==` 與 `operator!=` 模板，因此在與任意型別比對時不產生臨時 `bigint` 物件，亦不觸發記憶體配置。

#### 範例
```cpp
numeric::bigint z1 = numeric::bigint::zero;
numeric::bigint z2 = numeric::bigint::zero();
numeric::bigint o1 = numeric::bigint::one;
numeric::bigint o2 = numeric::bigint::one();

std::cout << (z1 == 0) << ", " << (o1 == 1) << "\n"; // 輸出: 1, 1
```

---

## 適用於
- C++11 及以上版本。
- `from_string` 與常數代理在 C++20 起支援 `constexpr` 編譯期常數評估。
