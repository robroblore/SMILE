#include <QApplication>

#include "app/MainWindow.h"
#include "companion/CompanionHost.h"
#include "companion/SmileyPainter.h"
#include "ui/Theme.h"

int main(int argc, char *argv[])
{
    // "SMILE --companion <server>": run only the floating avatar, in its own
    // process, so it can never pull the main window in front of a meeting.
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--companion") == 0)
            return runCompanionProcess(argc, argv);
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("SMILE"));
    QApplication::setApplicationDisplayName(QStringLiteral("SMILE"));
    QApplication::setOrganizationName(QStringLiteral("SMILE"));
    QApplication::setApplicationVersion(QStringLiteral(SMILE_VERSION));
    QApplication::setWindowIcon(SmileyPainter::appIcon());
    QApplication::setQuitOnLastWindowClosed(false); // the tray icon keeps running during meetings
    Theme::apply(app);

    MainWindow window;
    window.show();

    return app.exec();
}
