#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QSet>
#include <QTimer>

#include "core/Agenda.h"
#include "core/SessionReport.h"
#include "core/Tip.h"

class TipSource;

// Runs a Training or Meet session: keeps time per agenda point, raises timing
// tips, collects tips from every registered TipSource and produces a
// SessionReport at the end. Holds no UI code.
class SessionController : public QObject
{
    Q_OBJECT

public:
    explicit SessionController(QObject *parent = nullptr);

    // The agenda the user edits on the Training page.
    const Agenda &agenda() const { return m_agenda; }
    void setAgenda(const Agenda &agenda);

    // The agenda of the session in progress (may differ from agenda()).
    const Agenda &activeAgenda() const { return m_active; }

    // The controller takes ownership of the source.
    void addTipSource(TipSource *source);

    // Disabled categories are still logged in the report, just not shown live.
    bool isCategoryShownLive(TipCategory category) const;
    void setCategoryShownLive(TipCategory category, bool shown);

    bool isRunning() const { return m_running; }
    SessionMode mode() const { return m_mode; }
    int currentIndex() const { return m_index; }
    int totalElapsed() const;
    int elapsedOnCurrent() const;

public slots:
    // Speech transcribed while the session runs; used to track key points.
    void addTranscript(const QString &text);
    void start(SessionMode mode);
    void nextPoint();
    void stop();

signals:
    void agendaChanged();
    void runningChanged(bool running);
    void currentPointChanged(int index);
    void ticked();
    void tipRaised(const Tip &tip); // only categories shown live
    void tipLogged(const Tip &tip); // every tip, for the debug view
    void finished(const SessionReport &report);

private:
    void onTick();
    void closeCurrentPoint();
    void beginPoint(int index);
    void raiseTip(const Tip &tip);
    void updateCoverage(int index, bool announce);
    void checkSkippedKeyPoints(int index);

    Agenda m_agenda;
    Agenda m_active;
    QList<TipSource *> m_sources;
    QSet<int> m_hiddenCategories;

    QTimer m_ticker;
    QElapsedTimer m_clock;
    bool m_running = false;
    SessionMode m_mode = SessionMode::Training;
    int m_index = -1;
    int m_pointStartedAt = 0;
    int m_overtimeWarnings = 0;
    bool m_nearEndWarned = false;
    QDateTime m_startedAt;
    QList<Tip> m_log;
    int m_previousIndex = -1;
    qint64 m_switchedAtMs = 0;
    bool m_heardSpeech = false;
};
