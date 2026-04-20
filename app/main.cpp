#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("KNX open Developer Tool"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setOrganizationName(QStringLiteral("OpenKNX"));

    // Qt built-in translations (buttons, dialogs)
    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(), QStringLiteral("qt"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    // Application translations
    QTranslator appTranslator;
    if (appTranslator.load(QLocale(), QStringLiteral("app"), QStringLiteral("_"),
                           QStringLiteral(":/translations"))) {
        app.installTranslator(&appTranslator);
    }

    MainWindow w;
    w.show();

    return app.exec();
}
