#include "calculatorengine.h"

#include <QLatin1Char>
#include <QtGlobal>

#include <cmath>

// Digit0 ~ Digit9 必须连续，键盘映射依赖这个偏移量
static_assert(static_cast<int>(CalculatorKey::Digit9) - static_cast<int>(CalculatorKey::Digit0) == 9,
              "CalculatorKey::Digit0 ~ Digit9 必须连续排列");

CalculatorEngine::CalculatorEngine()
{
    m_memory = 0.0;
    m_hasMemory = false;
    clearAll();
}

// ---------------------------------------------------------------------------
// 对外接口
// ---------------------------------------------------------------------------

void CalculatorEngine::pressKey(CalculatorKey key)
{
    // 错误状态是一个“死状态”：除了清除，任何按键都不改变显示。
    // 这样可以避免用户在“除以 0”之后继续按数字，得到看起来正常但其实错误的结果。
    if (m_state == State::Error) {
        if (key == CalculatorKey::ClearAll || key == CalculatorKey::ClearEntry)
            clearAll();
        return;
    }

    // 数字键统一走 inputDigit()，避免在 switch 里写 10 个分支
    if (isDigitKey(key)) {
        inputDigit(digitOf(key));
        return;
    }

    switch (key) {
    case CalculatorKey::DecimalPoint:
        inputDecimalPoint();
        break;
    case CalculatorKey::Add:
    case CalculatorKey::Subtract:
    case CalculatorKey::Multiply:
    case CalculatorKey::Divide:
        applyOperator(key);
        break;
    case CalculatorKey::Equals:
        applyEquals();
        break;
    case CalculatorKey::Percent:
        applyPercent();
        break;
    case CalculatorKey::Negate:
        applyNegate();
        break;
    case CalculatorKey::Reciprocal:
    case CalculatorKey::Square:
    case CalculatorKey::SquareRoot:
        applyUnary(key);
        break;
    case CalculatorKey::Backspace:
        applyBackspace();
        break;
    case CalculatorKey::ClearEntry:
        applyClearEntry();
        break;
    case CalculatorKey::ClearAll:
        clearAll();
        break;
    case CalculatorKey::MemoryClear:
    case CalculatorKey::MemoryRecall:
    case CalculatorKey::MemoryAdd:
    case CalculatorKey::MemorySubtract:
    case CalculatorKey::MemoryStore:
        applyMemory(key);
        break;
    default:
        break;
    }
}

void CalculatorEngine::clearAll()
{
    m_state = State::Inputting;
    m_display = QStringLiteral("0");
    m_expression.clear();

    m_accumulator = 0.0;
    m_pendingOperator = CalculatorKey::Equals;
    m_hasPendingOperator = false;
    m_waitingOperand = false;

    m_lastOperator = CalculatorKey::Equals;
    m_lastOperand = 0.0;
    m_hasLastOperation = false;

    // 内存（MC/M+/M−/MS）与计算历史都不随 C 键清除，这与常见计算器的习惯一致
}

void CalculatorEngine::clearHistory()
{
    m_history.clear();
}

void CalculatorEngine::recallHistory(int index)
{
    if (index < 0 || index >= m_history.size())
        return;

    const CalculationRecord &record = m_history.at(index);

    // 回填结果，并把原来的算式放回副显示区，用户能看出这个数是怎么来的
    m_display = record.result;
    m_expression = record.expression;
    m_state = State::Result;

    // 回填后不沿用旧运算上下文：接着按数字是全新计算，按运算符则在该结果上继续
    m_hasPendingOperator = false;
    m_pendingOperator = CalculatorKey::Equals;
    m_hasLastOperation = false;
    m_waitingOperand = true;
}

// ---------------------------------------------------------------------------
// 数字与小数点
// ---------------------------------------------------------------------------

void CalculatorEngine::inputDigit(int digit)
{
    if (digit < 0 || digit > 9)
        return;

    if (m_waitingOperand || m_state == State::Result) {
        // 上一步是“=”或某个运算符：当前数字要开启一个新的操作数
        if (!m_hasPendingOperator) {
            m_expression.clear();       // 上一轮计算已经结束，开始一次全新的计算
            m_hasLastOperation = false; // 新数字不再参与“重复等号”
        }
        beginNewEntry();
    }

    // 限制有效数字位数：既避免显示溢出，也避免超过 double 的精确表示范围
    if (countDigits(m_display) >= kMaxInputDigits)
        return;

    const QChar digitChar = QLatin1Char(static_cast<char>('0' + digit));
    if (m_display == QStringLiteral("0")) {
        m_display = QString(digitChar);                 // 前导 0 被新数字替换
    } else if (m_display == QStringLiteral("-0")) {
        m_display = QStringLiteral("-") + digitChar;    // 保留取反后的负号
    } else {
        m_display += digitChar;
    }

    m_state = State::Inputting;
}

void CalculatorEngine::inputDecimalPoint()
{
    if (m_waitingOperand || m_state == State::Result) {
        if (!m_hasPendingOperator) {
            m_expression.clear();
            m_hasLastOperation = false;
        }
        beginNewEntry();    // 先回到 "0"，再补小数点，显示为 "0."
    }

    // 一个操作数里只允许出现一个小数点：连续按小数点时后面的直接忽略（需求 3）
    if (!m_display.contains(QLatin1Char('.')))
        m_display += QLatin1Char('.');

    m_state = State::Inputting;
}

// ---------------------------------------------------------------------------
// 四则运算
// ---------------------------------------------------------------------------

void CalculatorEngine::applyOperator(CalculatorKey op)
{
    if (!isBinaryOperator(op))
        return;

    if (m_hasPendingOperator && m_waitingOperand) {
        // 连续按运算符，例如 "5 + ×"：右操作数还没输入，直接替换掉待执行的运算符
        m_pendingOperator = op;
        m_expression = formatNumber(m_accumulator) + QLatin1Char(' ') + operatorSymbol(op);
        return;
    }

    if (m_hasPendingOperator) {
        // 连续运算，例如 "2 + 3 +"：先把前一个运算符算掉，结果作为新的左操作数
        double result = 0.0;
        switch (calculate(m_accumulator, currentValue(), m_pendingOperator, &result)) {
        case CalcStatus::Ok:
            break;
        case CalcStatus::DivideByZero:
            showError(QStringLiteral("除数不能为零"));
            return;
        case CalcStatus::Overflow:
            showError(QStringLiteral("数值超出范围"));
            return;
        }
        m_accumulator = result;
        showValue(result);
    } else {
        // 第一次按运算符（或结果之后继续运算）：当前显示值就是左操作数
        m_accumulator = currentValue();
        m_state = State::Result;
    }

    m_pendingOperator = op;
    m_hasPendingOperator = true;
    m_waitingOperand = true;    // 下一个数字开始新的操作数
    m_expression = formatNumber(m_accumulator) + QLatin1Char(' ') + operatorSymbol(op);
}

void CalculatorEngine::applyEquals()
{
    if (m_hasPendingOperator) {
        const double lhs = m_accumulator;
        const double rhs = currentValue();
        double result = 0.0;
        switch (calculate(lhs, rhs, m_pendingOperator, &result)) {
        case CalcStatus::Ok:
            break;
        case CalcStatus::DivideByZero:
            showError(QStringLiteral("除数不能为零"));
            return;
        case CalcStatus::Overflow:
            showError(QStringLiteral("数值超出范围"));
            return;
        }

        m_expression = formatNumber(lhs) + QLatin1Char(' ') + operatorSymbol(m_pendingOperator)
                       + QLatin1Char(' ') + formatNumber(rhs) + QLatin1String(" =");

        m_lastOperator = m_pendingOperator;
        m_lastOperand = rhs;
        m_hasLastOperation = true;

        m_pendingOperator = CalculatorKey::Equals;
        m_hasPendingOperator = false;

        appendHistory(m_expression, formatNumber(result));

        showValue(result);          // 状态置为 Result
        m_waitingOperand = true;    // 之后再按数字，将开始一次新的计算
        return;
    }

    if (m_hasLastOperation) {
        // 没有待执行运算却再次按 "="：重复执行上一次的运算
        const double lhs = currentValue();
        double result = 0.0;
        switch (calculate(lhs, m_lastOperand, m_lastOperator, &result)) {
        case CalcStatus::Ok:
            break;
        case CalcStatus::DivideByZero:
            showError(QStringLiteral("除数不能为零"));
            return;
        case CalcStatus::Overflow:
            showError(QStringLiteral("数值超出范围"));
            return;
        }

        m_expression = formatNumber(lhs) + QLatin1Char(' ') + operatorSymbol(m_lastOperator)
                       + QLatin1Char(' ') + formatNumber(m_lastOperand) + QLatin1String(" =");
        appendHistory(m_expression, formatNumber(result));
        showValue(result);
        m_waitingOperand = true;
        return;
    }

    // 既没有待执行运算也没有历史运算：把当前输入规整成标准数值显示
    showValue(currentValue());
    m_waitingOperand = true;
}

// ---------------------------------------------------------------------------
// 一元运算与百分号
// ---------------------------------------------------------------------------

void CalculatorEngine::applyUnary(CalculatorKey op)
{
    const double value = currentValue();
    double result = 0.0;

    switch (op) {
    case CalculatorKey::Reciprocal:
        if (value == 0.0) {
            showError(QStringLiteral("除数不能为零"));
            return;
        }
        result = 1.0 / value;
        break;
    case CalculatorKey::Square:
        result = value * value;
        break;
    case CalculatorKey::SquareRoot:
        if (value < 0.0) {
            showError(QStringLiteral("无效输入"));
            return;
        }
        result = std::sqrt(value);
        break;
    default:
        return;
    }

    if (!std::isfinite(result)) {
        showError(QStringLiteral("数值超出范围"));
        return;
    }

    showValue(result);
    m_waitingOperand = true;    // 一元运算结束后，再输入数字应开始新操作数
}

void CalculatorEngine::applyPercent()
{
    double value = currentValue();

    // 加减场景下，百分号表示“左操作数的百分比”，例如 200 + 10 % 得到 200 + 20；
    // 乘除场景下就是单纯的数值除以 100。与常见计算器行为一致。
    if (m_hasPendingOperator
        && (m_pendingOperator == CalculatorKey::Add || m_pendingOperator == CalculatorKey::Subtract)) {
        value = m_accumulator * value / 100.0;
    } else {
        value /= 100.0;
    }

    showValue(value);
    m_waitingOperand = true;
}

void CalculatorEngine::applyNegate()
{
    if (m_display.startsWith(QLatin1Char('-')))
        m_display.remove(0, 1);                 // 负负得正
    else if (currentValue() != 0.0)
        m_display.prepend(QLatin1Char('-'));    // 0 不显示成 -0

    if (m_state == State::Result)
        m_waitingOperand = true;                // 取反结果继续参与运算，按数字则重新输入
    else
        m_state = State::Inputting;
}

// ---------------------------------------------------------------------------
// 退格与清除
// ---------------------------------------------------------------------------

void CalculatorEngine::applyBackspace()
{
    // 只有“输入中”才允许退格：结果/错误状态退格没有意义，
    // 直接忽略可以避免把已经算好的结果改坏（需求 6）
    if (m_state != State::Inputting)
        return;

    m_display.chop(1);

    // 退到空（或只剩负号）时回到 0，避免主显示区出现空白
    if (m_display.isEmpty() || m_display == QStringLiteral("-"))
        m_display = QStringLiteral("0");
}

void CalculatorEngine::applyClearEntry()
{
    // CE 只清除当前正在输入的操作数，保留待执行的运算符与左操作数：
    // 例如 "12 + 5 CE 3 =" 得到 15
    beginNewEntry();

    // 副显示区只在“运算符仍然有效”时保留，避免算完之后按 CE 还残留 "12 + 3 ="
    if (!m_hasPendingOperator) {
        m_expression.clear();
        m_hasLastOperation = false;
    }
}

// ---------------------------------------------------------------------------
// 内存
// ---------------------------------------------------------------------------

void CalculatorEngine::applyMemory(CalculatorKey op)
{
    switch (op) {
    case CalculatorKey::MemoryClear:
        m_memory = 0.0;
        m_hasMemory = false;
        break;
    case CalculatorKey::MemoryStore:
        m_memory = currentValue();
        m_hasMemory = true;
        break;
    case CalculatorKey::MemoryRecall:
        if (!m_hasMemory)
            return;             // 空内存时 MR 不产生任何效果
        showValue(m_memory);
        m_waitingOperand = true;
        break;
    case CalculatorKey::MemoryAdd:
        m_memory += currentValue();
        m_hasMemory = true;
        m_state = State::Result;
        m_waitingOperand = true;
        break;
    case CalculatorKey::MemorySubtract:
        m_memory -= currentValue();
        m_hasMemory = true;
        m_state = State::Result;
        m_waitingOperand = true;
        break;
    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// 内部工具
// ---------------------------------------------------------------------------

void CalculatorEngine::beginNewEntry()
{
    m_display = QStringLiteral("0");
    m_state = State::Inputting;
    m_waitingOperand = false;
}

double CalculatorEngine::currentValue() const
{
    bool ok = false;
    const double value = m_display.toDouble(&ok);
    return ok ? value : 0.0;    // "0." 之类的未完成输入也能被正确解析
}

void CalculatorEngine::showValue(double value)
{
    m_display = formatNumber(value);
    m_state = State::Result;
}

void CalculatorEngine::showError(const QString &message)
{
    m_display = message;
    m_state = State::Error;

    // 错误后不允许再沿用之前的运算上下文
    m_hasPendingOperator = false;
    m_pendingOperator = CalculatorKey::Equals;
    m_hasLastOperation = false;
    m_waitingOperand = false;
}

void CalculatorEngine::appendHistory(const QString &expression, const QString &result)
{
    if (expression.isEmpty())
        return;

    m_history.append(CalculationRecord{expression, result});

    // 只保留最近的若干条，避免历史无限增长
    if (m_history.size() > kMaxHistoryRecords)
        m_history.remove(0, m_history.size() - kMaxHistoryRecords);
}

CalculatorEngine::CalcStatus CalculatorEngine::calculate(double lhs, double rhs, CalculatorKey op,
                                                         double *result) const
{
    if (!result)
        return CalcStatus::Overflow;

    double value = 0.0;
    switch (op) {
    case CalculatorKey::Add:
        value = lhs + rhs;
        break;
    case CalculatorKey::Subtract:
        value = lhs - rhs;
        break;
    case CalculatorKey::Multiply:
        value = lhs * rhs;
        break;
    case CalculatorKey::Divide:
        // 除数为 0 不做计算，交给上层切换到错误状态（需求 6）
        if (rhs == 0.0)
            return CalcStatus::DivideByZero;
        value = lhs / rhs;
        break;
    default:
        return CalcStatus::Overflow;
    }

    if (!std::isfinite(value))
        return CalcStatus::Overflow;

    *result = value;
    return CalcStatus::Ok;
}

bool CalculatorEngine::isDigitKey(CalculatorKey key)
{
    const int value = static_cast<int>(key);
    return value >= static_cast<int>(CalculatorKey::Digit0)
           && value <= static_cast<int>(CalculatorKey::Digit9);
}

int CalculatorEngine::digitOf(CalculatorKey key)
{
    return static_cast<int>(key) - static_cast<int>(CalculatorKey::Digit0);
}

bool CalculatorEngine::isBinaryOperator(CalculatorKey key)
{
    return key == CalculatorKey::Add || key == CalculatorKey::Subtract
           || key == CalculatorKey::Multiply || key == CalculatorKey::Divide;
}

QString CalculatorEngine::operatorSymbol(CalculatorKey key)
{
    switch (key) {
    case CalculatorKey::Add:
        return QStringLiteral("+");
    case CalculatorKey::Subtract:
        return QStringLiteral("−");
    case CalculatorKey::Multiply:
        return QStringLiteral("×");
    case CalculatorKey::Divide:
        return QStringLiteral("÷");
    default:
        return QString();
    }
}

QString CalculatorEngine::formatNumber(double value)
{
    if (std::isnan(value) || std::isinf(value))
        return QStringLiteral("错误");

    // 把 -0 规整为 0，避免显示 "-0"
    if (value == 0.0)
        value = 0.0;

    // 'g' 格式会自动去掉多余的尾随 0，并在数值过大/过小时切换成科学计数法。
    // 取 16 位有效数字：既能把 kMaxInputDigits 位整数完整显示出来（不会被提前转成 1e+15），
    // 又能把 0.1 + 0.2 = 0.30000000000000004 这类误差吸收掉显示为 0.3
    return QString::number(value, 'g', 16);
}

int CalculatorEngine::countDigits(const QString &text)
{
    int count = 0;
    for (const QChar &ch : text) {
        if (ch.isDigit())
            ++count;
    }
    return count;
}
