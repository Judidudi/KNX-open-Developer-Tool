#include <QApplication>
#include <QTranslator>
#include <QLocale>
#include <QLibraryInfo>
#include <QIcon>

#include "MainWindow.h"

// ETS-inspired stylesheet with orange (#E87722) accent color
static const char *kStyleSheet = R"(
* {
    font-size: 9pt;
}

/* Selection highlight */
QTreeView::item:selected,
QListView::item:selected,
QTableView::item:selected {
    background: #E87722;
    color: white;
}
QTreeView::item:selected:!active,
QListView::item:selected:!active,
QTableView::item:selected:!active {
    background: #f0a060;
    color: white;
}

/* Tab widget */
QTabWidget::pane {
    border: 1px solid #cccccc;
    border-top: 2px solid #E87722;
}
QTabBar::tab {
    background: #e8e8e8;
    border: 1px solid #cccccc;
    border-bottom: none;
    padding: 5px 16px;
    min-width: 60px;
}
QTabBar::tab:selected {
    background: #E87722;
    color: white;
    border-color: #E87722;
}
QTabBar::tab:hover:!selected {
    background: #fad0a8;
    border-color: #E87722;
}

/* Buttons */
QPushButton {
    background: #e8e8e8;
    border: 1px solid #aaaaaa;
    border-radius: 3px;
    padding: 4px 14px;
    min-height: 22px;
}
QPushButton:hover {
    background: #E87722;
    border-color: #c56a1a;
    color: white;
}
QPushButton:pressed {
    background: #c56a1a;
    color: white;
}
QPushButton:default {
    background: #E87722;
    border-color: #c56a1a;
    color: white;
}
QPushButton:disabled {
    background: #eeeeee;
    border-color: #cccccc;
    color: #aaaaaa;
}

/* Toolbar */
QToolBar {
    background: #f0f0f0;
    border-bottom: 1px solid #cccccc;
    spacing: 2px;
    padding: 2px 4px;
}
QToolBar::separator {
    background: #cccccc;
    width: 1px;
    margin: 4px 3px;
}
QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 3px;
    padding: 3px 8px;
}
QToolButton:hover {
    background: #E87722;
    border-color: #c56a1a;
    color: white;
}
QToolButton:pressed {
    background: #c56a1a;
    color: white;
}

/* Menu */
QMenuBar {
    background: #f0f0f0;
    border-bottom: 1px solid #dddddd;
}
QMenuBar::item {
    padding: 4px 10px;
    background: transparent;
}
QMenuBar::item:selected,
QMenuBar::item:pressed {
    background: #E87722;
    color: white;
}
QMenu {
    border: 1px solid #cccccc;
    background: white;
}
QMenu::item {
    padding: 4px 24px 4px 16px;
}
QMenu::item:selected {
    background: #E87722;
    color: white;
}
QMenu::separator {
    height: 1px;
    background: #e0e0e0;
    margin: 2px 8px;
}

/* Table / tree / list headers */
QHeaderView::section {
    background: #eeeeee;
    border: none;
    border-right: 1px solid #cccccc;
    border-bottom: 1px solid #cccccc;
    padding: 3px 6px;
    font-weight: bold;
}
QHeaderView::section:checked {
    background: #E87722;
    color: white;
}

/* Dock widget */
QDockWidget::title {
    background: #e4e4e4;
    padding: 6px 8px;
    font-weight: bold;
    border-bottom: 2px solid #E87722;
    text-align: left;
}
QDockWidget::close-button,
QDockWidget::float-button {
    border: none;
    background: transparent;
    padding: 1px;
}
QDockWidget::close-button:hover,
QDockWidget::float-button:hover {
    background: #E87722;
}

/* Status bar */
QStatusBar {
    background: #2e2e2e;
    color: #e0e0e0;
    border-top: 1px solid #555555;
}
QStatusBar QLabel {
    color: #e0e0e0;
    background: transparent;
    padding: 0 8px;
    border: none;
}
QStatusBar::item {
    border: none;
}

/* Splitter handle */
QSplitter::handle:horizontal {
    background: #d0d0d0;
    width: 2px;
}
QSplitter::handle:vertical {
    background: #d0d0d0;
    height: 2px;
}

/* Line edits and spin boxes */
QLineEdit, QSpinBox, QComboBox {
    border: 1px solid #cccccc;
    border-radius: 2px;
    padding: 2px 4px;
    background: white;
    selection-background-color: #E87722;
    selection-color: white;
}
QLineEdit:focus, QSpinBox:focus, QComboBox:focus {
    border-color: #E87722;
}

/* Scroll bars */
QScrollBar:vertical {
    width: 10px;
    background: #f0f0f0;
    border: none;
}
QScrollBar::handle:vertical {
    background: #c0c0c0;
    border-radius: 4px;
    min-height: 20px;
}
QScrollBar::handle:vertical:hover {
    background: #E87722;
}
QScrollBar::add-line:vertical,
QScrollBar::sub-line:vertical {
    height: 0;
}
QScrollBar:horizontal {
    height: 10px;
    background: #f0f0f0;
    border: none;
}
QScrollBar::handle:horizontal {
    background: #c0c0c0;
    border-radius: 4px;
    min-width: 20px;
}
QScrollBar::handle:horizontal:hover {
    background: #E87722;
}
QScrollBar::add-line:horizontal,
QScrollBar::sub-line:horizontal {
    width: 0;
}

/* CheckBox */
QCheckBox::indicator:checked {
    background: #E87722;
    border: 1px solid #c56a1a;
}
QCheckBox::indicator:unchecked {
    background: white;
    border: 1px solid #aaaaaa;
}
)";

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("KNX open Developer Tool"));
    app.setApplicationVersion(QStringLiteral("0.1.0"));
    app.setOrganizationName(QStringLiteral("OpenKNX"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/knxodt.svg")));

    app.setStyleSheet(QString::fromLatin1(kStyleSheet));

    // Qt built-in translations (buttons, standard dialogs)
    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(), QStringLiteral("qt"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    // Application translations (embedded in :/translations/)
    QTranslator appTranslator;
    if (appTranslator.load(QLocale(), QStringLiteral("app"), QStringLiteral("_"),
                           QStringLiteral(":/translations"))) {
        app.installTranslator(&appTranslator);
    }

    MainWindow w;
    w.show();

    return app.exec();
}
