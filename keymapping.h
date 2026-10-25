#ifndef KEYMAPPING_H
#define KEYMAPPING_H

#include <QtCore/Qt>

#include "calculatorengine.h"

/*!
 * \brief 键盘事件 -> 计算器按键的映射。
 *
 * 单独抽成一个不依赖窗口类的函数，好处有两点：
 *  1. 主窗口的 keyPressEvent 只负责转发，逻辑保持一行；
 *  2. 可以在单元测试里直接验证“键盘输入”和“鼠标点击”是否映射到同一个按键（需求 4）。
 */
namespace KeyMapping {

/*!
 * \brief 把 Qt 键值转换成计算器按键。
 * \param qtKey     QKeyEvent::key()
 * \param modifiers QKeyEvent::modifiers()
 * \param outKey    输出参数，仅在返回 true 时有效
 * \return 该按键是否被计算器支持
 */
bool fromKeyboard(int qtKey, Qt::KeyboardModifiers modifiers, CalculatorKey *outKey);

} // namespace KeyMapping

#endif // KEYMAPPING_H
