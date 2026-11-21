#include "companion/CompanionClient.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QRandomGenerator>

#include "companion/CompanionProtocol.h"
#include "core/LiveSignals.h"

namespace {
constexpr int kConnectTimeoutMs = 6000;
constexpr int kMaxRestarts = 3;
} // namespace

CompanionClient::CompanionClient(QObject *parent)
    : QObject(parent)
{
    m_connectTimeout.setSingleShot(true);
    connect(&m_connectTimeout, &QTimer::timeout, this, [this] {
        if (!m_socket) {
            LiveSignals::instance().addEvent(LiveSignals::Event::Info, tr("Companion running inside SMILE"),
                                             tr("The separate avatar process didn't start in time."));
            useInProcess();
        }
    });

    if (qEnvironmentVariableIsSet("SMILE_COMPANION_INPROCESS")) {
        useInProcess();
        return;
    }

    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    const QString name = QStringLiteral("smile-companion-%1-%2")
                             .arg(QCoreApplication::applicationPid())
                             .arg(QRandomGenerator::global()->generate(), 0, 16);
    if (!m_server->listen(name)) {
        useInProcess();
        return;
    }
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        QLocalSocket *socket = m_server->nextPendingConnection();
        if (!socket)
            return;
        if (m_socket)
            m_socket->deleteLater();
        m_socket = socket;
        m_connectTimeout.stop();
        connect(socket, &QLocalSocket::readyRead, this, &CompanionClient::onReadyRead);
        connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
            if (m_socket == socket)
                m_socket = nullptr;
            socket->deleteLater();
        });
        replayState();
        const QList<QJsonObject> queued = m_queue;
        m_queue.clear();
        for (const QJsonObject &m : queued)
            send(m);
    });
    launch();
}

CompanionClient::~CompanionClient()
{
    close();
}

void CompanionClient::close()
{
    if (m_closing)
        return;
    m_closing = true;
    if (m_socket) {
        send({{QStringLiteral("cmd"), QStringLiteral("quit")}});
        m_socket->flush();
        m_socket->waitForBytesWritten(300);
        m_socket->disconnectFromServer();
    }
    if (m_process) {
        if (!m_process->waitForFinished(800))
            m_process->kill();
    }
    if (m_local)
        m_local->close();
}

void CompanionClient::launch()
{
    if (!m_server || m_local)
        return;
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::ForwardedChannels);
    connect(m_process, &QProcess::finished, this, [this] {
        if (m_closing || m_local)
            return;
        m_socket = nullptr;
        if (++m_restarts <= kMaxRestarts) {
            m_process->deleteLater();
            m_process = nullptr;
            launch(); // the avatar crashed or was closed: bring it back
        } else {
            useInProcess();
        }
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart && !m_closing)
            useInProcess();
    });
    // Same executable in companion mode: nothing extra to install or bundle.
    m_process->start(QCoreApplication::applicationFilePath(), {QStringLiteral("--companion"), m_server->fullServerName()});
    m_connectTimeout.start(kConnectTimeoutMs);
}

void CompanionClient::useInProcess()
{
    if (m_local)
        return;
    m_connectTimeout.stop();
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
    }
    m_local = new CompanionWindow;
    connect(this, &QObject::destroyed, m_local, &QObject::deleteLater);
    connect(m_local, &CompanionWindow::openMainWindowRequested, this, &CompanionClient::openMainWindowRequested);
    connect(m_local, &CompanionWindow::nextPointRequested, this, &CompanionClient::nextPointRequested);
    connect(m_local, &CompanionWindow::endSessionRequested, this, &CompanionClient::endSessionRequested);
    connect(m_local, &CompanionWindow::debugViewRequested, this, &CompanionClient::debugViewRequested);
    connect(m_local, &CompanionWindow::liveTipPresented, this, &CompanionClient::liveTipPresented);
    m_local->setMinimumGap(m_gapMs);
    m_local->setSessionActive(m_sessionActive);
    m_local->setTimerVisible(m_timerVisible);
    m_queue.clear();
}

void CompanionClient::send(const QJsonObject &message)
{
    if (m_local)
        return;
    if (!m_socket) {
        m_queue.append(message);
        while (m_queue.size() > 20)
            m_queue.removeFirst();
        return;
    }
    m_socket->write(QJsonDocument(message).toJson(QJsonDocument::Compact) + '\n');
}

void CompanionClient::replayState()
{
    send({{QStringLiteral("cmd"), QStringLiteral("gap")}, {QStringLiteral("ms"), m_gapMs}});
    send({{QStringLiteral("cmd"), QStringLiteral("session")}, {QStringLiteral("active"), m_sessionActive}});
    send({{QStringLiteral("cmd"), QStringLiteral("timerVisible")}, {QStringLiteral("visible"), m_timerVisible}});
}

void CompanionClient::onReadyRead()
{
    if (!m_socket)
        return;
    m_buffer += m_socket->readAll();
    int nl;
    while ((nl = int(m_buffer.indexOf('\n'))) >= 0) {
        const QJsonObject m = QJsonDocument::fromJson(m_buffer.left(nl)).object();
        m_buffer.remove(0, nl + 1);
        const QString evt = m.value(QLatin1String("evt")).toString();
        if (evt == QLatin1String("openMain"))
            emit openMainWindowRequested();
        else if (evt == QLatin1String("nextPoint"))
            emit nextPointRequested();
        else if (evt == QLatin1String("endSession"))
            emit endSessionRequested();
        else if (evt == QLatin1String("debugView"))
            emit debugViewRequested();
        else if (evt == QLatin1String("presented"))
            emit liveTipPresented(CompanionProtocol::tipFromJson(m.value(QLatin1String("tip")).toObject()));
    }
}

// ----------------------------------------------------------------- commands

void CompanionClient::setMinimumGap(int ms)
{
    m_gapMs = ms;
    if (m_local)
        m_local->setMinimumGap(ms);
    else
        send({{QStringLiteral("cmd"), QStringLiteral("gap")}, {QStringLiteral("ms"), ms}});
}

void CompanionClient::showTip(const Tip &tip)
{
    if (m_local)
        m_local->showTip(tip);
    else
        send({{QStringLiteral("cmd"), QStringLiteral("tip")}, {QStringLiteral("tip"), CompanionProtocol::tipToJson(tip)}});
}

void CompanionClient::previewTip(const Tip &tip)
{
    if (m_local)
        m_local->previewTip(tip);
    else
        send({{QStringLiteral("cmd"), QStringLiteral("preview")}, {QStringLiteral("tip"), CompanionProtocol::tipToJson(tip)}});
}

void CompanionClient::dismiss()
{
    if (m_local)
        m_local->dismiss();
    else
        send({{QStringLiteral("cmd"), QStringLiteral("dismiss")}});
}

void CompanionClient::setSessionActive(bool active)
{
    m_sessionActive = active;
    if (m_local)
        m_local->setSessionActive(active);
    else
        send({{QStringLiteral("cmd"), QStringLiteral("session")}, {QStringLiteral("active"), active}});
}

void CompanionClient::resetPosition()
{
    if (m_local)
        m_local->resetPosition();
    else
        send({{QStringLiteral("cmd"), QStringLiteral("resetPosition")}});
}

void CompanionClient::setTimerVisible(bool visible)
{
    m_timerVisible = visible;
    if (m_local)
        m_local->setTimerVisible(visible);
    else
        send({{QStringLiteral("cmd"), QStringLiteral("timerVisible")}, {QStringLiteral("visible"), visible}});
}

void CompanionClient::setTimerInfo(const SectionTimerInfo &info)
{
    if (m_local)
        m_local->setTimerInfo(info);
    else if (m_socket) // timer ticks aren't worth queueing
        send({{QStringLiteral("cmd"), QStringLiteral("timer")}, {QStringLiteral("timer"), CompanionProtocol::timerToJson(info)}});
}

void CompanionClient::reloadSettings()
{
    if (!m_local)
        send({{QStringLiteral("cmd"), QStringLiteral("reload")}});
}
