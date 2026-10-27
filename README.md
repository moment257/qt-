# Qt 计算器（C++ / Qt Widgets）

一个用 Qt Widgets + C++17 写的标准计算器：支持四则运算、小数、退格、清除、连续运算，
鼠标和键盘共用同一套处理逻辑，除零等边界情况都有明确处理。

![界面截图](docs/screenshot.png)

界面从上往下依次是：显示区 → 内存键行 → 三角函数行 → 4×6 键盘。
角度制与弧度制用同一行的 `DEG / RAD` 按钮切换，切到 RAD 时按钮会变成琥珀色：

![三角函数行](docs/screenshot-trig.png)

---

## 1. 功能

- **四则运算**：`+ − × ÷`，支持连续运算（`2 + 3 + 4 =`）与结果后继续运算
- **小数输入**：以 `.` 开头自动补 `0`，重复小数点被忽略，最多 16 位有效数字
- **退格 / 清除**：`⌫` 逐位退格、`CE` 只清当前操作数、`C` 全清
- **一元运算**：`1/x`、`x²`、`√x`、`%`、`±`
- **三角函数**：`sin / cos / tan`，支持角度制（DEG）与弧度制（RAD）切换，按钮颜色随模式变化
- **内存**：`MC / MR / M+ / M− / MS`，存储状态由显示区左上角的 `M` 指示
- **计算历史**：点 `M▾`（或 `Ctrl+H`）弹出历史面板，能看到之前算过的式子，点任意一条可回填结果
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
├── calculate.h/.cpp         主窗口：按键绑定、键盘事件、样式、显示刷新、历史面板调度
├── calculatorengine.h/.cpp  计算核心：状态机 + 四则运算 + 边界处理 + 历史记录（不依赖界面）
├── historypanel.h/.cpp      点 M▾ 弹出的“计算历史”浮层（只负责展示与点击，不存数据）
├── keymapping.h/.cpp        键盘键值 → 计算器按键的映射（可独立测试）
├── tests/
│   └── tst_calculatorengine.cpp   Qt Test 单元测试（45 个用例）
└── docs/
    ├── 界面搭建步骤.md       在 UI 编辑器里还原界面的详细步骤
    ├── screenshot.png        界面截图（输入中）
    ├── screenshot-result.png 界面截图（结果）
    ├── screenshot-trig.png   界面截图（三角函数，角度制）
    └── screenshot-history.png 界面截图（M 键历史面板）
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
  不用为 28 个按钮各写一段 `connect`
- 样式集中在一份 QSS，靠 objectName 选择器区分按钮类别，避免逐个按钮设样式

### 4.4 计算历史（M▾ 面板）

历史不是界面自己攒的，而是**由 `CalculatorEngine` 记，界面只负责显示**：

```
按下 = 成功  →  applyEquals()  →  appendHistory("12.5 × 4 =", "50")   // 引擎侧
点 M▾ / Ctrl+H → toggleHistoryPanel() → setContent(内存值, history())  // 界面侧
点某一行       → recordActivated(index) → recallHistory(index) → refreshDisplay()
```

这样安排的好处：

- 历史逻辑能被单元测试覆盖（`tests/` 里有 8 个历史相关用例），不需要启动界面去点；
- 界面层不用维护第二份“算过什么”的状态，不会出现和历史不同步的情况。

几条明确的语义：

| 行为 | 处理方式 |
| --- | --- |
| 记录范围 | 只有真正按下 `=` 得到结果的计算才入栈；连续运算的中间步骤不算一次“式子” |
| 保存顺序 | 引擎按时间先后保存，面板倒序展示，最新一条在最上面 |
| 条数上限 | 最多 50 条，超出后丢弃最早的，避免长时间运行无限增长 |
| 失败的计算 | 除零、溢出、负数开方直接进错误态，不写历史（没有“结果”可记） |
| 点击某条记录 | 结果显示到主显示区，原算式回到副显示区，之后可继续运算 |
| 错误态下点击 | 可以正常回填，等于用一条旧结果把错误态解除 |
| `C` / `MC` 的影响 | 历史不随 `C` 清除；只受面板里的“清除”按钮控制 |

面板本身是 `Qt::Popup` 浮层：点面板外部自动关闭、`Esc` 关闭，位置由 `positionHistoryPanel()`
按 `btnMList` 的实际坐标算出（贴住显示区底沿，宽度跟随窗口），窗口缩放不会跑偏。

### 4.5 三角函数与角度单位

三个三角函数和 `1/x`、`√x` 一样，属于“对当前操作数做一次一元运算”，所以复用了同一条路径：
取 `currentValue()` → 算 → `showValue()` → 置 `m_waitingOperand = true`。
它们因此天然支持参与表达式（`2 + 30 sin =` 得 2.5）。

真正需要额外处理的是三件事：

**1）角度换算**。C 标准库只接受弧度：

```cpp
const double radians = (m_angleMode == AngleMode::Degree) ? value * kPi / 180.0 : value;
```

`DEG / RAD` 按钮切换的是 `m_angleMode`，它和内存值一样属于“设置”，`C` 键不会重置；
切换本身也不改当前显示，切完再按 `sin` 才按新单位算。

**2）浮点噪声**。这是三角函数最容易出丑的地方，两处都要处理：

| 现象 | 不处理的后果 | 处理方式 |
| --- | --- | --- |
| `sin(30°)` = `0.4999999999999999` | 显示一长串 9 | 三角函数结果用 `kTrigPrecision = 12` 位有效数字格式化 |
| `sin(180°)` = `1.22e-16` | 显示科学计数法，看起来是错的结果 | 绝对值小于 `1e-12` 一律当作 0 |

`formatNumber()` 因此多了一个 `precision` 参数，普通运算仍是 16 位（保证 16 位整数完整显示），
只有三角函数传 12 位。

**3）定义域**。`tan(90°)`、`tan(270°)` 没有定义，直接算会得到 `1.6e16` 这种“看起来有结果”的假数值。
这里用 `|cos(radians)| < 1e-12` 判定后走 `showError("无效输入")`，和负数开方共用同一套错误态。

界面上的模式提示交给样式表：`btnAngleMode` 根据动态属性 `radian` 切换样式（DEG 深底绿字 / RAD 琥珀底深字），
切换时 `unpolish/polish` 一次让新规则生效，避免每次刷新都重设样式。

## 5. 键盘对应表

| 键盘 | 功能 | 键盘 | 功能 |
| --- | --- | --- | --- |
| `0`–`9`（含小键盘） | 数字 | `Enter` / `=` | 等号 |
| `.` / `,` | 小数点 | `Backspace` | 退格 |
| `+` `-` `*` `/` | 四则运算 | `Esc` | 清除全部（C） |
| `%` | 百分号 | `Delete` | 清除当前项（CE） |
| `F9` | ± 取反 | `r` | 1/x |
| `q` | x² | `@` | √x |
| `S` | sin | `C` | cos |
| `T` | tan | `D` | 切换 DEG / RAD |
| `Ctrl+M` | MS | `Ctrl+P` / `Ctrl+Q` | M+ / M− |
| `Ctrl+R` | MR | `Ctrl+L` | MC |
| `Ctrl+H` | 打开 / 关闭计算历史 | `Esc`（面板内） | 关闭历史面板 |

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
| 历史记录内容 | 每次 `=` 记一条算式 + 结果 | `historyRecordsCompletedCalculations` / `repeatedEqualsAddsHistory` |
| 历史条数上限 | 只保留最近 50 条 | `historyKeepsOnlyRecentRecords` |
| 点击历史回填 | 结果回到显示区，原算式回到副显示区 | `recallHistoryRecordRestoresResult` |
| 错误态点历史 | 自动恢复为正常状态 | `recallAfterErrorRecovers` |
| 失败的计算 | 不写入历史 | `failedCalculationIsNotRecorded` |
| `C` 与历史 | 历史保留，只清当前计算 | `historySurvivesClearAll` |
| `sin 30°` / `cos 60°` | 都显示 `0.5`，不出现 `0.4999999999999999` | `trigInDegreeMode` |
| `sin 180°` | 显示 `0`，不是 `1.22e-16` | `trigHidesFloatingNoise` |
| `tan 90°` | 显示“无效输入”，进入错误态 | `tangentUndefinedEntersError` |
| 角度 / 弧度切换 | 同一输入 `90`，DEG 得 1、RAD 得 0.894 | `trigInRadianMode` |
| 三角函数参与表达式 | `2 + 30 sin =` 得 2.5 | `trigParticipatesInExpression` |
| `C` 与角度单位 | 切换后的单位不被 `C` 重置 | `angleModePersistsAcrossClearAll` |

## 7. 测试

```bash
cmake --build build --target tst_calculatorengine
./build/tst_calculatorengine
```

当前结果：**45 passed, 0 failed**。

测试只依赖 `calculatorengine.cpp` / `keymapping.cpp`，不启动界面，
因此可以在 CI 里无界面运行。界面部分用手动清单验证：

1. 鼠标点 `1 2 . 5 × 4 =`，看显示是否为 `50`，副显示是否为 `12.5 × 4 =`
2. 键盘输入同样序列，结果与鼠标一致，且按键有闪动反馈
3. `8 ÷ 0 =` 显示“除数不能为零”，此时按数字无反应，按 `C` 恢复
4. `1 2 + 5 CE 3 =` 得 `15`；`1 2 3 ⌫ ⌫` 显示 `1`
5. 窗口拉宽拉窄，数字自动缩放且不越界
6. 算两道题后点 `M▾`，面板里应出现两条算式；点较早那条，显示区变成它的结果，副显示区显示当时的算式
7. `8 ÷ 0 =` 后点 `M▾` 里任一条记录，应从“除数不能为零”恢复到正常状态

## 8. 版本管理（Git / GitHub）

本仓库按“功能 → 测试 → 修复 → 界面 → 文档”的顺序提交，便于体现迭代过程：

```bash
git log --oneline
# feat: 增加三角函数 sin/cos/tan 与角度制切换
# feat: 点击 M 键查看计算历史（M▾ 面板）
# feat(ui): 按截图样式实现计算器界面与鼠标/键盘统一分发
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

- 历史记录保存在内存中，关闭程序后不保留（需要持久化可以接 `QSettings`）
- 历史面板只能整体“清除”，不支持逐条删除；最多保留最近 50 条
- 副显示区只显示“当前这一步”的算式，完整历史在 `M▾` 面板里
- 三角函数只做了 `sin / cos / tan`，没有反三角（asin/acos/atan）和双曲函数
- 三角函数结果固定按 12 位有效数字显示，极小的值（|x| < 1e-12）会被当作 0
- 不支持运算优先级（与 Windows 标准计算器一致，按输入顺序从左到右计算）
- 不处理千分位分隔符
