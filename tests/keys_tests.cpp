#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "theme.h"
#include "translator.h"

class KeysTests : public QObject {
    Q_OBJECT

private slots:
    void normalizesCombos()
    {
        QCOMPARE(Backend::normalizeCombo("shift + super + z"), QString("SUPER + SHIFT + Z"));
        QCOMPARE(Backend::normalizeCombo("mod4 + alt + ctrl + b"), QString("SUPER + CTRL + ALT + B"));
    }

    void managedBlockRoundTripPreservesUserLines()
    {
        const QString userLua = "-- Personal comment\no.bind(\"SUPER + H\", \"Voice\", \"voxtype\")\n";
        ManagedState state;
        state.unbinds << "SUPER + SHIFT + X";
        ManagedBind b;
        b.combo = "SUPER + SHIFT + Z";
        b.description = "Zen Browser";
        b.command = "zen-browser";
        b.isLaunch = true;
        b.isRebind = false;
        state.binds << b;

        const QString written = Backend::writeManagedBlock(userLua, state);
        QVERIFY(written.contains("Personal comment"));
        QVERIFY(written.contains("voxtype"));
        QVERIFY(written.contains("hl.unbind(\"SUPER + SHIFT + X\")"));
        QVERIFY(written.contains("o.rebind(\"SUPER + SHIFT + Z\", \"Zen Browser\", { launch = \"zen-browser\" })"));

        const ManagedState parsed = Backend::parseManagedBlock(written);
        QCOMPARE(parsed.unbinds, state.unbinds);
        QCOMPARE(parsed.binds.size(), 1);
        QCOMPARE(parsed.binds.first().combo, state.binds.first().combo);
        QCOMPARE(parsed.binds.first().command, state.binds.first().command);
        QCOMPARE(parsed.binds.first().isRebind, true);
    }

    void parsesJournalUsageAndRunningClasses()
    {
        const QString journal =
            "Nov 24 10:00:00 host systemd[1000]: Started app-md.obsidian.Obsidian-1234.scope.\n"
            "Nov 24 10:05:00 host systemd[1000]: app-Hyprland-zen\\x2dbin-a1b2c3d4.scope: Consumed 5s CPU time.\n"
            "Nov 24 10:10:00 host systemd[1000]: app-Hyprland-zen\\x2dbin-e5f6a7b8.scope: Consumed 4s CPU time.\n"
            "Nov 24 10:12:00 host systemd[1000]: app-Hyprland-xdg\\x2dterminal\\x2dexec-11112222.scope: Consumed 2s CPU time.\n";
        const auto counts = Backend::parseJournalUsage(journal);
        QCOMPARE(counts.value("obsidian"), 1);
        QCOMPARE(counts.value("zen"), 2);
        QVERIFY(!counts.contains("xdg-terminal-exec"));

        const QByteArray clients = R"([{"class":"zen-bin","initialClass":"zen-bin"},{"class":"foot","initialClass":"foot"}])";
        const auto running = Backend::parseRunningClasses(clients);
        QVERIFY(running.contains("zen"));
        QVERIFY(running.contains("foot"));
    }

    void suggestsFreeComboAvoidingConflicts()
    {
        QSet<QString> used{"SUPER + SHIFT + Z", "SUPER + SHIFT + E"};
        QCOMPARE(Backend::suggestComboForName("Zen Browser", used), QString("SUPER + SHIFT + N"));
    }

    void bindUnbindAndUndoUpdateLuaFile()
    {
        QTemporaryDir dir;
        const QString bindingsLua = dir.filePath("bindings.lua");
        const QString defaultApps = dir.filePath("applications.lua");
        write(bindingsLua, "-- User file\n");
        write(defaultApps,
              "o.bind(\"SUPER + SHIFT + X\", \"X\", { webapp = \"https://x.com/\" })\n"
              "o.bind(\"SUPER + RETURN\", \"Terminal\", o.launch_terminal())\n");

        Backend::Config cfg;
        cfg.bindingsLuaPath = bindingsLua;
        cfg.defaultAppsLuaPath = defaultApps;
        cfg.desktopDirs = {dir.path()};
        cfg.skipSystemCommands = true;

        Backend backend(cfg);
        backend.refresh();
        QCOMPARE(backend.unbindRecommendations().size(), 1);
        QCOMPARE(backend.unbindRecommendations().first().toMap().value("combo").toString(),
                 QString("SUPER + SHIFT + X"));

        backend.unbindKey("SUPER + SHIFT + X");
        QCOMPARE(backend.unbindRecommendations().size(), 0);
        QCOMPARE(backend.unboundOverrides().size(), 1);
        QVERIFY(backend.canUndo());

        backend.bindApp("SUPER + SHIFT + Z", "Zen Browser", "zen-browser", true);
        QCOMPARE(backend.findConflict("SUPER + SHIFT + Z"), QString("Zen Browser"));

        backend.undo();
        QVERIFY(backend.findConflict("SUPER + SHIFT + Z").isEmpty());
        QVERIFY(backend.canRedo());

        backend.redo();
        QCOMPARE(backend.findConflict("SUPER + SHIFT + Z"), QString("Zen Browser"));
    }

    void listsAllInstalledAppsForSearch()
    {
        QTemporaryDir dir;
        const QString bindingsLua = dir.filePath("bindings.lua");
        write(bindingsLua, "");
        write(dir.filePath("gimp.desktop"),
              "[Desktop Entry]\nType=Application\nName=GIMP\nExec=gimp %U\n");

        Backend::Config cfg;
        cfg.bindingsLuaPath = bindingsLua;
        cfg.defaultAppsLuaPath = dir.filePath("missing.lua");
        cfg.desktopDirs = {dir.path()};
        cfg.skipSystemCommands = true;

        Backend backend(cfg);
        backend.refresh();
        QCOMPARE(backend.allApps().size(), 1);
        const QVariantMap gimp = backend.allApps().first().toMap();
        QCOMPARE(gimp.value("name").toString(), QString("GIMP"));
        QCOMPARE(gimp.value("command").toString(), QString("gimp"));
        QCOMPARE(gimp.value("suggestedCombo").toString(), QString("SUPER + SHIFT + G"));
    }

    void unbindingTerminalClearsBoundStateInAllApps()
    {
        QTemporaryDir dir;
        const QString bindingsLua = dir.filePath("bindings.lua");
        const QString defaultApps = dir.filePath("applications.lua");
        write(bindingsLua, "");
        write(defaultApps, "o.bind(\"SUPER + RETURN\", \"Terminal\", o.launch_terminal())\n");
        write(dir.path() + "/foot.desktop",
              "[Desktop Entry]\nType=Application\nName=Foot\nExec=foot\n");

        Backend::Config cfg;
        cfg.bindingsLuaPath = bindingsLua;
        cfg.defaultAppsLuaPath = defaultApps;
        cfg.desktopDirs = {dir.path()};
        cfg.skipSystemCommands = true;

        Backend backend(cfg);
        backend.refresh();
        QCOMPARE(backend.allApps().first().toMap().value("isBound").toBool(), true);

        backend.unbindKey("SUPER + RETURN");
        QCOMPARE(backend.allApps().first().toMap().value("isBound").toBool(), false);
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"#112233\"\n");

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsLanguageAndPlurals()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "da\n");
        write(dir.path() + "/en.json", R"({"%n bindings": {"one": "%n binding", "other": "%n bindings"}})");
        write(dir.path() + "/da.json", R"({"Save": "Gem", "%n bindings": {"one": "%n genvej", "other": "%n genveje"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("da"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Gem"));
        QCOMPARE(translator.translate("Main", "%n bindings", nullptr, 1), QString("%n genvej"));
        QCOMPARE(translator.translate("Main", "%n bindings", nullptr, 5), QString("%n genveje"));
    }

private:
    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(KeysTests)
#include "keys_tests.moc"
