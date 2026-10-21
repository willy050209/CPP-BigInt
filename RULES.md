# CPP-BigInt 開發原則與編碼規範 (RULES.md)

本規範適用於專案中所有人類開發者與 AI Worker 子代理，確保架構一致性、向下相容性與極限執行效能。

---

## 1. 核心軟體工程原則 (Core Principles)

### 1.1 職責分離 (Separation of Concerns)
- **資料結構 (Storage / Data Class)** 與 **業務邏輯 (Algorithm Logic)** 必須嚴格隔離：
  - `Storage.hpp` 僅負責 limbs 儲存、容量擴充與 SBO 生命週期。
  - 演算法邏輯置於 `Arithmetic.hpp`, `Bitwise.hpp`, `Multiplication.hpp`, `Division.hpp`。
- 演算法核心必須盡可能設計為**純指標 raw 函式**（如 `*_raw(uint64_t* out, const uint64_t* a, ...)`），方便重用與多重演算法串接。

### 1.2 註解規範 (Documentation Standards)
- **僅對類別 (Class/Struct)、公開介面與函式 (Function/Method) 使用 XML 文件註解**：
  - 必須包含 `<summary>`, `<param>`, `<returns>`, `<exception>` 等標準標籤。
- **函式內部執行邏輯一律使用一般 C++ 註解**：
  - 單行使用 `//`，區塊使用 `/* */`。
  - **嚴禁在函式內部運算邏輯中濫用 XML 標籤**。

### 1.3 架構偏好 (Architectural Preferences)
- **Fail-Fast (快速失敗)**：在函式進入點對參數進行前置校驗 (Guard Clauses)，如除零檢查、空指針檢查，遇異常立即拋出或終止，禁止帶病執行。
- **純函數 (Pure Function) 傾向**：底層原語輸入即輸出，不依賴或變更隱式全域狀態。
- **零動態配置原則 (Zero-Allocation on Hotpaths)**：熱點路徑（位元運算、遞迴乘除法）必須優先使用棧上記憶體或 `ScratchArena`，嚴禁在遞迴每輪實例化動態容器。

### 1.4 TDD 測試驅動開發 (Test-Driven Development)
- 新增或重構演算法時，嚴格遵守 **Red → Green → Refactor**：
  1. **Red**：在 `tests/` 目錄建立或擴充單元測試用例。
  2. **Green**：實作剛好能通過測試的精簡代碼。
  3. **Refactor**：重構、優化效能並消除重複。

---

## 2. C++ 相容性與巨集規範 (Compatibility Guidelines)

1. **向下相容性**：
   - 保證支援 C++11 至 C++23。
   - 命名空間不可使用 C++17 巢狀寫法 `namespace A::B { ... }`，必須寫為：
     ```cpp
     namespace numeric {
     namespace detail {
     ...
     }
     }
     ```
2. **Constexpr 規則**：
   - C++14 有效之函式使用 `NUMERIC_CONSTEXPR_14`。
   - C++20 有效之動態/指標運算使用 `NUMERIC_CONSTEXPR_20`。
   - 純常數評估使用 `std::is_constant_evaluated()`（置於 `#if (NUMERIC_CPLUSPLUS >= NUMERIC_CXX_20)` 內）。
3. **微最佳化 hotpaths**：
   - 極限關鍵路徑使用 `NUMERIC_FORCEINLINE` 或 `NUMERIC_CONSTEXPR_20_FORCEINLINE`。
   - 確保純回傳無副作用函式標註 `NUMERIC_NODISCARD`。
