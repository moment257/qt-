#ifndef HISTORYPANEL_H
#define HISTORYPANEL_H

#include <QFrame>
#include <QVector>

#include "calculatorengine.h"

class QEvent;
class QKeyEvent;
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;

/*!
 * \brief 点击 M 键（btnMList）后弹出的“计算历史”面板。
 *
 * 面板只负责显示与交互，不保存任何数据：每次弹出前由 Calculate 把
 * CalculatorEngine 里的内存值和历史记录灌进来，点击某一行时通过信号
 * 把“用户选了哪一条”告诉主窗口。
 *
 * 用 Qt::Popup 作为窗口标志，点击面板外部会自动关闭，符合浮层的使用习惯。
 */
class HistoryPanel : public QFrame
{
    Q_OBJECT

public:
    /// 内存行使用的特殊下标，与历史记录的下标区分开
    static constexpr int kMemoryRowIndex = -1;

    explicit HistoryPanel(QWidget *parent = nullptr);

    /*!
     * \brief 刷新面板内容。
     * \param hasMemory   是否存在内存值，false 时不显示内存行
     * \param memoryText  内存值的显示文本
     * \param records     历史记录，内部按时间倒序展示（最新的在最上面）
     */
    void setContent(bool hasMemory, const QString &memoryText,
                    const QVector<CalculationRecord> &records);

signals:
    /// 用户点击了内存行，等价于按下 MR
    void memoryActivated();
    /// 用户点击了某条历史记录，index 对应 records 中的下标
    void recordActivated(int index);
    /// 用户点击了“清除全部”
    void clearHistoryRequested();

protected:
    /// Esc 关闭面板
    void keyPressEvent(QKeyEvent *event) override;
    /// 行是由 QFrame + 两个 QLabel 拼出来的，用事件过滤器统一处理点击
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void clearRows();
    QFrame *createRow(const QString &expression, const QString &result, int recordIndex);

    QVBoxLayout *m_rowLayout = nullptr;   ///< 滚动区内容区的布局，所有行都挂在这里
    QWidget *m_rowContainer = nullptr;    ///< 滚动区的内容控件
    QScrollArea *m_scrollArea = nullptr;
    QLabel *m_emptyHint = nullptr;        ///< 没有记录时的提示文字
    QPushButton *m_clearButton = nullptr;
};

#endif // HISTORYPANEL_H
