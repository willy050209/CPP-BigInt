# 編譯配置與巨集環境 (Configuration & Macros)

介紹 `<numeric/Config.hpp>` 中的 **C++23** 編譯期環境檢測、屬性與生命週期綁定巨集、`constexpr` 支援以及例外處理回退機制。

## 命名空間與標頭檔
- **命名空間**: `numeric`
- **標頭檔**: `<numeric/Config.hpp>`

---

## 核心巨集總覽 (Macros Overview)

| 巨集名稱 | 預期取值 | 說明 |
| :--- | :--- | :--- |
| `NUMERIC_BIGINT_SBO_LIMBS` | 正整數（預設 `4`，可設為 `8`） | SBO 小對象最佳化容量（以 64-bit limb 為單位）。預設 4 肢（256 位元），物件大小 64 位元組剛好對齊單一 L1 快取行；設為 8 肢（512 位元）可支援 0-heap 加密運算。 |
| `NUMERIC_CPLUSPLUS` | 數值（如 `202302L`） | 正規化後的 C++ 語言標準版本代碼（相容 MSVC `_MSVC_LANG` 與 GCC/Clang `__cplusplus`）。 |
| `NUMERIC_NODISCARD` | `[[nodiscard]]` | C++23 下強制展開為 `[[nodiscard]]`，防止忽略純運算或檢查狀態回傳值。 |
| `NUMERIC_CONSTEXPR_14` | `constexpr` | C++23 下直接展開為 `constexpr`。 |
| `NUMERIC_CONSTEXPR_20` | `constexpr` | C++23 下直接展開為 `constexpr`。 |
| `NUMERIC_INLINE_VAR` | `inline` | C++17+ 行內變數指示字（供標頭檔常數使用）。 |
| `NUMERIC_LIFETIMEBOUND` | `[[msvc::lifetimebound]]` 或空 | 在 MSVC（非 Clang）下展開為 `[[msvc::lifetimebound]]`，標註衍生自 `*this` 之指標或 `std::span` 視圖，防止懸空參考；其餘編譯器展開為空。 |
| `NUMERIC_CONSTEXPR_20_FORCEINLINE` | `constexpr ...` | 將 `constexpr` 與編譯器強制內聯屬性安全結合的熱路徑巨集。 |
| `NUMERIC_RESTRICT` | `__restrict` 或 `__restrict__` | 指標無別名（No-Alias）限定詞，指示編譯器指標無記憶體重疊以強化指令平行化。 |
| `NUMERIC_HAS_EXCEPTIONS` | `1` 或 `0` | 偵測編譯器是否啟用 C++ 例外處理機制。 |
| `NUMERIC_THROW_OR_ABORT(ex)` | 巨集陳述式 | 若啟用例外則執行 `throw (ex)`；若在無例外環境（`-fno-exceptions` / `/EHs-c-`）下則直接呼叫 `std::abort()`。 |
| `NUMERIC_ALWAYS_INLINE` | 編譯器專屬指示字 | 強制內聯指示（MSVC `__forceinline`、GCC/Clang `__attribute__((always_inline)) inline`）。 |
| `NUMERIC_LIKELY(x)`<br>`NUMERIC_UNLIKELY(x)` | 分支預測提示 | 向編譯器提示分支機率（GCC/Clang `__builtin_expect`）。 |
| `NUMERIC_HAS_STD_FORMAT` | `1` 或 `0` | 透過 `<version>`（`__cpp_lib_format`）偵測標準庫是否支援 `<format>`。 |
| `NUMERIC_HAS_INT128` | `1` 或 `0` | 偵測平台是否具備原生 128 位元整數（`__int128`）。 |

---

## 詳細說明

### 1. 例外處理安全機制 (`NUMERIC_THROW_OR_ABORT`) & `NUMERIC_LIFETIMEBOUND`

為了適用於禁止使用 C++ 例外的嵌入式系統、遊戲引擎或安全關鍵程式碼庫（編譯旗標如 `-fno-exceptions` 或 `/EHs-c-`），CPP-BigInt 提供了安全的例外回退機制（若需完全非拋出的錯誤恢復，亦可直接呼叫回傳 `std::expected<T, BigIntError>` 的 `try_*` API）：

```cpp
#if defined(_MSC_VER) && !defined(__clang__)
#  define NUMERIC_LIFETIMEBOUND [[msvc::lifetimebound]]
#else
#  define NUMERIC_LIFETIMEBOUND
#endif

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || (defined(_MSC_VER) && defined(_CPPUNWIND))
#  define NUMERIC_HAS_EXCEPTIONS 1
#  define NUMERIC_THROW_OR_ABORT(ex) throw (ex)
#else
#  define NUMERIC_HAS_EXCEPTIONS 0
#  define NUMERIC_THROW_OR_ABORT(ex) std::abort()
#endif
```

#### 行為說明
- **標準模式**：當發生非法字串輸入或除以零時，拋出標準例外物件（如 `std::invalid_argument`），調用端可透過 `try ... catch` 捕獲處理。
- **無例外模式**：當編譯器關閉例外時，立即安全中止執行（呼叫 `std::abort()`），避免任何未定義行為。
- **生命週期綁定標註**：`BigIntBase::limbs()`、`as_span()`、`capacity_span()`、`inline_span()` 與 `BasicBigInt::storage()` 等視圖與存取函式均標註 `NUMERIC_LIFETIMEBOUND`，協助 MSVC 靜態分析攔截綁定至暫時 `BasicBigInt` 的懸空參考。

---

### 2. `numeric::string_view` 標準別名

在 C++23 基準下，`numeric::string_view` 直接對齊標準庫零拷貝視圖 `std::string_view`：

```cpp
namespace numeric {
    using string_view = std::string_view;
}
```

#### 備註
所有接收字串檢視的介面（如 `from_string`、`try_from_string`、`from_binary_string`、`try_from_binary_string`）皆透過 `numeric::string_view`（即 `std::string_view`）達成零堆積複製。

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

將 `constexpr` 與各編譯器的強制內聯關鍵字統一結合於熱路徑原語中：
```cpp
#if defined(_MSC_VER)
#  define NUMERIC_ALWAYS_INLINE __forceinline
#  define NUMERIC_CONSTEXPR_20_FORCEINLINE constexpr __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#  define NUMERIC_ALWAYS_INLINE __attribute__((always_inline)) inline
#  define NUMERIC_CONSTEXPR_20_FORCEINLINE constexpr __attribute__((always_inline)) inline
#else
#  define NUMERIC_ALWAYS_INLINE inline
#  define NUMERIC_CONSTEXPR_20_FORCEINLINE constexpr inline
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

### 6. SBO 小對象最佳化容量配置與樣板化 (`BasicBigInt<SboLimbs>`)

CPP-BigInt 採用了借鑒 **LLVM SmallVector** 的雙層儲存與演算法解耦架構：
- **底層核心**：`numeric::BigIntBase`（32 bytes 非樣板中介層），封裝指標、大小、容量、符號與 `std::unique_ptr` RAII 堆積管理，並透過 `bind_inline(std::span<std::uint64_t>)` 繫結衍生類別內聯空間。
- **最上層**：`numeric::BasicBigInt<std::size_t SboLimbs>`，透過 `std::array<std::uint64_t, ACTUAL_SBO>` 宣告靜態內聯陣列，並在預設建構階段將該 `std::span` 綁定給基底類別（當 `SboLimbs == 0` 時，內部保留 `ACTUAL_SBO = 1` 並保持 `m_sbo_capacity = 0`）。

```cpp
template <std::size_t SboLimbs = NUMERIC_BIGINT_SBO_LIMBS>
class BasicBigInt;

using BigInt     = BasicBigInt<NUMERIC_BIGINT_SBO_LIMBS>; // 預設 4 limbs (256-bit, 64B)
using bigint     = BigInt;                                // 向後相容別名
using BigInt256  = BasicBigInt<4>;                        // 256-bit SBO (64B)
using bigint256  = BigInt256;
using BigInt512  = BasicBigInt<8>;                        // 512-bit SBO (96B, 0-Heap Crypto)
using bigint512  = BigInt512;
using BigInt1024 = BasicBigInt<16>;                       // 1024-bit SBO (160B)
using bigint1024 = BigInt1024;
```

#### 數值與架構對照

| 型別別名 | 內聯肢數 | 內聯無號位元數 | `sizeof(...)` | 適用情境與設計權衡 |
| :---: | :---: | :---: | :---: | :--- |
| **`BigInt` / `bigint`** *(預設)* | 4 limbs (64-bit) | 256 bits | **64 bytes** | **極致快取友好**：整體結構體尺寸精確契合現代 CPU 單一 L1 快取行（Cache Line, 64 bytes）。涵蓋絕大多數常規高精度運算，並提供最高的記憶體密度。 |
| **`BigInt512` / `bigint512`** | 8 limbs (64-bit) | 512 bits | **96 bytes** | **密碼學專用 (Zero-Heap Crypto)**：為 256/512 位元橢圓曲線密碼（如 secp256k1、Curve25519）、SHA-512、Ed448 及 RSA 中間值提供完全零記憶體配置的純暫存器/棧上運算保障。 |
| **`BigInt1024` / `bigint1024`** | 16 limbs (64-bit) | 1024 bits | **160 bytes** | **大數專用**：為 RSA-1024 或超高精度運算提供棧上零配置運算。 |

#### 異質運算與右值取負
- **異質雙目運算**：不同 SBO 容量的型別可直接混算（例如 `BasicBigInt<4> + BasicBigInt<8>`），回傳型別嚴格遵循 LHS（左運算元）型別。
- **右值單元取負**：`operator-() &&` 原地反轉符號並沿用暫存緩衝區，杜絕任何不必要的動態記憶體配置。

#### 全域預設配置方式
若需變更全域預設之 `BigInt` / `bigint` 預設 Limb 數，可調整巨集 `NUMERIC_BIGINT_SBO_LIMBS`：
- **CMake 全域配置**：
  ```cmake
  target_compile_definitions(my_project PRIVATE NUMERIC_BIGINT_SBO_LIMBS=8)
  ```
- **原始碼預定義**：
  ```cpp
  #define NUMERIC_BIGINT_SBO_LIMBS 8
  #include <numeric/BigInt.hpp>
  ```

---

## 適用於
- **C++23** 嚴格標準環境（CMake `CMAKE_CXX_STANDARD 23`）。
- 支援 MSVC（`/std:c++latest` / Visual Studio 2022 17.6+ / VS 2026）、GCC（13+ `-std=c++23`）與 Clang（16+ `-std=c++23`）。
- CMake 預設對 MSVC Debug 組態開啟 `/sdl`，於 Release 組態維持高效能原生最佳化。
