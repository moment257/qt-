#ifndef CALCULATE_H
#define CALCULATE_H

#include <QVector>
#include <QWidget>

#include "calculatorengine.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class Calculate;
}
QT_END_NAMESPACE

class QPushButton;
class QKeyEvent;
class QResizeEvent;
class HistoryPanel;

/*!
 * \brief 计算器主窗口。
 *
 * 界面本身完全由 calculate.ui 搭建，本类只做三件事：
 *  1. 把界面上的按钮和 CalculatorKey 建立映射；
 *  2. 把鼠标点击、键盘按键都交给同一个 pressCalculatorKey()；
 *  3. 把计算核心的结果刷新到显示区，并按 Windows 计算器风格设置样式。
 */
class Calculate : public QWidget
{
    Q_OBJECT

public:
    explicit Calculate(QWidget *parent = nullptr);
    ~Calculate() override;

protected:
    /// 键盘输入：映射成计算器按键后走和鼠标完全相同的处理路径
    void keyPressEvent(QKeyEvent *event) override;
    /// 窗口尺寸变化时重新计算显示区字号
    void resizeEvent(QResizeEvent *event) override;

private:
    /// 一个按钮与一个逻辑按键的绑定关系
    struct ButtonBinding {
        QPushButton *button = nullptr;
        CalculatorKey key = CalculatorKey::ClearAll;
    };

    void setupButtonBindings();
    void pressCalculatorKey(CalculatorKey key, bool visualFeedback = false);
    void refreshDisplay();
    void applyStyleSheet();
    void adjustDisplayFont();
    void flashButton(CalculatorKey key);
    QPushButton *buttonForKey(CalculatorKey key) const;

    // ---- 计算历史面板（M 键） ----
    void toggleHistoryPanel();
    void updateHistoryPanel();
    void positionHistoryPanel();
    void recallHistoryRecord(int index);

    Ui::Calculate *ui;
    CalculatorEngine m_engine;
    QVector<ButtonBinding> m_buttonBindings;
    HistoryPanel *m_historyPanel = nullptr;   ///< 首次点击 M 键时创建，之后复用
};

#endif // CALCULATE_H
