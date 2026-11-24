# 專案任務看板 (STATE.md)

## 當前專案狀態: ✅ Phase 5: GMP 追平效能調優與官方 .NET 跨語言套件開發完成 (All C++/C#/F#/VB Tests & Benchmarks Passed)

---

### 任務清單 (Task List)

- [x] **Task 1: 底層硬體原語擴充 (Intrinsics & Multiplication)**
  - [x] 在 `Intrinsics.hpp` 實作無分支 `addmul_1_raw` 與 `submul_1_raw`（C++20 constexpr fallback）
  - [x] 在 `Multiplication.hpp` 重構 `mul_schoolbook_raw` 採用 `addmul_1_raw`
  - [x] 驗證 Schoolbook 乘法單元測試與 Kernel Benchmark (1223/1223 passed)

- [x] **Task 2: Knuth Algorithm D 原地除法分支消除 (Division Layer Part 1)**
  - [x] 將 `Division.hpp` 的 Knuth D 內層 `submul` 替換為硬體友善無分支 `submul_1_raw`
  - [x] 將回加修正步驟替換為 `add_to_raw`
  - [x] 消除暫存 stack array 溢位風險，直通輸出指標
  - [x] 驗證長除法單元測試 (1223/1223 passed)

- [x] **Task 3: Burnikel-Ziegler 零配置純指針重構 (`bz_raw`) 與 ScratchArena 指針穩定化**
  - [x] 實作純指標切片介面 `bz_div3by2_raw` 與 `bz_div2by1_raw`
  - [x] 重構 `ScratchArena` 為分塊穩定機制 (Chunked Bump Allocator with pointer stability)，徹底消除遞迴成長導致懸置指標的嚴重記憶體崩潰問題 (0xC0000005 fix)
  - [x] 整合 `ScratchArena` 預算空間，徹底消滅遞迴中的 `BigIntStorage` 堆積物件
  - [x] 驗證大數除法與邊界測試 (1223/1223 passed)

- [x] **Task 4: 超大十進位轉字串與除法協同**
  - [x] `StringConversion.hpp` 直接受益於 `bz_div2by1_raw` 與 `div_knuth_raw` 的核心加速
  - [x] 驗證字串雙向轉換測試

- [x] **Task 5: GMP 追平調優迭代與跨編譯器全版本驗證 (241/270 項追平或超越)**
  - [x] 實作 1/2-limb 標量快路徑、`1/2/4-limb` 無分支比較特化
  - [x] 實作 `NUMERIC_RESTRICT` 4x 展開就地位元運算與無分支 `shld64` / `shrd64` 位移
  - [x] 調優 `TOOM3_THRESHOLD = 64`、`BZ_THRESHOLD = 32` 並實作原地零配置除/模
  - [x] GCC 16 / Clang 23 / MSVC 19.52 (C++14/17/20/23/latest) 1,403/1,403 項全數通過

- [x] **Task 6: 官方 .NET 10 跨語言封裝函式庫 (`Numeric.BigInt`) 與 Tri-Language 驗證**
  - [x] 建置 `dotnet/src/Numeric.BigInt`（`BigInt` 不可變安全類別 + `ref struct BigIntScope` / `BigIntSbo256` 0-GC 堆疊層）
  - [x] 實作 `NativeResolver` 自動本機庫探查與 `[LibraryImport]` 零封送開銷綁定
  - [x] 完成 C# 14、F# 10、VB.NET 10 三語言測試與 `Numeric.BigInt.1.0.0.nupkg` 打包
  - [x] 完善架構、API 與 .NET 技術說明文件 (`docs/dotnet.md`, `README.md`, `ARCHITECTURE.md`)

---

### 子代理派發歷程 (Subagent Delegation Log)

| 代理 ID | 負責模組 / 任務 | 派發狀態 | 測試結果 | 驗收狀態 |
| :--- | :--- | :--- | :--- | :--- |
| `6bd0c55d-9162-4065-a9d7-39bcf435ed66` | Task 1: Intrinsics addmul_1/submul_1 & Schoolbook | Done | 1223/1223 Passed | Approved |
| `0b6211fa-21a1-47e4-8cfb-60654034d16f` | Task 2 & 3: Knuth D submul_1 & bz_raw Zero-Alloc | Done | 1223/1223 Passed | Approved |
| Main Orchestrator | ScratchArena Stable Chunks Bugfix & Benchmark Suite | Done | 1223/1223 Tests, 61/61 C-ABI, Benchmarks 100% Passed | Approved |
