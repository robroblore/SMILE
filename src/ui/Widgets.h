#pragma once

#include <QAbstractButton>
#include <QFrame>
#include <QImage>
#include <QLineEdit>
#include <QVariantAnimation>

#include "ui/Icons.h"
#include "vision/FaceFrame.h"

class QLabel;
class QScrollArea;
class QVBoxLayout;

// Windows 11 style on/off switch.
class ToggleSwitch : public QAbstractButton
{
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QVariantAnimation m_anim; // knob position 0..1
    bool m_hover = false;
};

// Round check mark, used for checklist items.
class CheckCircle : public QAbstractButton
{
    Q_OBJECT
public:
    explicit CheckCircle(QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
};

// Item in the left navigation pane: icon + label, accent pill when selected.
class NavButton : public QAbstractButton
{
    Q_OBJECT
public:
    NavButton(Icons::Icon icon, const QString &text, QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    Icons::Icon m_icon;
    bool m_hover = false;
};

// The SMILE face as a plain widget (logo, empty states).
class SmileyBadge : public QWidget
{
    Q_OBJECT
public:
    explicit SmileyBadge(int size, qreal smile = 0.8, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_smile;
};

// Shows the latest camera frame with rounded corners (aspect-fill). In debug
// mode it can draw the face tracker's box, landmarks and head direction, and
// boxes around objects the cloud AI flagged.
class CameraPreview : public QWidget
{
    Q_OBJECT
public:
    explicit CameraPreview(QWidget *parent = nullptr);
    void setFrame(const QImage &frame);
    void setMessage(const QString &message); // shown when there is no frame
    void setOverlay(bool enabled) { m_overlay = enabled; update(); }
    void setFace(const FaceFrame &face, bool looking);
    struct Marked {
        QRectF box; // normalised
        QString label;
    };
    void setMarked(const QList<Marked> &marked) { m_marked = marked; update(); }
    QSize sizeHint() const override { return {360, 240}; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage m_frame;
    QString m_message;
    bool m_overlay = false;
    FaceFrame m_face;
    bool m_looking = true;
    QList<Marked> m_marked;
};

// Horizontal input level meter (microphone).
class LevelMeter : public QWidget
{
    Q_OBJECT
public:
    explicit LevelMeter(QWidget *parent = nullptr);
    void setLevel(qreal level);
    void setSpeaking(bool speaking);
    QSize sizeHint() const override { return {200, 6}; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_level = 0;
    bool m_speaking = false;
};

// Duration field: accepts "5" (minutes), "5:30", "90s", "2m 30s", "1.5", "1h".
// Shows the value as "5 min" or "5:30 min".
class DurationEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit DurationEdit(QWidget *parent = nullptr);
    int seconds() const { return m_seconds; }
    void setSeconds(int seconds);
    void setAllowZero(bool allow) { m_allowZero = allow; } // 0 shows "No limit"
    static int parse(const QString &text, bool *ok);
    static QString format(int seconds, bool allowZero);
    QSize sizeHint() const override;

signals:
    void secondsChanged(int seconds);

protected:
    void focusInEvent(QFocusEvent *event) override;

private:
    void commit();
    int m_seconds = 0;
    bool m_allowZero = false;
};

// A row that expands to show details: title, coloured score bar, rating and
// a chevron. Click the header to open or close it.
class Expander : public QFrame
{
    Q_OBJECT
public:
    Expander(const QString &title, const QString &subtitle, int score, const QColor &colour, const QString &valueText,
             QWidget *parent = nullptr);
    QVBoxLayout *body() const { return m_body; }
    void setExpanded(bool expanded);
    bool isExpanded() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWidget *m_header;
    QWidget *m_content;
    QVBoxLayout *m_body;
    class QLabel *m_chevron;
};

// Big circular score (0..100) with a label underneath the number.
class ScoreRing : public QWidget
{
    Q_OBJECT
public:
    ScoreRing(int score, const QColor &colour, int size, const QString &caption = QString(), QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_score;
    QColor m_colour;
    QString m_caption;
};

// A rounded "card" surface with a vertical layout.
class Card : public QFrame
{
    Q_OBJECT
public:
    explicit Card(QWidget *parent = nullptr);
    explicit Card(const QString &title, QWidget *parent = nullptr);
    QVBoxLayout *body() const { return m_body; }

private:
    QVBoxLayout *m_body;
};

// Title + description on the left, a control (toggle, check...) on the right.
// Clicking anywhere on the row toggles the control.
class OptionRow : public QWidget
{
    Q_OBJECT
public:
    OptionRow(const QString &title, const QString &description, QAbstractButton *control,
              bool controlOnLeft = false, QWidget *parent = nullptr);
    // Any other control (slider, combo box, spin box): clicking the row does nothing
    OptionRow(const QString &title, const QString &description, QWidget *control, QWidget *parent = nullptr);
    QAbstractButton *control() const { return m_control; }

protected:
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QAbstractButton *m_control;
    bool m_hover = false;
};

namespace Ui {
QLabel *label(const QString &text, const char *objectName = nullptr, QWidget *parent = nullptr);
QWidget *pageHeader(const QString &title, const QString &subtitle, QWidget *parent = nullptr);
QScrollArea *scrollPage(QWidget *content);
QFrame *divider(QWidget *parent = nullptr);
} // namespace Ui
