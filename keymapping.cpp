#include "keymapping.h"

namespace KeyMapping {

bool fromKeyboard(int qtKey, Qt::KeyboardModifiers modifiers, CalculatorKey *outKey)
{
    if (!outKey)
        return false;

    // ---- Ctrl 组合键：内存操作，与 Windows 计算器的习惯保持一致 ----
    if (modifiers.testFlag(Qt::ControlModifier)) {
        switch (qtKey) {
        case Qt::Key_L:
            *outKey = CalculatorKey::MemoryClear;
            return true;
        case Qt::Key_R:
            *outKey = CalculatorKey::MemoryRecall;
            return true;
        case Qt::Key_M:
            *outKey = CalculatorKey::MemoryStore;
            return true;
        case Qt::Key_P:
            *outKey = CalculatorKey::MemoryAdd;
            return true;
        case Qt::Key_Q:
            *outKey = CalculatorKey::MemorySubtract;
            return true;
        default:
            return false;
        }
    }

    // ---- 数字键（主键盘与小键盘都会落到 Key_0 ~ Key_9）----
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        *outKey = static_cast<CalculatorKey>(static_cast<int>(CalculatorKey::Digit0)
                                             + (qtKey - Qt::Key_0));
        return true;
    }

    switch (qtKey) {
    case Qt::Key_Period:
    case Qt::Key_Comma:      // 部分键盘布局的小数点会报成逗号
        *outKey = CalculatorKey::DecimalPoint;
        return true;
    case Qt::Key_Plus:
        *outKey = CalculatorKey::Add;
        return true;
    case Qt::Key_Minus:
        *outKey = CalculatorKey::Subtract;
        return true;
    case Qt::Key_Asterisk:
        *outKey = CalculatorKey::Multiply;
        return true;
    case Qt::Key_Slash:
        *outKey = CalculatorKey::Divide;
        return true;
    case Qt::Key_Equal:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        *outKey = CalculatorKey::Equals;
        return true;
    case Qt::Key_Percent:
        *outKey = CalculatorKey::Percent;
        return true;
    case Qt::Key_Backspace:
        *outKey = CalculatorKey::Backspace;
        return true;
    case Qt::Key_Escape:
        *outKey = CalculatorKey::ClearAll;
        return true;
    case Qt::Key_Delete:
        *outKey = CalculatorKey::ClearEntry;
        return true;
    case Qt::Key_F9:
        *outKey = CalculatorKey::Negate;
        return true;
    case Qt::Key_R:
        *outKey = CalculatorKey::Reciprocal;
        return true;
    case Qt::Key_Q:
        *outKey = CalculatorKey::Square;
        return true;
    case Qt::Key_At:
        *outKey = CalculatorKey::SquareRoot;
        return true;
    // ---- 三角函数：取英文首字母，D 用来切换角度 / 弧度 ----
    case Qt::Key_S:
        *outKey = CalculatorKey::Sine;
        return true;
    case Qt::Key_C:
        *outKey = CalculatorKey::Cosine;
        return true;
    case Qt::Key_T:
        *outKey = CalculatorKey::Tangent;
        return true;
    case Qt::Key_D:
        *outKey = CalculatorKey::AngleMode;
        return true;
    // 角度符号取单引号：度分秒的写法里 30° 就常写作 30'，且不与任何计算键冲突
    case Qt::Key_Apostrophe:
        *outKey = CalculatorKey::DegreeMark;
        return true;
    default:
        return false;
    }
}

} // namespace KeyMapping
