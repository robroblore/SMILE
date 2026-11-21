#include "ui/Theme.h"

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QGraphicsDropShadowEffect>
#include <QWidget>

#include "core/AppSettings.h"

namespace Theme {

namespace {

Colors lightColors()
{
    // White & blue: white surfaces, light-blue secondary elements, strong blue
    // for primary actions; green/orange/red only for performance feedback.
    Colors c;
    c.dark = false;
    c.background = QColor(0xF5, 0xF8, 0xFD);
    c.nav = QColor(0xEA, 0xF1, 0xFC);
    c.card = QColor(0xFF, 0xFF, 0xFF);
    c.cardBorder = QColor(0xDF, 0xE8, 0xF5);
    c.flyout = QColor(0xFF, 0xFF, 0xFF);
    c.text = QColor(0x10, 0x1F, 0x36);
    c.textSecondary = QColor(0x53, 0x63, 0x7C);
    c.textDisabled = QColor(0xA3, 0xAE, 0xBF);
    c.accent = QColor(0x1F, 0x63, 0xE0);
    c.accentHover = QColor(0x1A, 0x56, 0xC6);
    c.accentPressed = QColor(0x16, 0x4A, 0xAD);
    c.onAccent = QColor(0xFF, 0xFF, 0xFF);
    c.control = QColor(0xFF, 0xFF, 0xFF);
    c.controlHover = QColor(0xF6, 0xF9, 0xFF);
    c.controlPressed = QColor(0xEC, 0xF2, 0xFD);
    c.controlBorder = QColor(0xD5, 0xE1, 0xF2);
    c.controlBorderBottom = QColor(0xBF, 0xCF, 0xE8);
    c.button = QColor(0xE8, 0xF0, 0xFD);
    c.buttonHover = QColor(0xDC, 0xE8, 0xFC);
    c.subtleHover = QColor(0x1F, 0x63, 0xE0, 20);
    c.subtlePressed = QColor(0x1F, 0x63, 0xE0, 12);
    c.track = QColor(0xDC, 0xE6, 0xF5);
    c.success = QColor(0x13, 0x8A, 0x43);
    c.caution = QColor(0x9D, 0x5D, 0x00);
    c.critical = QColor(0xD1, 0x34, 0x38);
    c.orange = QColor(0xD9, 0x73, 0x1A);
    c.shadow = QColor(0x10, 0x30, 0x70, 22);
    return c;
}

Colors darkColors()
{
    Colors c;
    c.dark = true;
    c.background = QColor(0x20, 0x20, 0x20);
    c.card = QColor(0x2B, 0x2B, 0x2B);
    c.cardBorder = QColor(0x1C, 0x1C, 0x1C);
    c.flyout = QColor(0x2C, 0x2C, 0x2C);
    c.text = QColor(0xFF, 0xFF, 0xFF);
    c.textSecondary = QColor(0xC5, 0xC5, 0xC5);
    c.textDisabled = QColor(0x78, 0x78, 0x78);
    c.accent = QColor(0x4C, 0xC2, 0xFF);
    c.accentHover = QColor(0x47, 0xB1, 0xE8);
    c.accentPressed = QColor(0x42, 0xA1, 0xD2);
    c.onAccent = QColor(0x00, 0x00, 0x00);
    c.control = QColor(0x2D, 0x2D, 0x2D);
    c.controlHover = QColor(0x32, 0x32, 0x32);
    c.controlPressed = QColor(0x27, 0x27, 0x27);
    c.controlBorder = QColor(0x35, 0x35, 0x35);
    c.controlBorderBottom = QColor(0x40, 0x40, 0x40);
    c.subtleHover = QColor(255, 255, 255, 15);
    c.subtlePressed = QColor(255, 255, 255, 10);
    c.track = QColor(0x48, 0x48, 0x48);
    c.success = QColor(0x6C, 0xCB, 0x5F);
    c.caution = QColor(0xFC, 0xE1, 0x00);
    c.critical = QColor(0xFF, 0x80, 0x8A);
    c.orange = QColor(0xFF, 0xA9, 0x4D);
    c.nav = c.background;
    c.button = c.control;
    c.buttonHover = c.controlHover;
    c.shadow = QColor(0, 0, 0, 0);
    return c;
}

Colors g_colors = lightColors();

QApplication *g_app = nullptr;

bool systemPrefersDark()
{
    // SMILE_THEME=light|dark overrides the system setting (handy for testing)
    const QByteArray forced = qgetenv("SMILE_THEME").toLower();
    if (forced == "dark")
        return true;
    if (forced == "light")
        return false;
    const int mode = AppSettings::instance().themeMode();
    if (mode == 1)
        return false;
    if (mode == 2)
        return true;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return false;
#endif
}

QString styleSheet(const Colors &c)
{
    QString s = QStringLiteral(R"(
QWidget { color: @text; }
#root, QStackedWidget, #page { background: @bg; }
#navPane { background: @nav; }
QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }

#pageTitle { font-size: 21pt; font-weight: 600; }
#pageSubtitle, #secondary { color: @text2; }
#card { background: @card; border: 1px solid @cardBorder; border-radius: 10px; }
#cardTitle { font-size: 11.5pt; font-weight: 600; }
#caption { color: @text2; font-size: 8.5pt; font-weight: 600; }
#bigValue { font-size: 20pt; font-weight: 600; }
#appTitle { font-size: 11pt; font-weight: 600; }

QPushButton {
    background: @button; border: 1px solid @controlBorder; border-bottom-color: @controlBorderBottom;
    border-radius: 6px; padding: 6px 16px; min-height: 20px;
}
QPushButton:hover { background: @buttonHover; }
QPushButton:pressed { background: @controlPressed; color: @text2; border-bottom-color: @controlBorder; }
QPushButton:disabled { color: @textDisabled; background: @control; border-color: @controlBorder; }
QPushButton#accent { background: @accent; color: @onAccent; border: 1px solid @accent; }
QPushButton#accent:hover { background: @accentHover; border-color: @accentHover; }
QPushButton#accent:pressed { background: @accentPressed; border-color: @accentPressed; }
QPushButton#accent:disabled { background: @track; color: @textDisabled; border-color: @track; }
QPushButton#subtle { background: transparent; border: 1px solid transparent; padding: 6px 10px; }
QPushButton#subtle:hover { background: @subtleHover; }
QPushButton#subtle:pressed { background: @subtlePressed; }

QPushButton#segment { background: transparent; border: 1px solid transparent; padding: 5px 12px; }
QPushButton#segment:hover { background: @subtleHover; }
QPushButton#segment:checked { background: @control; color: @accent; border: 1px solid @controlBorder; border-bottom-color: @controlBorderBottom; }
QPlainTextEdit, QTextEdit {
    background: @control; border: 1px solid @controlBorder; border-bottom: 1px solid @controlBorderBottom;
    border-radius: 6px; padding: 6px 8px; selection-background-color: @accent; selection-color: @onAccent;
}
QPlainTextEdit:focus, QTextEdit:focus { background: @card; border-bottom: 2px solid @accent; }
QLineEdit, QAbstractSpinBox {
    background: @control; border: 1px solid @controlBorder; border-bottom: 1px solid @controlBorderBottom;
    border-radius: 5px; padding: 5px 10px; min-height: 20px;
    selection-background-color: @accent; selection-color: @onAccent;
}
QLineEdit:hover, QAbstractSpinBox:hover { background: @controlHover; }
QLineEdit:focus, QAbstractSpinBox:focus { background: @card; border-bottom: 2px solid @accent; }
QLineEdit:disabled, QAbstractSpinBox:disabled { color: @textDisabled; }

QComboBox {
    background: @control; border: 1px solid @controlBorder; border-bottom: 1px solid @controlBorderBottom;
    border-radius: 5px; padding: 5px 10px; min-height: 20px;
}
QComboBox:hover { background: @controlHover; }
QComboBox:disabled { color: @textDisabled; }
QComboBox::drop-down { border: none; width: 24px; }
QComboBox QAbstractItemView {
    background: @flyout; border: 1px solid @cardBorder; border-radius: 6px; padding: 4px;
    selection-background-color: @subtleHover; selection-color: @text; outline: none;
}
QProgressBar { background: @track; border: none; border-radius: 2px; max-height: 4px; min-height: 4px; }
QProgressBar::chunk { background: @accent; border-radius: 2px; }
QProgressBar[state="over"]::chunk { background: @critical; }
QProgressBar[state="done"]::chunk { background: @success; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 4px 2px; }
QScrollBar::handle:vertical { background: @track; border-radius: 3px; min-height: 32px; }
QScrollBar::handle:vertical:hover { background: @text2; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

QToolTip { background: @flyout; color: @text; border: 1px solid @cardBorder; border-radius: 4px; padding: 4px 8px; }
QMenu { background: @flyout; border: 1px solid @cardBorder; border-radius: 8px; padding: 4px; }
QMenu::item { padding: 7px 28px 7px 12px; border-radius: 4px; margin: 1px 0; }
QMenu::item:selected { background: @subtleHover; }
QMenu::item:disabled { color: @textDisabled; }
QMenu::separator { height: 1px; background: @cardBorder; margin: 4px 6px; }
QMessageBox { background: @bg; }
)");
    const QList<std::pair<QString, QColor>> tokens = {
        {QStringLiteral("@textDisabled"), c.textDisabled},
        {QStringLiteral("@text2"), c.textSecondary},
        {QStringLiteral("@text"), c.text},
        {QStringLiteral("@bg"), c.background},
        {QStringLiteral("@cardBorder"), c.cardBorder},
        {QStringLiteral("@card"), c.card},
        {QStringLiteral("@flyout"), c.flyout},
        {QStringLiteral("@accentHover"), c.accentHover},
        {QStringLiteral("@accentPressed"), c.accentPressed},
        {QStringLiteral("@accent"), c.accent},
        {QStringLiteral("@onAccent"), c.onAccent},
        {QStringLiteral("@controlHover"), c.controlHover},
        {QStringLiteral("@controlPressed"), c.controlPressed},
        {QStringLiteral("@controlBorderBottom"), c.controlBorderBottom},
        {QStringLiteral("@controlBorder"), c.controlBorder},
        {QStringLiteral("@control"), c.control},
        {QStringLiteral("@subtleHover"), c.subtleHover},
        {QStringLiteral("@buttonHover"), c.buttonHover},
        {QStringLiteral("@button"), c.button},
        {QStringLiteral("@nav"), c.nav},
        {QStringLiteral("@subtlePressed"), c.subtlePressed},
        {QStringLiteral("@track"), c.track},
        {QStringLiteral("@success"), c.success},
        {QStringLiteral("@critical"), c.critical},
    };
    for (const auto &[token, colour] : tokens)
        s.replace(token, css(colour));
    return s;
}

void applyColors(QApplication &app)
{
    g_colors = systemPrefersDark() ? darkColors() : lightColors();
    const Colors &c = g_colors;

    QPalette pal;
    pal.setColor(QPalette::Window, c.background);
    pal.setColor(QPalette::WindowText, c.text);
    pal.setColor(QPalette::Base, c.card);
    pal.setColor(QPalette::AlternateBase, c.background);
    pal.setColor(QPalette::Text, c.text);
    pal.setColor(QPalette::Button, c.control);
    pal.setColor(QPalette::ButtonText, c.text);
    pal.setColor(QPalette::Highlight, c.accent);
    pal.setColor(QPalette::HighlightedText, c.onAccent);
    pal.setColor(QPalette::ToolTipBase, c.flyout);
    pal.setColor(QPalette::ToolTipText, c.text);
    pal.setColor(QPalette::PlaceholderText, c.textSecondary);
    pal.setColor(QPalette::Link, c.accent);
    pal.setColor(QPalette::Disabled, QPalette::Text, c.textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::WindowText, c.textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::ButtonText, c.textDisabled);
    app.setPalette(pal);
    app.setStyleSheet(styleSheet(c));
}

} // namespace

const Colors &colors()
{
    return g_colors;
}

QString css(const QColor &color)
{
    if (color.alpha() == 255)
        return color.name(QColor::HexRgb);
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

void reapply()
{
    if (g_app)
        applyColors(*g_app);
}

void addShadow(QWidget *widget)
{
    if (g_colors.dark || g_colors.shadow.alpha() == 0)
        return;
    auto *effect = new QGraphicsDropShadowEffect(widget);
    effect->setBlurRadius(22);
    effect->setOffset(0, 3);
    effect->setColor(g_colors.shadow);
    widget->setGraphicsEffect(effect);
}

void apply(QApplication &app)
{
    g_app = &app;
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QFont font = app.font();
    font.setFamilies({QStringLiteral("Segoe UI Variable Text"), QStringLiteral("Segoe UI"),
                      QStringLiteral(".AppleSystemUIFont"), QStringLiteral("SF Pro Text"),
                      QStringLiteral("Inter"), QStringLiteral("Noto Sans"), QStringLiteral("Cantarell"),
                      font.family()});
#ifdef Q_OS_WIN
    font.setPointSizeF(10);
#endif
    app.setFont(font);

    applyColors(app);

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &app,
                     [&app] {
                         if (AppSettings::instance().themeMode() == 0)
                             applyColors(app);
                     });
#endif
}

QColor ratingColor(int rating)
{
    switch (rating) {
    case 1:
        return g_colors.success;
    case 2:
        return g_colors.orange;
    case 3:
        return g_colors.critical;
    default:
        break;
    }
    return g_colors.textSecondary;
}

void repolish(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace Theme
