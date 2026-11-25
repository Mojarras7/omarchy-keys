// keys — Manage Omarchy keybindings with smart bind and unbind recommendations.

#include <QGuiApplication>
#include <QIcon>
#include <QJsonDocument>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

#include "backend.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    // Headless CLI subcommands run offscreen so scripts and agents can query
    // recommendations or active bindings without a display.
    bool cliScan = false;
    bool cliList = false;
    bool demoMode = false;
    QString screenshotPath;
    QString languageOverride;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "scan") cliScan = true;
        else if (arg == "list") cliList = true;
        else if (arg == "--demo") demoMode = true;
        else if (arg == "--screenshot" && i + 1 < argc) screenshotPath = QString::fromLocal8Bit(argv[++i]);
        else if (arg == "--lang-file" && i + 1 < argc) languageOverride = QString::fromLocal8Bit(argv[++i]);
        else if (arg == "--help" || arg == "-h") {
            QTextStream out(stdout);
            out << "Usage: keys [open | scan --json | list --json | --demo]\n";
            return 0;
        }
    }

    if (cliScan || cliList || !screenshotPath.isEmpty())
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    app.setApplicationName("keys");
    app.setApplicationVersion("0.1.0");

    // Matches the window to keys.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("keys");
    app.setWindowIcon(QIcon::fromTheme("keys"));

    if (cliScan || cliList) {
        Backend backend;
        if (demoMode)
            backend.loadDemoData();
        else
            backend.refresh();
        const QJsonObject payload = cliScan ? backend.toScanJson() : backend.toBindingsJson();
        QTextStream out(stdout);
        out << QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Indented));
        return 0;
    }

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator(languageOverride);
    app.installTranslator(&translator);

    // Demo screenshots use the fallback accent so the README image does not
    // depend on the machine's current theme.
    Theme theme(demoMode ? QStringLiteral("/nonexistent/omarchy-demo-theme") : QString());
    Backend backend;
    if (demoMode)
        backend.loadDemoData();
    else
        backend.refresh();

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    if (!screenshotPath.isEmpty()) {
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QTimer::singleShot(200, &app, [window, screenshotPath, &app]() {
            if (window)
                window->grabWindow().save(screenshotPath);
            app.quit();
        });
    }

    return app.exec();
}
