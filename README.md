# Qt 计算器（C++ / Qt Widgets）

一个用 Qt Widgets + C++17 写的标准计算器：支持四则运算、小数、退格、清除、连续运算，
鼠标和键盘共用同一套处理逻辑，除零等边界情况都有明确处理。

![界面截图](docs/screenshot.png)

---

## 1. 功能

- **四则运算**：`+ − × ÷`，支持连续运算（`2 + 3 + 4 =`）与结果后继续运算
- **小数输入**：以 `.` 开头自动补 `0`，重复小数点被忽略，最多 16 位有效数字
- **退格 / 清除**：`⌫` 逐位退格、`CE` 只清当前操作数、`C` 全清
- **一元运算**：`1/x`、`x²`、`√x`、`%`、`±`
- **内存**：`MC / MR / M+ / M− / MS`，存储状态由显示区左上角的 `M` 指示
- **键盘操作**：与鼠标点击完全等价，按下时对应按钮会闪一下作为反馈
- **异常处理**：除以 0、负数开方、数值溢出统一进入错误态，只允许清除键恢复

## 2. 运行环境

| 项目 | 要求 |
| --- | --- |
| Qt | Qt 6.x（本机验证：Qt 6.11.2 MinGW 64-bit），兼容 Qt 5.15 |
| 编译器 | MinGW 13.1 / MSVC 2019+，需 C++17 |
| 构建工具 | CMake ≥ 3.16（Qt Creator 自带） |

### 用 Qt Creator 打开

1. `文件 → 打开文件或项目`，选择本目录的 `CMakeLists.txt`
2. 配置套件（如 `Desktop Qt 6.11.2 MinGW 64-bit`）
3. `Ctrl+B` 构建，`Ctrl+R` 运行

### 命令行构建

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=<Qt安装路径>/6.11.2/mingw_64
cmake --build build
./build/Calculator.exe          # Windows
./build/tst_calculatorengine    # 单元测试
```

## 3. 目录结构

```
├── main.cpp                 程序入口
├── calculate.ui             界面（Qt Designer 搭建，详见 docs/界面搭建步骤.md）
├── calculate.h/.cpp         主窗口：按键绑定、键盘事件、样式、显示刷新
├── calculatorengine.h/.cpp  计算核心：状态机 + 四则运算 + 边界处理（不依赖界面）
├── keymapping.h/.cpp        键盘键值 → 计算器按键的映射（可独立测试）
├── tests/
│   └── tst_calculatorengine.cpp   Qt Test 单元测试（30 个用例）
└── docs/
    ├── 界面搭建步骤.md       在 UI 编辑器里还原界面的详细步骤
    ├── screenshot.png        界面截图（输入中）
    └── screenshot-result.png 界面截图（结果）
```

## 4. 设计要点

### 4.1 三层结构，界面与逻辑解耦

```
calculate.ui  ←界面布局
      │
      ▼
Calculate（主窗口）   ←只做三件事：绑定按钮、转发键盘、刷新显示
      │  统一入口 pressCalculatorKey(CalculatorKey)
      ▼
CalculatorEngine（纯逻辑）  ←所有运算与边界处理，不使用任何界面类
```

- 鼠标点击：`QPushButton::clicked` → `pressCalculatorKey(key)`
- 键盘输入：`keyPressEvent` → `KeyMapping::fromKeyboard()` → `pressCalculatorKey(key)`

两条路径在 `pressCalculatorKey()` 汇合，之后完全相同，所以**鼠标和键盘永远保持一致**，
这也是需求 4 中“尽可能复用同一套处理逻辑”的落点。

### 4.2 用状态机描述输入阶段

`CalculatorEngine::State` 只有三个值，同一个按键在不同状态下的行为就自然分开了：

| 状态 | 含义 | 典型行为 |
| --- | --- | --- |
| `Inputting` | 正在输入一个操作数 | 数字追加、允许退格、允许补小数点 |
| `Result` | 刚得到结果或刚按下运算符 | 数字开启新操作数；退格被忽略；运算符继续运算 |
| `Error` | 出错（如除数为 0） | 除 `C`/`CE` 外所有按键被忽略 |

配合 `m_waitingOperand` 这个标志，区分“当前显示的是不是待输入的新操作数”，
连续运算、结果后继续输入这类场景都不需要额外的分支判断。

### 4.3 命名与去重

- 一个按键一个枚举值：`CalculatorKey::Digit0 … MemoryStore`
- 一个按键一个处理函数：`inputDigit()` / `applyOperator()` / `applyEquals()` …
- 界面按钮与逻辑按键用一张 `m_buttonBindings` 表绑定，新增按键只需加一行，
  不用为 24 个按钮各写一段 `connect`
- 样式集中在一份 QSS，靠 objectName 选择器区分按钮类别，避免逐个按钮设样式

## 5. 键盘对应表

| 键盘 | 功能 | 键盘 | 功能 |
| --- | --- | --- | --- |
| `0`–`9`（含小键盘） | 数字 | `Enter` / `=` | 等号 |
| `.` / `,` | 小数点 | `Backspace` | 退格 |
| `+` `-` `*` `/` | 四则运算 | `Esc` | 清除全部（C） |
| `%` | 百分号 | `Delete` | 清除当前项（CE） |
| `F9` | ± 取反 | `r` | 1/x |
| `q` | x² | `@` | √x |
| `Ctrl+M` | MS | `Ctrl+P` / `Ctrl+Q` | M+ / M− |
| `Ctrl+R` | MR | `Ctrl+L` | MC |

## 6. 边界情况处理

| 场景 | 行为 | 对应测试 |
| --- | --- | --- |
| 连续小数点 `3..5` | 第二个小数点被忽略，显示 `3.5` | `repeatedDecimalPointIgnored` |
| 以小数点开头 `.5` | 自动显示 `0.5` | 同上 |
| 连续运算符 `5 + × 3` | 用后按的运算符替换前一个，结果 15 | `consecutiveOperatorsReplaced` |
| 连续运算 `2 + 3 × 4` | 按顺序计算，结果 20 | `chainedOperations` |
| 除数为 0 | 显示“除数不能为零”，进入错误态 | `divideByZeroEntersErrorState` |
| 错误态乱按 | 只有 `C`/`CE` 能恢复，其余按键忽略 | `errorStateIgnoresKeysUntilCleared` |
| 结果后继续输入 | 按数字开始新计算，按运算符在结果上继续 | `typingAfterEqualsStartsNewCalculation` / `operatorAfterEqualsContinues` |
| 输入中退格 | 逐位删除，退到空显示 `0` | `backspaceWhileInputting` / `backspaceToZero` |
| 结果后退格 | 忽略，不会破坏已算出的结果 | `backspaceIgnoredAfterResult` |
| 清除后重新计算 | `C` 清空所有上下文，可立即开始新计算 | `clearAllResetsEverything` |
| `CE` 与 `C` 的区别 | `12 + 5 CE 3 =` 得 15（保留运算符） | `clearEntryKeepsPendingOperator` |
| 浮点误差 `0.1+0.2` | 显示 `0.3` 而不是 `0.30000000000000004` | `floatingPointNoiseHidden` |
| 16 位整数结果 | 完整显示，不转成科学计数法 | `largeIntegerResult` |
| 重复按 `=` | 重复上一次运算 | `repeatedEqualsRepeatsLastOperation` |
| 鼠标 / 键盘混用 | 两侧走同一入口，结果一致 | `keyboardAndMouseShareSameEngine` |

## 7. 测试

```bash
cmake --build build --target tst_calculatorengine
./build/tst_calculatorengine
```

当前结果：**30 passed, 0 failed**。

测试只依赖 `calculatorengine.cpp` / `keymapping.cpp`，不启动界面，
因此可以在 CI 里无界面运行。界面部分用手动清单验证：

1. 鼠标点 `1 2 . 5 × 4 =`，看显示是否为 `50`，副显示是否为 `12.5 × 4 =`
2. 键盘输入同样序列，结果与鼠标一致，且按键有闪动反馈
3. `8 ÷ 0 =` 显示“除数不能为零”，此时按数字无反应，按 `C` 恢复
4. `1 2 + 5 CE 3 =` 得 `15`；`1 2 3 ⌫ ⌫` 显示 `1`
5. 窗口拉宽拉窄，数字自动缩放且不越界

## 8. 版本管理（Git / GitHub）

本仓库按“功能 → 测试 → 修复 → 界面 → 文档”的顺序提交，便于体现迭代过程：

```bash
git log --oneline
# fix: 修正大整数显示为科学计数法与 CE 后副显示区残留
# feat: 实现计算核心 CalculatorEngine 与键盘映射
# chore: 初始化 Qt 计算器工程骨架
```

推送到自己的 GitHub 仓库：

```bash
git remote add origin https://github.com/<你的用户名>/qt-calculator.git
git branch -M main
git push -u origin main
```

> 仓库内的提交身份是占位配置，推送前可改成自己的：
> `git config user.name "你的名字"`、`git config user.email "你的邮箱"`。

## 9. 已知限制

- `M▾`（内存历史列表）按截图保留了按钮但未实现，置灰状态
- 表达式栏显示的是“当前这一步”的算式，不做完整历史栈
- 不支持运算优先级（与 Windows 标准计算器一致，按输入顺序从左到右计算）
- 不处理千分位分隔符
