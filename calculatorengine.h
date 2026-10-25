#ifndef CALCULATORENGINE_H
#define CALCULATORENGINE_H

#include <QString>

/*!
 * \brief 计算器上可被按下的所有“键”。
 *
 * 鼠标点击按钮和键盘敲击最终都会转换成这里的同一个枚举值，
 * 再交给 CalculatorEngine::pressKey() 统一处理，因此两种输入方式
 * 共享同一套业务逻辑（需求 4）。
 *
 * 注意：Digit0 ~ Digit9 必须连续排列，KeyMapping 与 Engine 都依赖这一点。
 */
enum class CalculatorKey {
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,

    DecimalPoint,   ///< 小数点
    Add,            ///< +
    Subtract,       ///< −
    Multiply,       ///< ×
    Divide,         ///< ÷
    Equals,         ///< =
    Percent,        ///< %
    Negate,         ///< ±
    Reciprocal,     ///< 1/x
    Square,         ///< x²
    SquareRoot,     ///< √x

    Backspace,      ///< ⌫ 退格
    ClearEntry,     ///< CE 清除当前操作数
    ClearAll,       ///< C  清除全部

    MemoryClear,    ///< MC
    MemoryRecall,   ///< MR
    MemoryAdd,      ///< M+
    MemorySubtract, ///< M−
    MemoryStore     ///< MS
};

/*!
 * \brief 计算器的纯逻辑核心，不依赖任何界面类。
 *
 * 内部用一个显式状态机描述“当前处于什么输入阶段”，
 * 所有边界情况（连续小数点、连续运算符、除零、结果后继续输入等）
 * 都在这里统一处理，界面层只负责把按键转交进来并显示结果。
 */
class CalculatorEngine
{
public:
    /// 一个操作数最多允许输入的位数，避免显示溢出与 double 精度失真
    static constexpr int kMaxInputDigits = 16;

    CalculatorEngine();

    /// 唯一对外入口：按下某个键
    void pressKey(CalculatorKey key);

    /// 全部清除（C 键），注意内存值不会被清除
    void clearAll();

    /// 主显示区文本（正在输入时为原始输入，结果为格式化后的数值）
    QString displayText() const { return m_display; }

    /// 副显示区文本，用于展示已输入的运算符/算式，例如 "12 +"、"2 + 3 ="
    QString expressionText() const { return m_expression; }

    /// 是否已经存过内存值（用于界面上的 M 指示灯）
    bool hasMemory() const { return m_hasMemory; }

    /// 是否处于错误状态（如除数为 0）
    bool isError() const { return m_state == State::Error; }

private:
    /// 输入阶段，决定了同一个按键在不同时刻应该产生什么行为
    enum class State {
        Inputting, ///< 正在输入一个操作数（可以退格、可以补小数点）
        Result,    ///< 刚得到结果或刚按下运算符，下一个数字开启新操作数
        Error      ///< 出错了，只响应清除键
    };

    /// 一次二元运算的执行结果
    enum class CalcStatus {
        Ok,
        DivideByZero,
        Overflow
    };

    // ---- 按键处理：一个键一个函数，pressKey() 只做分发 ----
    void inputDigit(int digit);
    void inputDecimalPoint();
    void applyOperator(CalculatorKey op);
    void applyEquals();
    void applyUnary(CalculatorKey op);
    void applyPercent();
    void applyNegate();
    void applyBackspace();
    void applyClearEntry();
    void applyMemory(CalculatorKey op);

    // ---- 内部工具 ----
    void beginNewEntry();                                   ///< 开始输入新的操作数
    double currentValue() const;                            ///< 把显示文本解析成数值
    void showValue(double value);                           ///< 显示数值并把状态置为 Result
    void showError(const QString &message);                ///< 进入错误状态
    CalcStatus calculate(double lhs, double rhs, CalculatorKey op, double *result) const;

    static bool isDigitKey(CalculatorKey key);
    static int digitOf(CalculatorKey key);
    static bool isBinaryOperator(CalculatorKey key);
    static QString operatorSymbol(CalculatorKey key);
    static QString formatNumber(double value);
    static int countDigits(const QString &text);

    // ---- 状态数据 ----
    State m_state = State::Inputting;
    QString m_display = QStringLiteral("0");  ///< 主显示区文本
    QString m_expression;                     ///< 副显示区文本（运算符/算式）

    double m_accumulator = 0.0;               ///< 左操作数，也是连续运算的中间结果
    CalculatorKey m_pendingOperator = CalculatorKey::Equals; ///< 待执行的运算符
    bool m_hasPendingOperator = false;

    /// true 表示下一个数字/小数点将开启新的操作数（刚按下运算符或刚得到结果）
    bool m_waitingOperand = false;

    // 重复按 "=" 时复用上一次的运算
    CalculatorKey m_lastOperator = CalculatorKey::Equals;
    double m_lastOperand = 0.0;
    bool m_hasLastOperation = false;

    // 内存
    double m_memory = 0.0;
    bool m_hasMemory = false;
};

#endif // CALCULATORENGINE_H
