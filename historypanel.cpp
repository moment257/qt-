#include "historypanel.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

namespace {

/// 每行 QFrame 上保存“对应哪条记录”的动态属性名
const char *const kRecordIndexProperty = "recordIndex";

/*!
 * 面板样式：独立于主窗口，避免继承主窗口里给计算器按键写的那套规则。
 * 只写颜色与字号，字体族沿用主窗口的微软雅黑。
 */
const char *const kHistoryPanelStyleSheet = R"(
QFrame#historyPanel {
    background-color: #2B2B2B;
    border: 1px solid #3D3D3D;
    border-radius: 6px;
}

QWidget#historyHeader {
    background-color: transparent;
}

QLabel#historyTitle {
    color: #C8C8C8;
    font-size: 12px;
}

QPushButton#btnClearHistory {
    background-color: transparent;
    color: #7FA6D0;
    border: none;
    border-radius: 3px;
    font-size: 12px;
    padding: 3px 6px;
}

QPushButton#btnClearHistory:hover {
    background-color: #3A3A3A;
    color: #A6C4E4;
}

QPushButton#btnClearHistory:disabled {
    color: #5E5E5E;
}

QScrollArea#historyScroll,
QWidget#historyRowContainer {
    background-color: transparent;
    border: none;
}

/* 一条记录：默认透明，鼠标悬停时高亮，暗示可以点击 */
QFrame#historyRow {
    background-color: transparent;
    border: none;
    border-radius: 4px;
}

QFrame#historyRow:hover {
    background-color: #383838;
}

QLabel#historyExpression {
    color: #9A9A9A;
    font-size: 12px;
}

QLabel#historyResult {
    color: #FFFFFF;
    font-size: 18px;
}

QLabel#historyEmpty {
    color: #7A7A7A;
    font-size: 12px;
}

QScrollBar:vertical {
    background: transparent;
    width: 8px;
    margin: 2px;
}

QScrollBar::handle:vertical {
    background-color: #4A4A4A;
    border-radius: 4px;
    min-height: 24px;
}

QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical {
    height: 0;
}

QScrollBar::add-page:vertical,
QScrollBar::sub-page:vertical {
    background: transparent;
}
)";

} // namespace

HistoryPanel::HistoryPanel(QWidget *parent)
    : QFrame(parent, Qt::Popup)
{
    setObjectName(QStringLiteral("historyPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setFrameShape(QFrame::NoFrame);
    setStyleSheet(QString::fromUtf8(kHistoryPanelStyleSheet));

    buildUi();
}

void HistoryPanel::buildUi()
{
    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // ---- 标题栏：标题 + 清除按钮 ----
    auto *header = new QWidget(this);
    header->setObjectName(QStringLiteral("historyHeader"));

    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 8, 6);
    headerLayout->setSpacing(6);

    auto *title = new QLabel(QStringLiteral("计算历史"), header);
    title->setObjectName(QStringLiteral("historyTitle"));

    m_clearButton = new QPushButton(QStringLiteral("清除"), header);
    m_clearButton->setObjectName(QStringLiteral("btnClearHistory"));
    m_clearButton->setCursor(Qt::PointingHandCursor);
    m_clearButton->setFocusPolicy(Qt::NoFocus);
    connect(m_clearButton, &QPushButton::clicked, this, &HistoryPanel::clearHistoryRequested);

    headerLayout->addWidget(title);
    headerLayout->addStretch(1);
    headerLayout->addWidget(m_clearButton);
    outerLayout->addWidget(header);

    // ---- 滚动区：记录很多时不会把面板撑破 ----
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName(QStringLiteral("historyScroll"));
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_rowContainer = new QWidget(m_scrollArea);
    m_rowContainer->setObjectName(QStringLiteral("historyRowContainer"));

    m_rowLayout = new QVBoxLayout(m_rowContainer);
    m_rowLayout->setContentsMargins(6, 0, 6, 6);
    m_rowLayout->setSpacing(2);

    m_emptyHint = new QLabel(QStringLiteral("还没有计算记录\n按 = 完成一次计算后会出现在这里"),
                             m_rowContainer);
    m_emptyHint->setObjectName(QStringLiteral("historyEmpty"));
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setWordWrap(true);
    m_emptyHint->setMinimumHeight(90);
    m_emptyHint->hide();

    m_scrollArea->setWidget(m_rowContainer);
    outerLayout->addWidget(m_scrollArea, 1);
}

void HistoryPanel::setContent(bool hasMemory, const QString &memoryText,
                             const QVector<CalculationRecord> &records)
{
    clearRows();

    // 内存值放在最上面，点击它相当于按下 MR
    if (hasMemory)
        m_rowLayout->addWidget(createRow(QStringLiteral("内存 M"), memoryText, kMemoryRowIndex));

    // 历史按时间倒序展示：刚算完的那条排在最上面，符合“回看刚才算了什么”的直觉
    for (int index = records.size() - 1; index >= 0; --index) {
        const CalculationRecord &record = records.at(index);
        m_rowLayout->addWidget(createRow(record.expression, record.result, index));
    }

    if (records.isEmpty()) {
        // 上下各留一个弹簧，让提示文字落在面板中间而不是贴着顶部
        m_rowLayout->addStretch(1);
        m_rowLayout->addWidget(m_emptyHint);
        m_emptyHint->show();
    } else {
        m_emptyHint->hide();
    }

    m_rowLayout->addStretch(1);
    m_clearButton->setEnabled(!records.isEmpty());
}

void HistoryPanel::clearRows()
{
    while (QLayoutItem *item = m_rowLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            // 提示标签要重复使用，只从布局里摘出来，不能删掉
            if (widget == m_emptyHint) {
                widget->hide();
            } else {
                // 先隐藏再延迟销毁：当前可能正处在这一行的事件过滤器里，
                // 立刻 delete 会让调用栈上的对象凭空消失
                widget->hide();
                widget->deleteLater();
            }
        }
        delete item;
    }
}

QFrame *HistoryPanel::createRow(const QString &expression, const QString &result, int recordIndex)
{
    auto *row = new QFrame(m_rowContainer);
    row->setObjectName(QStringLiteral("historyRow"));
    row->setAttribute(Qt::WA_Hover, true);
    row->setCursor(Qt::PointingHandCursor);
    row->setProperty(kRecordIndexProperty, recordIndex);

    auto *rowLayout = new QVBoxLayout(row);
    rowLayout->setContentsMargins(8, 4, 8, 5);
    rowLayout->setSpacing(1);

    auto *expressionLabel = new QLabel(expression, row);
    expressionLabel->setObjectName(QStringLiteral("historyExpression"));
    expressionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    expressionLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    auto *resultLabel = new QLabel(result, row);
    resultLabel->setObjectName(QStringLiteral("historyResult"));
    resultLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    resultLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    rowLayout->addWidget(expressionLabel);
    rowLayout->addWidget(resultLabel);

    // 点击落在整行上，而不是落在两个标签上
    row->installEventFilter(this);
    return row;
}

bool HistoryPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);
        auto *row = qobject_cast<QFrame *>(watched);
        if (row && mouseEvent->button() == Qt::LeftButton) {
            const int index = row->property(kRecordIndexProperty).toInt();

            // 先收起面板再通知主窗口，避免面板还开着时背后的显示区已经变了
            hide();
            if (index == kMemoryRowIndex)
                emit memoryActivated();
            else
                emit recordActivated(index);
            return true;
        }
    }

    return QFrame::eventFilter(watched, event);
}

void HistoryPanel::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        event->accept();
        return;
    }

    QFrame::keyPressEvent(event);
}
