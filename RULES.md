# CPP-BigInt 開發原則與編碼規範 (RULES.md)

本規範適用於專案中所有人類開發者與 AI Worker 子代理，確保架構一致性、C++23 安全子集規範、零警告合規與極限執行效能。

---

## 1. 核心軟體工程原則 (Core Principles)

### 1.1 職責分離 (Separation of Concerns)
- **資料結構 (Storage / Data Class)** 與 **業務邏輯 (Algorithm Logic)** 必須嚴格隔離：
  - `Storage.hpp` 僅負責 limbs 儲存、容量擴充與 SBO 生命週期。
  - 演算法邏輯置於 `Arithmetic.hpp`, `Bitwise.hpp`, `Multiplication.hpp`, `Division.hpp`。
- 演算法核心必須以邊界安全的 `std::span<std::uint64_t>` / `std::span<const std::uint64_t>` 或受邊界檢查保護之 `*_raw` 純指標核心（搭配 `NUMERIC_RESTRICT`）設計，方便重用與向量化。

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

## 2. C++23 規範、安全性與巨集慣例 (C++23 & Safety Guidelines)

1. **C++23 基準與模組相容**：
   - 專案全面採用 **C++23** 標準（MSVC `/std:c++latest`，GCC/Clang `-std=c++23`），並保持與 C++20 模組匯出（`dist/bigint.ixx`）的單向依賴相容。
   - 公開函式一律使用尾隨回傳型別（Trailing Return Type：`auto func(...) -> ReturnType`），整數泛型約束強制使用 `<concepts>`（`std::integral`、`std::signed_integral`、`std::unsigned_integral`）。
   - 無例外可恢復失敗路徑優先回傳 `std::expected<T, BigIntError>`，衍生指標與 `std::span` 視圖必須標註 `NUMERIC_LIFETIMEBOUND`。
2. **Constexpr 與屬性規則**：
   - 編譯期常數函式使用 `NUMERIC_CONSTEXPR_20`（於 C++23 下展開為 `constexpr`）。
   - 常數評估檢測使用 `std::is_constant_evaluated()` 或 `if consteval`。
   - 純回傳值與不可忽略狀態必須標註 `NUMERIC_NODISCARD`（`[[nodiscard]]`）。
3. **微最佳化熱點與零開銷安全規則**：
   - 極限關鍵路徑使用 `NUMERIC_ALWAYS_INLINE` 或 `NUMERIC_CONSTEXPR_20_FORCEINLINE`。
   - 大型棧上暫存陣列僅於必要長度區段寫入，避免在熱路徑對整個固定緩衝區執行無條件 `{}` 全陣列零初始化；CMake `/sdl` 旗標僅啟用於 Debug 組態。
