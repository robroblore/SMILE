#include "ui/Theme.h"

#include <QApplication>
#include <QFont>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QWidget>

namespace Theme {

namespace {

Colors lightColors()
{
    Colors c;
    c.dark = false;
    c.background = QColor(0xF3, 0xF3, 0xF3);
    c.card = QColor(0xFF, 0xFF, 0xFF);
    c.cardBorder = QColor(0xE5, 0xE5, 0xE5);
    c.flyout = QColor(0xFC, 0xFC, 0xFC);
    c.text = QColor(0x1A, 0x1A, 0x1A);
    c.textSecondary = QColor(0x5D, 0x5D, 0x5D);
    c.textDisabled = QColor(0xA0, 0xA0, 0xA0);
    c.accent = QColor(0x00, 0x5F, 0xB8);
    c.accentHover = QColor(0x19, 0x6E, 0xBF);
    c.accentPressed = QColor(0x31, 0x7D, 0xC6);
    c.onAccent = QColor(0xFF, 0xFF, 0xFF);
    c.control = QColor(0xFB, 0xFB, 0xFB);
    c.controlHover = QColor(0xF6, 0xF6, 0xF6);
    c.controlPressed = QColor(0xF2, 0xF2, 0xF2);
    c.controlBorder = QColor(0xE5, 0xE5, 0xE5);
    c.controlBorderBottom = QColor(0xCC, 0xCC, 0xCC);
    c.subtleHover = QColor(0, 0, 0, 10);
    c.subtlePressed = QColor(0, 0, 0, 6);
    c.track = QColor(0xD6, 0xD6, 0xD6);
    c.success = QColor(0x0F, 0x7B, 0x0F);
    c.caution = QColor(0x9D, 0x5D, 0x00);
    c.critical = QColor(0xC4, 0x2B, 0x1C);
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
    c.critical = QColor(0xFF, 0x99, 0xA4);
    return c;
}

Colors g_colors = lightColors();

bool systemPrefersDark()
{
    // SMILE_THEME=light|dark overrides the system setting (handy for testing)
    const QByteArray forced = qgetenv("SMILE_THEME").toLower();
    if (forced == "dark")
        return true;
    if (forced == "light")
        return false;
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
#root, #navPane, QStackedWidget, #page { background: @bg; }
QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; border: none; }

#pageTitle { font-size: 21pt; font-weight: 600; }
#pageSubtitle, #secondary { color: @text2; }
#card { background: @card; border: 1px solid @cardBorder; border-radius: 8px; }
#cardTitle { font-size: 11.5pt; font-weight: 600; }
#caption { color: @text2; font-size: 8.5pt; font-weight: 600; }
#bigValue { font-size: 20pt; font-weight: 600; }
#appTitle { font-size: 11pt; font-weight: 600; }

QPushButton {
    background: @control; border: 1px solid @controlBorder; border-bottom-color: @controlBorderBottom;
    border-radius: 5px; padding: 6px 16px; min-height: 20px;
}
QPushButton:hover { background: @controlHover; }
QPushButton:pressed { background: @controlPressed; color: @text2; border-bottom-color: @controlBorder; }
QPushButton:disabled { color: @textDisabled; background: @control; border-color: @controlBorder; }
QPushButton#accent { background: @accent; color: @onAccent; border: 1px solid @accent; }
QPushButton#accent:hover { background: @accentHover; border-color: @accentHover; }
QPushButton#accent:pressed { background: @accentPressed; border-color: @accentPressed; }
QPushButton#accent:disabled { background: @track; color: @textDisabled; border-color: @track; }
QPushButton#subtle { background: transparent; border: 1px solid transparent; padding: 6px 10px; }
QPushButton#subtle:hover { background: @subtleHover; }
QPushButton#subtle:pressed { background: @subtlePressed; }

QLineEdit, QAbstractSpinBox {
    background: @control; border: 1px solid @controlBorder; border-bottom: 1px solid @controlBorderBottom;
    border-radius: 5px; padding: 5px 10px; min-height: 20px;
    selection-background-color: @accent; selection-color: @onAccent;
}
QLineEdit:hover, QAbstractSpinBox:hover { background: @controlHover; }
QLineEdit:focus, QAbstractSpinBox:focus { background: @card; border-bottom: 2px solid @accent; }
QLineEdit:disabled, QAbstractSpinBox:disabled { color: @textDisabled; }

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

void apply(QApplication &app)
{
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
                     [&app] { applyColors(app); });
#endif
}

void repolish(QWidget *widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace Theme
