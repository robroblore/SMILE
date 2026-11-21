#include <QApplication>

#include "app/MainWindow.h"
#include "companion/SmileyPainter.h"
#include "ui/Theme.h"

int main(int argc, char *argv[])
{
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
