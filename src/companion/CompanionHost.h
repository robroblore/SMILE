#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>

class CompanionWindow;
class QLocalSocket;

// Runs in the companion process: receives commands from the main app over a
// local socket and drives the floating avatar. Quits when the app goes away.
class CompanionHost : public QObject
{
    Q_OBJECT

public:
    explicit CompanionHost(const QString &serverName, QObject *parent = nullptr);

private:
    void onReadyRead();
    void send(const QString &event, const QJsonObject &extra = {});

    QLocalSocket *m_socket;
    CompanionWindow *m_window;
    QByteArray m_buffer;
};

// Entry point for "SMILE --companion <server>"
int runCompanionProcess(int argc, char *argv[]);
