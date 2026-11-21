#include "companion/CompanionHost.h"

#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QSettings>

#include "companion/CompanionProtocol.h"
#include "companion/CompanionWindow.h"
#include "companion/SmileyPainter.h"
#include "ui/Theme.h"

#ifdef Q_OS_MACOS
void smileMakeAccessoryApp(); // MacAccessory.mm: no Dock icon, never activates
#endif

CompanionHost::CompanionHost(const QString &serverName, QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
    , m_window(new CompanionWindow)
{
    connect(m_socket, &QLocalSocket::readyRead, this, &CompanionHost::onReadyRead);
    connect(m_socket, &QLocalSocket::disconnected, qApp, &QCoreApplication::quit);
    connect(m_socket, &QLocalSocket::errorOccurred, qApp, [](QLocalSocket::LocalSocketError) { QCoreApplication::quit(); });

    connect(m_window, &CompanionWindow::openMainWindowRequested, this, [this] { send(QStringLiteral("openMain")); });
    connect(m_window, &CompanionWindow::nextPointRequested, this, [this] { send(QStringLiteral("nextPoint")); });
    connect(m_window, &CompanionWindow::endSessionRequested, this, [this] { send(QStringLiteral("endSession")); });
    connect(m_window, &CompanionWindow::debugViewRequested, this, [this] { send(QStringLiteral("debugView")); });
    connect(m_window, &CompanionWindow::liveTipPresented, this, [this](const Tip &tip) {
        send(QStringLiteral("presented"), {{QStringLiteral("tip"), CompanionProtocol::tipToJson(tip)}});
    });

    m_socket->connectToServer(serverName);
}

void CompanionHost::send(const QString &event, const QJsonObject &extra)
{
    QJsonObject m = extra;
    m.insert(QStringLiteral("evt"), event);
    m_socket->write(QJsonDocument(m).toJson(QJsonDocument::Compact) + '\n');
}

void CompanionHost::onReadyRead()
{
    m_buffer += m_socket->readAll();
    int nl;
    while ((nl = int(m_buffer.indexOf('\n'))) >= 0) {
        const QJsonObject m = QJsonDocument::fromJson(m_buffer.left(nl)).object();
        m_buffer.remove(0, nl + 1);
        const QString cmd = m.value(QLatin1String("cmd")).toString();
        if (cmd == QLatin1String("tip"))
            m_window->showTip(CompanionProtocol::tipFromJson(m.value(QLatin1String("tip")).toObject()));
        else if (cmd == QLatin1String("preview"))
            m_window->previewTip(CompanionProtocol::tipFromJson(m.value(QLatin1String("tip")).toObject()));
        else if (cmd == QLatin1String("dismiss"))
            m_window->dismiss();
        else if (cmd == QLatin1String("session"))
            m_window->setSessionActive(m.value(QLatin1String("active")).toBool());
        else if (cmd == QLatin1String("gap"))
            m_window->setMinimumGap(m.value(QLatin1String("ms")).toInt(45000));
        else if (cmd == QLatin1String("timerVisible"))
            m_window->setTimerVisible(m.value(QLatin1String("visible")).toBool());
        else if (cmd == QLatin1String("timer"))
            m_window->setTimerInfo(CompanionProtocol::timerFromJson(m.value(QLatin1String("timer")).toObject()));
        else if (cmd == QLatin1String("resetPosition"))
            m_window->resetPosition();
        else if (cmd == QLatin1String("reload"))
            QSettings().sync(); // pick up settings changed in the main app (debug mode, theme)
        else if (cmd == QLatin1String("quit"))
            QCoreApplication::quit();
    }
}

int runCompanionProcess(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SMILE"));
    QApplication::setOrganizationName(QStringLiteral("SMILE"));
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setWindowIcon(SmileyPainter::appIcon());
#ifdef Q_OS_MACOS
    smileMakeAccessoryApp();
#endif
    Theme::apply(app);

    QString server;
    const QStringList args = app.arguments();
    const int i = int(args.indexOf(QStringLiteral("--companion")));
    if (i >= 0 && i + 1 < args.size())
        server = args.at(i + 1);
    if (server.isEmpty())
        return 1;
    CompanionHost host(server);
    return app.exec();
}
