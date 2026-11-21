#pragma once

#include <QJsonObject>
#include <QObject>
#include <QTimer>

#include "companion/CompanionWindow.h"
#include "core/Tip.h"

class QLocalServer;
class QLocalSocket;
class QProcess;

// The main app's handle on the floating avatar. The avatar runs in its own
// small process (SMILE started with --companion), so showing a tip can never
// bring the main SMILE window to the front or steal focus from the meeting.
// If that process can't start, the avatar runs inside the app instead.
class CompanionClient : public QObject
{
    Q_OBJECT

public:
    explicit CompanionClient(QObject *parent = nullptr);
    ~CompanionClient() override;

    bool isSeparateProcess() const { return m_socket != nullptr && !m_local; }
    void setMinimumGap(int milliseconds);

public slots:
    void showTip(const Tip &tip);
    void previewTip(const Tip &tip);
    void dismiss();
    void setSessionActive(bool active);
    void resetPosition();
    void setTimerVisible(bool visible);
    void setTimerInfo(const SectionTimerInfo &info);
    void reloadSettings();
    void close();

signals:
    void openMainWindowRequested();
    void nextPointRequested();
    void endSessionRequested();
    void debugViewRequested();
    void liveTipPresented(const Tip &tip);

private:
    void launch();
    void useInProcess();
    void send(const QJsonObject &message);
    void onReadyRead();
    void replayState();

    QLocalServer *m_server = nullptr;
    QLocalSocket *m_socket = nullptr;
    QProcess *m_process = nullptr;
    CompanionWindow *m_local = nullptr;
    QList<QJsonObject> m_queue;
    QTimer m_connectTimeout;
    QByteArray m_buffer;
    int m_restarts = 0;
    bool m_closing = false;

    // Last known state, replayed if the companion restarts
    bool m_sessionActive = false;
    bool m_timerVisible = false;
    int m_gapMs = 45000;
};
