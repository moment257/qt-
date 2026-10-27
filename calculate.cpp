#include "calculate.h"
#include "ui_calculate.h"

#include "historypanel.h"
#include "keymapping.h"

#include <QFontMetrics>
#include <QIcon>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QResizeEvent>
#include <QStyle>
#include <QTimer>

namespace {

/*!
 * \brief 用 QPainter 现画一个退格图标。
 *
 * 退格符号 U+232B 在部分系统字体里没有字形，直接写文字会显示成方块，
 * 这里改成矢量绘制，保证任何环境下的显示都一致。
 */
QIcon createBackspaceIcon(const QColor &color)
{
    constexpr int kGrid = 24;   // 以 24x24 的逻辑网格作图
    QPixmap pixmap(kGrid * 2, kGrid * 2);
    pixmap.setDevicePixelRatio(2.0);   // 2 倍图，高分屏下不糊
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.scale(2.0, 2.0);

    QPen pen(color);
    pen.setWidthF(1.6);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    // 外轮廓：左边一个尖角，右边是矩形，形状与 Windows 计算器的退格键一致
    QPainterPath outline;
    outline.moveTo(2.0, 12.0);
    outline.lineTo(7.5, 5.0);
    outline.lineTo(21.5, 5.0);
    outline.lineTo(21.5, 19.0);
    outline.lineTo(7.5, 19.0);
    outline.closeSubpath();
    painter.drawPath(outline);

    // 中间的小叉
    painter.drawLine(QPointF(12.0, 9.5), QPointF(17.0, 14.5));
    painter.drawLine(QPointF(17.0, 9.5), QPointF(12.0, 14.5));

    painter.end();
    return QIcon(pixmap);
}

/*!
 * Windows 计算器（深色）风格样式表。
 * 统一写在这里而不是逐个按钮设置，避免大段重复代码；
 * 通过 objectName 选择器区分“数字键 / 功能键 / 内存键 / 等号键”。
 */
const char *const kCalculatorStyleSheet = R"(
QWidget#Calculate {
    background-color: #202020;
}

QLabel {
    color: #FFFFFF;
    font-family: "Microsoft YaHei UI", "Segoe UI";
}

QLabel#labelExpression {
    color: #9A9A9A;
}

QLabel#labelMemory {
    color: #9A9A9A;
}

QPushButton {
    background-color: #3B3B3B;
    color: #FFFFFF;
    border: none;
    border-radius: 5px;
    font-family: "Microsoft YaHei UI", "Segoe UI";
    font-size: 18px;
}

QPushButton:hover {
    background-color: #4A4A4A;
}

QPushButton:pressed {
    background-color: #2A2A2A;
}

/* 功能键：颜色略深，与数字键区分 */
QPushButton#btnPercent,
QPushButton#btnClearEntry,
QPushButton#btnClear,
QPushButton#btnBackspace,
QPushButton#btnReciprocal,
QPushButton#btnSquare,
QPushButton#btnSquareRoot,
QPushButton#btnSin,
QPushButton#btnCos,
QPushButton#btnTan,
QPushButton#btnPower,
QPushButton#btnNegate,
QPushButton#btnDivide,
QPushButton#btnMultiply,
QPushButton#btnSubtract,
QPushButton#btnAdd {
    background-color: #323232;
    font-size: 15px;
}

QPushButton#btnPercent:hover,
QPushButton#btnClearEntry:hover,
QPushButton#btnClear:hover,
QPushButton#btnBackspace:hover,
QPushButton#btnReciprocal:hover,
QPushButton#btnSquare:hover,
QPushButton#btnSquareRoot:hover,
QPushButton#btnSin:hover,
QPushButton#btnCos:hover,
QPushButton#btnTan:hover,
QPushButton#btnPower:hover,
QPushButton#btnNegate:hover,
QPushButton#btnDivide:hover,
QPushButton#btnMultiply:hover,
QPushButton#btnSubtract:hover,
QPushButton#btnAdd:hover {
    background-color: #3F3F3F;
}

/* 幂运算键：指数是上标字形，字号比其它功能键大一号才看得清 */
QPushButton#btnPower {
    font-size: 16px;
}

/* 角度符号键：按下后当前数字被标成角度，按钮同步变琥珀色，
   提示接下来的 sin/cos/tan 会按角度制换算（与全局 DEG/RAD 模式无关） */
QPushButton#btnDegree {
    background-color: #323232;
    color: #D8D8D8;
    font-size: 17px;
    border: 1px solid #323232;
}

QPushButton#btnDegree:hover {
    background-color: #3F3F3F;
}

QPushButton#btnDegree[marked="true"] {
    background-color: #3A3326;
    color: #E8B767;
    border: 1px solid #E8B767;
}

/* 角度单位切换键：DEG 是深色底 + 绿字，切到 RAD 后变成琥珀色底 + 深字，
   一眼就能看出当前用的不是默认单位 */
QPushButton#btnAngleMode {
    background-color: #2C2C2C;
    color: #8FCF9A;
    font-size: 13px;
    border: 1px solid #3D3D3D;
}

QPushButton#btnAngleMode:hover {
    background-color: #3A3A3A;
}

QPushButton#btnAngleMode[radian="true"] {
    background-color: #E8B767;
    color: #1A1A1A;
    border: 1px solid #E8B767;
}

QPushButton#btnAngleMode[radian="true"]:hover {
    background-color: #F0C27C;
}

/* 内存键：与截图一致，做成无底色的浅灰小字 */
QPushButton#btnMC,
QPushButton#btnMR,
QPushButton#btnMAdd,
QPushButton#btnMSub,
QPushButton#btnMS,
QPushButton#btnMList {
    background-color: transparent;
    color: #CFCFCF;
    font-size: 12px;
}

QPushButton#btnMC:hover,
QPushButton#btnMR:hover,
QPushButton#btnMAdd:hover,
QPushButton#btnMSub:hover,
QPushButton#btnMS:hover {
    background-color: #2F2F2F;
}

QPushButton#btnMList:disabled {
    color: #7A7A7A;
}

/* 等号键：浅色强调 */
QPushButton#btnEquals {
    background-color: #E6E6E6;
    color: #101010;
    font-size: 20px;
}

QPushButton#btnEquals:hover {
    background-color: #F5F5F5;
}

QPushButton#btnEquals:pressed {
    background-color: #C9C9C9;
}
)";

} // namespace

Calculate::Calculate(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Calculate)
{
    ui->setupUi(this);

    // 让顶层 QWidget 的 stylesheet 背景真正生效
    setAttribute(Qt::WA_StyledBackground, true);

    applyStyleSheet();

    // 副显示区占满剩余宽度并右对齐：文本变长时不需要等布局重排，避免出现被裁掉一截的瞬间
    ui->labelExpression->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // 退格键用绘制的图标代替文字，避免字体缺字形时显示成方块
    ui->btnBackspace->setText(QString());
    ui->btnBackspace->setIcon(createBackspaceIcon(QColor(0xE6, 0xE6, 0xE6)));
    ui->btnBackspace->setIconSize(QSize(22, 22));

    setupButtonBindings();

    // M▾ 不改变计算状态，因此不进按键映射表，单独连接成“打开历史面板”
    ui->btnMList->setFocusPolicy(Qt::NoFocus);
    connect(ui->btnMList, &QPushButton::clicked, this, &Calculate::toggleHistoryPanel);

    refreshDisplay();
}

Calculate::~Calculate()
{
    delete ui;
}

void Calculate::setupButtonBindings()
{
    // 一张表描述“界面按钮 -> 逻辑按键”。
    // 连接信号、键盘视觉反馈都复用这张表，新增按键时只需要加一行，
    // 不需要为每个按钮重复写 connect（需求 7：避免大段重复代码）。
    m_buttonBindings = {
        {ui->btn0, CalculatorKey::Digit0},
        {ui->btn1, CalculatorKey::Digit1},
        {ui->btn2, CalculatorKey::Digit2},
        {ui->btn3, CalculatorKey::Digit3},
        {ui->btn4, CalculatorKey::Digit4},
        {ui->btn5, CalculatorKey::Digit5},
        {ui->btn6, CalculatorKey::Digit6},
        {ui->btn7, CalculatorKey::Digit7},
        {ui->btn8, CalculatorKey::Digit8},
        {ui->btn9, CalculatorKey::Digit9},
        {ui->btnDecimalPoint, CalculatorKey::DecimalPoint},
        {ui->btnAdd, CalculatorKey::Add},
        {ui->btnSubtract, CalculatorKey::Subtract},
        {ui->btnMultiply, CalculatorKey::Multiply},
        {ui->btnDivide, CalculatorKey::Divide},
        {ui->btnEquals, CalculatorKey::Equals},
        {ui->btnPercent, CalculatorKey::Percent},
        {ui->btnNegate, CalculatorKey::Negate},
        {ui->btnReciprocal, CalculatorKey::Reciprocal},
        {ui->btnSquare, CalculatorKey::Square},
        {ui->btnSquareRoot, CalculatorKey::SquareRoot},
        {ui->btnSin, CalculatorKey::Sine},
        {ui->btnCos, CalculatorKey::Cosine},
        {ui->btnTan, CalculatorKey::Tangent},
        {ui->btnDegree, CalculatorKey::DegreeMark},
        {ui->btnPower, CalculatorKey::Power},
        {ui->btnAngleMode, CalculatorKey::AngleMode},
        {ui->btnBackspace, CalculatorKey::Backspace},
        {ui->btnClearEntry, CalculatorKey::ClearEntry},
        {ui->btnClear, CalculatorKey::ClearAll},
        {ui->btnMC, CalculatorKey::MemoryClear},
        {ui->btnMR, CalculatorKey::MemoryRecall},
        {ui->btnMAdd, CalculatorKey::MemoryAdd},
        {ui->btnMSub, CalculatorKey::MemorySubtract},
        {ui->btnMS, CalculatorKey::MemoryStore},
    };

    for (const ButtonBinding &binding : m_buttonBindings) {
        // 按钮不接收焦点，保证键盘事件始终由窗口的 keyPressEvent 处理
        binding.button->setFocusPolicy(Qt::NoFocus);

        connect(binding.button, &QPushButton::clicked, this,
                [this, key = binding.key]() { pressCalculatorKey(key); });
    }
}

void Calculate::pressCalculatorKey(CalculatorKey key, bool visualFeedback)
{
    if (visualFeedback)
        flashButton(key);

    // 鼠标与键盘唯一的业务入口：两种输入方式共享同一套处理逻辑
    m_engine.pressKey(key);
    refreshDisplay();
}

void Calculate::keyPressEvent(QKeyEvent *event)
{
    // Ctrl+H 与点击 M▾ 等价，键盘用户也能打开历史面板
    if (event->key() == Qt::Key_H && event->modifiers().testFlag(Qt::ControlModifier)) {
        toggleHistoryPanel();
        event->accept();
        return;
    }

    CalculatorKey key = CalculatorKey::ClearAll;
    if (KeyMapping::fromKeyboard(event->key(), event->modifiers(), &key)) {
        // 键盘输入时让对应按钮闪一下，用户得到的反馈与鼠标点击一致
        pressCalculatorKey(key, true);
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void Calculate::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    adjustDisplayFont();    // 窗口变宽/变窄后重新挑选合适的字号
}

void Calculate::refreshDisplay()
{
    ui->labelDisplay->setText(m_engine.displayText());
    ui->labelExpression->setText(m_engine.expressionText());
    ui->labelMemory->setVisible(m_engine.hasMemory());

    // 角度单位按钮：文字随模式变，颜色由动态属性 radian 决定
    ui->btnAngleMode->setText(m_engine.angleModeText());
    const bool radian = m_engine.angleMode() == CalculatorEngine::AngleMode::Radian;
    if (ui->btnAngleMode->property("radian").toBool() != radian) {
        ui->btnAngleMode->setProperty("radian", radian);
        // 动态属性变化后要手动让样式表重新生效
        ui->btnAngleMode->style()->unpolish(ui->btnAngleMode);
        ui->btnAngleMode->style()->polish(ui->btnAngleMode);
    }

    // 角度符号键：当前数字带 ° 标记时高亮，用户能看出这个后缀还生效
    const bool marked = m_engine.hasDegreeMark();
    if (ui->btnDegree->property("marked").toBool() != marked) {
        ui->btnDegree->setProperty("marked", marked);
        ui->btnDegree->style()->unpolish(ui->btnDegree);
        ui->btnDegree->style()->polish(ui->btnDegree);
    }

    adjustDisplayFont();
}

void Calculate::adjustDisplayFont()
{
    // 数字很长（或窗口很窄）时自动缩小字号，避免显示被截断
    const QString text = ui->labelDisplay->text();
    const int availableWidth = qMax(40, ui->labelDisplay->width() - 12);

    QFont font = ui->labelDisplay->font();
    // 错误提示（如“除数不能为零”）字数较多，用更小的基准字号，避免顶到显示区两边
    int pixelSize = m_engine.isError() ? 28 : 46;
    font.setPixelSize(pixelSize);
    while (pixelSize > 18 && QFontMetrics(font).horizontalAdvance(text) > availableWidth) {
        font.setPixelSize(--pixelSize);
    }

    ui->labelDisplay->setFont(font);
}

void Calculate::applyStyleSheet()
{
    setStyleSheet(QString::fromUtf8(kCalculatorStyleSheet));
}

void Calculate::flashButton(CalculatorKey key)
{
    QPushButton *button = buttonForKey(key);
    if (!button || !button->isEnabled())
        return;

    // 用按下状态模拟一次“按下 - 弹起”，不会触发 clicked，纯粹是视觉反馈
    button->setDown(true);
    QTimer::singleShot(90, button, [button]() { button->setDown(false); });
}

QPushButton *Calculate::buttonForKey(CalculatorKey key) const
{
    for (const ButtonBinding &binding : m_buttonBindings) {
        if (binding.key == key)
            return binding.button;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// 计算历史面板（M 键）
// ---------------------------------------------------------------------------

void Calculate::toggleHistoryPanel()
{
    if (m_historyPanel && m_historyPanel->isVisible()) {
        m_historyPanel->hide();
        return;
    }

    if (!m_historyPanel) {
        m_historyPanel = new HistoryPanel(this);

        // 点击内存行 = 按下 MR，走的仍是引擎里同一个处理函数
        connect(m_historyPanel, &HistoryPanel::memoryActivated, this,
                [this]() { pressCalculatorKey(CalculatorKey::MemoryRecall, true); });
        connect(m_historyPanel, &HistoryPanel::recordActivated, this,
                &Calculate::recallHistoryRecord);
        connect(m_historyPanel, &HistoryPanel::clearHistoryRequested, this, [this]() {
            m_engine.clearHistory();
            updateHistoryPanel();
        });
    }

    updateHistoryPanel();
    positionHistoryPanel();
    m_historyPanel->show();
    m_historyPanel->raise();
}

void Calculate::updateHistoryPanel()
{
    if (!m_historyPanel)
        return;

    m_historyPanel->setContent(m_engine.hasMemory(), m_engine.memoryText(), m_engine.history());
}

void Calculate::positionHistoryPanel()
{
    if (!m_historyPanel)
        return;

    // 面板贴在显示区上，下沿紧挨内存键行（btnMList 所在的那一行）
    const QRect windowRect(mapToGlobal(QPoint(0, 0)), size());
    const QRect anchorRect(ui->btnMList->mapToGlobal(QPoint(0, 0)), ui->btnMList->size());

    constexpr int kMargin = 8;
    constexpr int kMinPanelHeight = 160;

    const int top = windowRect.top() + kMargin;
    const int height = qMax(kMinPanelHeight, anchorRect.top() - 6 - top);
    const int width = qMax(240, windowRect.width() - kMargin * 2);

    m_historyPanel->setGeometry(windowRect.left() + kMargin, top, width, height);
}

void Calculate::recallHistoryRecord(int index)
{
    m_engine.recallHistory(index);
    refreshDisplay();
}
