# 編譯配置與巨集環境 (Configuration & Macros)

介紹 `<numeric/Config.hpp>` 中的編譯期環境檢測、跨版本相容性巨集、constexpr 支援等級以及例外處理回退機制。

## 命名空間與標頭檔
- **命名空間**: `numeric`
- **標頭檔**: `<numeric/Config.hpp>`

---

## 核心巨集總覽 (Macros Overview)

| 巨集名稱 | 預期取值 | 說明 |
| :--- | :--- | :--- |
| `NUMERIC_CPLUSPLUS` | 數值（如 `202002L`） | 正規化後的 C++ 語言標準版本代碼（相容 MSVC `_MSVC_LANG` 與 GCC/Clang `__cplusplus`）。 |
| `NUMERIC_NODISCARD` | `[[nodiscard]]` 或屬性 | 函式傳回值未被使用時觸發編譯器警告之屬性包裝。 |
| `NUMERIC_CONSTEXPR_14` | `constexpr` 或 `inline` | 於 C++14 及以上展開為 `constexpr`，低於 C++14 時回退為 `inline`。 |
| `NUMERIC_CONSTEXPR_20` | `constexpr` 或 `inline` | 於 C++20 及以上展開為 `constexpr`，低於 C++20 時回退為 `inline`。 |
| `NUMERIC_CONSTEXPR_20_FORCEINLINE` | `constexpr ...` 或 `...` | 解決 pre-C++20 下 `NUMERIC_CONSTEXPR_20` 回退為 `inline` 導致與強制內聯重複宣告之巨集。 |
| `NUMERIC_RESTRICT` | `__restrict` 或 `__restrict__` | 指標無別名（No-Alias）限定詞，指示編譯器指標無記憶體重疊以強化指令平行化。 |
| `NUMERIC_HAS_EXCEPTIONS` | `1` 或 `0` | 偵測編譯器是否啟用 C++ 例外處理機制。 |
| `NUMERIC_THROW_OR_ABORT(ex)` | 巨集陳述式 | 若啟用例外則執行 `throw (ex)`；若在無例外環境（`-fno-exceptions`）下則直接呼叫 `std::abort()`。 |
| `NUMERIC_ALWAYS_INLINE` | 編譯器專屬指示字 | 強制內聯指示（MSVC `__forceinline`、GCC/Clang `__attribute__((always_inline))`）。 |
| `NUMERIC_LIKELY(x)`<br>`NUMERIC_UNLIKELY(x)` | 分支預測提示 | 向編譯器提示分支機率（GCC/Clang `__builtin_expect`）。 |
| `NUMERIC_HAS_STD_FORMAT` | `1` 或 `0` | 偵測標準庫是否支援 C++20 `<format>` 函式庫。 |
| `NUMERIC_HAS_INT128` | `1` 或 `0` | 偵測平台是否具備原生 128 位元整數（`__int128`）。 |

---

## 詳細說明

### 1. 例外處理安全機制 (`NUMERIC_THROW_OR_ABORT`)

為了適用於禁止使用 C++ 例外的嵌入式系統、遊戲引擎或安全關鍵程式碼庫（編譯旗標如 `-fno-exceptions` 或 `/EHs-c-`），CPP-BigInt 提供了安全的例外回退機制：

```cpp
#if NUMERIC_HAS_EXCEPTIONS
#  define NUMERIC_THROW_OR_ABORT(ex) throw (ex)
#else
#  define NUMERIC_THROW_OR_ABORT(ex) std::abort()
#endif
```

#### 行為說明
- **標準模式**：當發生非法字串輸入或除以零時，拋出標準例外物件（如 `std::invalid_argument`），調用端可透過 `try ... catch` 捕獲處理。
- **無例外模式**：當編譯器關閉例外時，立即安全中止執行（呼叫 `std::abort()`），避免任何未定義行為。

---

### 2. `numeric::string_view` 輕量型抽象

在 C++17 之前，標準庫缺乏 `std::string_view`。為了兼顧無記憶體配置的高效字串視圖與 C++11 相容性，`Config.hpp` 提供了自動適配：

```cpp
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_17)
namespace numeric {
    using string_view = std::string_view;
}
#else
namespace numeric {
    class string_view { ... };
}
#endif
```

#### 備註
在 C++17 及更高版本中，`numeric::string_view` 即為 `std::string_view` 之型別別名；在 C++11/C++14 中，則自動啟用零外部依賴的自製精簡版視圖。

---

### 3. 編譯期常數計算 (`NUMERIC_CONSTEXPR_20`)

在 C++20 中，標準放寬了 `constexpr` 對動態記憶體配置（Transient Allocation）與虛擬函式等的限制。`CPP-BigInt` 的核心運算法在 C++20 下均符合 `constexpr` 要求。

#### 範例 (C++20 編譯期運算)
```cpp
#include <numeric/BigInt.hpp>

// 編譯期常數評估
constexpr numeric::bigint compute_compile_time() {
    numeric::bigint a = 12345;
    numeric::bigint b = 67890;
    return a * b + 42;
}

int main() {
    constexpr numeric::bigint result = compute_compile_time();
    static_assert(result == 838102092, "Compile-time calculation failed");
}
```

---

### 4. 內聯與編譯期巨集組合 (`NUMERIC_CONSTEXPR_20_FORCEINLINE`)

在低於 C++20 的環境中，`NUMERIC_CONSTEXPR_20` 會自動回退退化為 `inline`。若函式同時被宣告為 `NUMERIC_CONSTEXPR_20 NUMERIC_ALWAYS_INLINE`，在 GCC/Clang 下會展開為：
```cpp
inline __attribute__((always_inline)) inline // 觸發 duplicate 'inline' 語法編譯錯誤
```
為了確保在所有 C++ 標準（C++11/14/17/20/23）下皆能無縫編譯並取得最強制的內聯優化，定義了此專用組合巨集：
```cpp
#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)
#  define NUMERIC_CONSTEXPR_20_FORCEINLINE constexpr NUMERIC_ALWAYS_INLINE
#else
#  define NUMERIC_CONSTEXPR_20_FORCEINLINE NUMERIC_ALWAYS_INLINE
#endif
```

---

### 5. 指標無別名限定詞 (`NUMERIC_RESTRICT`)

在底層的多精度運算核心（如 `BigIntCore::add`、`BigIntCore::sub`、長除法與字串緩衝寫入）中，明確告知編譯器指標之間互不重疊（No-Alias），可讓編譯器實施激進的暫存器快取與指令管線排程：
```cpp
#if defined(_MSC_VER)
#  define NUMERIC_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#  define NUMERIC_RESTRICT __restrict__
#else
#  define NUMERIC_RESTRICT
#endif
```

---

## 適用於
- 所有現代 C++ 編譯器（MSVC 2015+、GCC 4.8+、Clang 3.4+）。
- 涵蓋 C++11, C++14, C++17, C++20, C++23 等各版本標準。
