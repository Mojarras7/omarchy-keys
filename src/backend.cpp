#include "backend.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>

namespace {

const char *kMarkerStart = "-- >>> keys-managed (do not edit between markers)";
const char *kMarkerEnd = "-- <<< keys-managed";

// Essential system/window-manager shortcuts that should never be recommended
// for unbinding even if they don't appear in systemd app scopes.
bool isCoreBinding(const QString &description, const QString &combo) {
    const QString d = description.toLower();
    static const QStringList protectedKeywords = {
        "terminal", "browser", "file manager", "editor", "keybindings",
        "omarchy menu", "system menu", "theme menu", "full screen", "full width",
        "close window", "close all", "lock system", "floating", "split",
        "pop window", "universal copy", "universal paste", "universal cut",
        "clipboard", "screenshot", "screenrecord", "color picker", "volume",
        "brightness", "workspace", "focus", "swap", "move window", "resize",
        "emojis", "notification", "dictation", "night light", "tmux", "herdr"
    };
    for (const auto &kw : protectedKeywords) {
        if (d.contains(kw))
            return true;
    }
    if (combo.contains("PRINT") || combo.contains("XF86") || combo.contains("RETURN"))
        return true;
    return false;
}

// Map common reverse-DNS app IDs and WM classes to canonical app tokens so
// journal scopes, Hyprland clients, and .desktop entries line up reliably.
QString canonicalAppToken(const QString &raw) {
    const QString t = Backend::normalizeToken(raw);
    if (t.isEmpty())
        return {};
    static const QHash<QString, QString> aliases = {
        {"org.chromium.chromium", "chromium"},
        {"chromium-browser", "chromium"},
        {"com.google.chrome", "google-chrome"},
        {"google-chrome-stable", "google-chrome"},
        {"md.obsidian.obsidian", "obsidian"},
        {"zen-bin", "zen"},
        {"zen-browser", "zen"},
        {"app.zen_browser.zen", "zen"},
        {"org.gnome.nautilus", "nautilus"},
        {"org.mozilla.firefox", "firefox"},
        {"code-oss", "code"},
        {"visual-studio-code", "code"},
        {"com.mitchellh.ghostty", "ghostty"},
        {"signal-desktop", "signal"},
        {"1password", "1password"},
        {"spotify-launcher", "spotify"},
        {"discord-canary", "discord"},
        {"com.discordapp.discord", "discord"},
        {"org.telegram.desktop", "telegram"},
        {"com.slack.slack", "slack"},
        {"org.videolan.vlc", "vlc"},
        {"com.obsproject.studio", "obs"},
    };
    if (aliases.contains(t))
        return aliases.value(t);
    // For reverse-DNS IDs like "org.foo.BarApp", also check the trailing segment.
    if (t.count('.') >= 2) {
        const QString tail = t.section('.', -1);
        if (aliases.contains(tail))
            return aliases.value(tail);
        return tail;
    }
    return t;
}

// Ignore systemd plumbing and launcher wrappers when counting real apps.
bool isNoiseScope(const QString &token) {
    static const QSet<QString> noise = {
        "xdg-terminal-exec", "gtk-launch", "uwsm-app", "omarchy-hyprland-monitor-watch",
        "omarchy-launch-browser", "omarchy-launch-terminal", "omarchy-launch-webapp",
        "omarchy-menu", "omarchy-shell", "hyprland", "dbus", "pipewire",
        "wireplumber", "podman-pause", "xdg-desktop-portal", "polkit", "udiskie",
        "fcitx5", "gnome-keyring", "keys"
    };
    if (noise.contains(token))
        return true;
    if (token.startsWith("omarchy-") || token.startsWith("podman-") || token.startsWith("dbus-"))
        return true;
    return false;
}

bool binaryExistsOnPath(const QString &binary) {
    if (binary.isEmpty())
        return false;
    if (QFileInfo::exists(binary) && QFileInfo(binary).isExecutable())
        return true;
    return !QStandardPaths::findExecutable(binary).isEmpty();
}

} // namespace

Backend::Backend(QObject *parent)
    : Backend(defaultConfig(), parent) {}

Backend::Backend(const Config &config, QObject *parent)
    : QObject(parent), m_config(config) {
    if (m_config.bindingsLuaPath.isEmpty())
        m_config.bindingsLuaPath = QDir::homePath() + "/.config/hypr/bindings.lua";
    if (m_config.defaultAppsLuaPath.isEmpty())
        m_config.defaultAppsLuaPath = "/usr/share/omarchy/default/hypr/bindings/applications.lua";
    if (m_config.desktopDirs.isEmpty()) {
        m_config.desktopDirs = {
            QDir::homePath() + "/.local/share/applications",
            "/usr/share/applications"
        };
    }
    if (!m_config.skipSystemCommands) {
        if (m_config.hyprctlPath.isEmpty())
            m_config.hyprctlPath = QStandardPaths::findExecutable("hyprctl");
        if (m_config.journalctlPath.isEmpty())
            m_config.journalctlPath = QStandardPaths::findExecutable("journalctl");
        if (m_config.keybindingsCmdPath.isEmpty())
            m_config.keybindingsCmdPath = QStandardPaths::findExecutable("omarchy");
    }
}

Backend::Config Backend::defaultConfig() const {
    return Config{};
}

QString Backend::normalizeToken(const QString &raw) {
    QString s = raw.trimmed().toLower();
    if (s.endsWith(".desktop"))
        s.chop(8);
    if (s.endsWith(".scope"))
        s.chop(6);
    return s;
}

QString Backend::normalizeCombo(const QString &combo) {
    QStringList parts = combo.split(QRegularExpression(R"(\s*\+\s*|\s+)"), Qt::SkipEmptyParts);
    QStringList mods;
    QString key;
    for (QString part : parts) {
        part = part.trimmed().toUpper();
        if (part == "SUPER" || part == "MOD4" || part == "META") {
            if (!mods.contains("SUPER")) mods << "SUPER";
        } else if (part == "CTRL" || part == "CONTROL") {
            if (!mods.contains("CTRL")) mods << "CTRL";
        } else if (part == "ALT" || part == "MOD1") {
            if (!mods.contains("ALT")) mods << "ALT";
        } else if (part == "SHIFT") {
            if (!mods.contains("SHIFT")) mods << "SHIFT";
        } else {
            key = part;
        }
    }
    // Canonical modifier order matches Omarchy's bindings.lua style.
    QStringList ordered;
    for (const char *m : {"SUPER", "SHIFT", "CTRL", "ALT"}) {
        if (mods.contains(QLatin1String(m)))
            ordered << QLatin1String(m);
    }
    if (!key.isEmpty())
        ordered << key;
    return ordered.join(" + ");
}

QString Backend::luaEscape(const QString &text) {
    QString out = text;
    out.replace('\\', "\\\\");
    out.replace('"', "\\\"");
    out.replace('\n', "\\n");
    return out;
}

QString Backend::unescapeSystemdName(const QString &escaped) {
    QString out = escaped;
    static const QRegularExpression hexRe(R"(\\x([0-9a-fA-F]{2}))");
    QRegularExpressionMatchIterator it = hexRe.globalMatch(out);
    QList<QPair<qsizetype, QPair<qsizetype, QChar>>> replacements;
    while (it.hasNext()) {
        const auto m = it.next();
        bool ok = false;
        const int code = m.captured(1).toInt(&ok, 16);
        if (ok)
            replacements.append({m.capturedStart(), {m.capturedLength(), QChar(code)}});
    }
    for (int i = replacements.size() - 1; i >= 0; --i) {
        out.replace(replacements[i].first, replacements[i].second.first, replacements[i].second.second);
    }
    return out;
}

ManagedState Backend::parseManagedBlock(const QString &luaContent) {
    ManagedState state;
    const int startIdx = luaContent.indexOf(kMarkerStart);
    const int endIdx = luaContent.indexOf(kMarkerEnd);
    if (startIdx < 0 || endIdx <= startIdx)
        return state;

    const QString block = luaContent.mid(startIdx, endIdx - startIdx);
    static const QRegularExpression unbindRe(R"re(hl\.unbind\("([^"]+)"\))re");
    static const QRegularExpression bindLaunchRe(
        R"re(o\.(bind|rebind)\("([^"]+)",\s*"([^"]*)",\s*\{\s*launch\s*=\s*"([^"]+)"\s*\}\))re");
    static const QRegularExpression bindCmdRe(
        R"re(o\.(bind|rebind)\("([^"]+)",\s*"([^"]*)",\s*"([^"]+)"\))re");

    const QStringList lines = block.split('\n');
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (auto m = unbindRe.match(line); m.hasMatch()) {
            const QString c = normalizeCombo(m.captured(1));
            if (!state.unbinds.contains(c))
                state.unbinds.append(c);
        } else if (auto m = bindLaunchRe.match(line); m.hasMatch()) {
            ManagedBind b;
            b.isRebind = (m.captured(1) == "rebind");
            b.combo = normalizeCombo(m.captured(2));
            b.description = m.captured(3);
            b.command = m.captured(4);
            b.isLaunch = true;
            state.binds.append(b);
        } else if (auto m = bindCmdRe.match(line); m.hasMatch()) {
            ManagedBind b;
            b.isRebind = (m.captured(1) == "rebind");
            b.combo = normalizeCombo(m.captured(2));
            b.description = m.captured(3);
            b.command = m.captured(4);
            b.isLaunch = false;
            state.binds.append(b);
        }
    }
    return state;
}

QString Backend::writeManagedBlock(const QString &luaContent, const ManagedState &state) {
    QString base = luaContent;
    const int startIdx = base.indexOf(kMarkerStart);
    const int endIdx = base.indexOf(kMarkerEnd);
    if (startIdx >= 0 && endIdx > startIdx) {
        int removeStart = startIdx;
        while (removeStart > 0 && base.at(removeStart - 1) == '\n')
            --removeStart;
        int removeEnd = endIdx + int(strlen(kMarkerEnd));
        if (removeEnd < base.size() && base.at(removeEnd) == '\n')
            ++removeEnd;
        base.remove(removeStart, removeEnd - removeStart);
    }
    base = base.trimmed();

    if (state.unbinds.isEmpty() && state.binds.isEmpty())
        return base.isEmpty() ? QString() : base + "\n";

    QStringList blockLines;
    blockLines << kMarkerStart;
    for (const QString &combo : state.unbinds) {
        blockLines << QStringLiteral("hl.unbind(\"%1\")").arg(luaEscape(combo));
    }
    for (const ManagedBind &b : state.binds) {
        const QString fn = b.isRebind ? "o.rebind" : "o.bind";
        if (b.isLaunch) {
            blockLines << QStringLiteral("%1(\"%2\", \"%3\", { launch = \"%4\" })")
                              .arg(fn, luaEscape(b.combo), luaEscape(b.description), luaEscape(b.command));
        } else {
            blockLines << QStringLiteral("%1(\"%2\", \"%3\", \"%4\")")
                              .arg(fn, luaEscape(b.combo), luaEscape(b.description), luaEscape(b.command));
        }
    }
    blockLines << kMarkerEnd;

    if (base.isEmpty())
        return blockLines.join('\n') + "\n";
    return base + "\n\n" + blockLines.join('\n') + "\n";
}

QHash<QString, int> Backend::parseJournalUsage(const QString &journalText) {
    QHash<QString, int> counts;
    static const QRegularExpression hyprScopeRe(R"(app-Hyprland-([^\s:]+)-[0-9a-fA-F]+\.scope)");
    static const QRegularExpression appScopeRe(R"(Started app-([^\s:]+)-[0-9]+\.scope)");

    const QStringList lines = journalText.split('\n');
    for (const QString &line : lines) {
        QString rawName;
        if (auto m = hyprScopeRe.match(line); m.hasMatch()) {
            // Count only the completion or start once per scope by matching "Consumed"
            // or "Started" lines so a single launch isn't double-counted.
            if (!line.contains("Consumed") && !line.contains("Started"))
                continue;
            rawName = unescapeSystemdName(m.captured(1));
        } else if (auto m = appScopeRe.match(line); m.hasMatch()) {
            rawName = unescapeSystemdName(m.captured(1));
        } else {
            continue;
        }
        const QString token = canonicalAppToken(rawName);
        if (token.isEmpty() || isNoiseScope(token))
            continue;
        counts[token] = counts.value(token, 0) + 1;
    }
    return counts;
}

QSet<QString> Backend::parseRunningClasses(const QByteArray &hyprClientsJson) {
    QSet<QString> classes;
    const QJsonDocument doc = QJsonDocument::fromJson(hyprClientsJson);
    if (!doc.isArray())
        return classes;
    for (const QJsonValue &val : doc.array()) {
        const QJsonObject obj = val.toObject();
        for (const char *field : {"class", "initialClass"}) {
            const QString token = canonicalAppToken(obj.value(QLatin1String(field)).toString());
            if (!token.isEmpty() && !isNoiseScope(token))
                classes.insert(token);
        }
    }
    return classes;
}

QString Backend::suggestComboForName(const QString &appName, const QSet<QString> &usedCombos) {
    QString letters;
    for (const QChar &ch : appName.toUpper()) {
        if (ch >= 'A' && ch <= 'Z')
            letters.append(ch);
    }
    if (letters.isEmpty())
        letters = "K";

    const QStringList prefixes = {"SUPER + SHIFT + ", "SUPER + ALT + ", "SUPER + CTRL + SHIFT + "};
    for (const QString &prefix : prefixes) {
        for (const QChar &ch : letters) {
            const QString candidate = normalizeCombo(prefix + ch);
            if (!usedCombos.contains(candidate))
                return candidate;
        }
    }
    // Fall back to scanning A-Z if every letter in the app name is taken.
    for (const QString &prefix : prefixes) {
        for (char c = 'A'; c <= 'Z'; ++c) {
            const QString candidate = normalizeCombo(prefix + QChar(c));
            if (!usedCombos.contains(candidate))
                return candidate;
        }
    }
    return "SUPER + ALT + SHIFT + K";
}

QString Backend::runCommand(const QString &program, const QStringList &args, int timeoutMs,
                            bool *ok) const {
    if (ok) *ok = false;
    if (program.isEmpty())
        return {};
    QProcess proc;
    proc.start(program, args);
    if (!proc.waitForStarted(1500))
        return {};
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(500);
        return {};
    }
    if (ok) *ok = (proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0);
    return QString::fromUtf8(proc.readAllStandardOutput());
}

QList<DesktopApp> Backend::scanDesktopApps() const {
    QHash<QString, DesktopApp> byId;
    // Scan in reverse priority so user ~/.local/share/applications overrides /usr/share/applications.
    for (int i = m_config.desktopDirs.size() - 1; i >= 0; --i) {
        QDir dir(m_config.desktopDirs.at(i));
        if (!dir.exists())
            continue;
        const QStringList files = dir.entryList({"*.desktop"}, QDir::Files);
        for (const QString &fileName : files) {
            QFile file(dir.filePath(fileName));
            if (!file.open(QIODevice::ReadOnly))
                continue;
            bool inEntry = false;
            bool hidden = false;
            bool isApp = false;
            DesktopApp app;
            app.id = normalizeToken(fileName);
            while (!file.atEnd()) {
                const QString line = QString::fromUtf8(file.readLine()).trimmed();
                if (line.isEmpty() || line.startsWith('#'))
                    continue;
                if (line.startsWith('[')) {
                    inEntry = (line == "[Desktop Entry]");
                    continue;
                }
                if (!inEntry)
                    continue;
                const int eq = line.indexOf('=');
                if (eq <= 0)
                    continue;
                const QString key = line.left(eq).trimmed();
                const QString val = line.mid(eq + 1).trimmed();
                if (key == "Type") isApp = (val == "Application");
                else if (key == "NoDisplay" || key == "Hidden") {
                    if (val.compare("true", Qt::CaseInsensitive) == 0)
                        hidden = true;
                } else if (key == "Name" && app.name.isEmpty()) app.name = val;
                else if (key == "Exec" && app.exec.isEmpty()) app.exec = val;
                else if (key == "Icon" && app.icon.isEmpty()) app.icon = val;
                else if (key == "StartupWMClass" && app.wmClass.isEmpty()) app.wmClass = val;
            }
            if (!isApp || hidden || app.name.isEmpty() || app.exec.isEmpty())
                continue;
            if (app.id == "keys")
                continue;

            // Strip freedesktop field codes (%u, %U, %f, %F, etc.) from Exec.
            QString cleaned = app.exec;
            cleaned.remove(QRegularExpression(R"(\s+%[a-zA-Z])"));
            cleaned = cleaned.trimmed();
            app.exec = cleaned;
            app.isWebapp = cleaned.contains("omarchy-launch-webapp") || cleaned.contains("--app=");

            QString firstToken = cleaned.section(' ', 0, 0);
            firstToken.remove('"');
            if (firstToken == "env" || firstToken == "uwsm-app")
                firstToken = cleaned.section(' ', 1, 1);
            app.binary = QFileInfo(firstToken).fileName();
            byId.insert(app.id, app);
        }
    }
    return byId.values();
}

QHash<QString, QPair<QString, bool>> Backend::parseDefaultAppBindings() const {
    // Maps normalized combo -> { appKey/binary, isWebapp }
    QHash<QString, QPair<QString, bool>> map;
    QFile file(m_config.defaultAppsLuaPath);
    if (!file.open(QIODevice::ReadOnly))
        return map;

    const QString text = QString::fromUtf8(file.readAll());
    static const QRegularExpression bindRe(
        R"re(o\.bind\(\s*"([^"]+)"\s*,\s*"([^"]*)"\s*,\s*(.+)\))re");
    static const QRegularExpression omarchyRe(R"re(omarchy\s*=\s*"([^"]+)")re");
    static const QRegularExpression launchRe(R"re(launch\s*=\s*"([^"]+)")re");
    static const QRegularExpression tuiRe(R"re(tui\s*=\s*"([^"]+)")re");
    static const QRegularExpression webappRe(R"re(webapp\s*=\s*"([^"]+)")re");
    static const QRegularExpression plainCmdRe(R"re(^"([^"]+)")re");

    for (const QString &rawLine : text.split('\n')) {
        const QString line = rawLine.trimmed();
        if (line.startsWith("--"))
            continue;
        auto m = bindRe.match(line);
        if (!m.hasMatch())
            continue;
        const QString combo = normalizeCombo(m.captured(1));
        const QString target = m.captured(3).trimmed();
        if (auto w = webappRe.match(target); w.hasMatch()) {
            map.insert(combo, {m.captured(2).toLower(), true});
        } else if (auto o = omarchyRe.match(target); o.hasMatch()) {
            map.insert(combo, {o.captured(1).section(' ', 0, 0).toLower(), false});
        } else if (auto l = launchRe.match(target); l.hasMatch()) {
            map.insert(combo, {l.captured(1).section(' ', 0, 0).toLower(), false});
        } else if (auto t = tuiRe.match(target); t.hasMatch()) {
            map.insert(combo, {t.captured(1).section(' ', 0, 0).toLower(), false});
        } else if (auto p = plainCmdRe.match(target); p.hasMatch()) {
            map.insert(combo, {p.captured(1).section(' ', 0, 0).toLower(), false});
        }
    }
    return map;
}

QList<BindingEntry> Backend::loadActiveBindings(const ManagedState &managed) const {
    QList<BindingEntry> entries;
    const auto defaultApps = parseDefaultAppBindings();

    QHash<QString, ManagedBind> customByCombo;
    for (const auto &b : managed.binds)
        customByCombo.insert(b.combo, b);

    bool ok = false;
    QString output;
    if (!m_config.skipSystemCommands && !m_config.keybindingsCmdPath.isEmpty()) {
        output = runCommand(m_config.keybindingsCmdPath, {"menu", "keybindings", "--print"}, 4000, &ok);
    }

    if (ok && !output.trimmed().isEmpty()) {
        QSet<QString> seen;
        for (const QString &rawLine : output.split('\n')) {
            const QString line = rawLine.trimmed();
            const int arrow = line.indexOf(QString::fromUtf8("→"));
            if (arrow <= 0)
                continue;
            const QString left = line.left(arrow).trimmed();
            const QString desc = line.mid(arrow + 1).trimmed();
            // Some rows join aliases with " / " (e.g. "SUPER + W / SUPER + Q").
            const QString primaryCombo = normalizeCombo(left.section(" / ", 0, 0));
            if (primaryCombo.isEmpty() || seen.contains(primaryCombo))
                continue;
            seen.insert(primaryCombo);

            BindingEntry entry;
            entry.combo = primaryCombo;
            entry.description = desc;
            entry.isProtected = isCoreBinding(desc, primaryCombo);
            if (customByCombo.contains(primaryCombo)) {
                entry.isCustom = true;
                entry.isAppBinding = true;
                entry.arg = customByCombo.value(primaryCombo).command;
                entry.appKey = canonicalAppToken(entry.arg.section(' ', 0, 0));
            } else if (defaultApps.contains(primaryCombo)) {
                entry.isAppBinding = true;
                entry.appKey = canonicalAppToken(defaultApps.value(primaryCombo).first);
                entry.isWebapp = defaultApps.value(primaryCombo).second;
            }
            entries.append(entry);
        }
    } else {
        // Fallback when running in tests or without Hyprland: synthesize from
        // default applications.lua + managed overrides.
        QSet<QString> unboundSet(managed.unbinds.begin(), managed.unbinds.end());
        QFile file(m_config.defaultAppsLuaPath);
        if (file.open(QIODevice::ReadOnly)) {
            static const QRegularExpression bindRe(R"re(o\.bind\(\s*"([^"]+)"\s*,\s*"([^"]*)")re");
            for (const QString &line : QString::fromUtf8(file.readAll()).split('\n')) {
                auto m = bindRe.match(line.trimmed());
                if (!m.hasMatch())
                    continue;
                const QString combo = normalizeCombo(m.captured(1));
                if (unboundSet.contains(combo) || customByCombo.contains(combo))
                    continue;
                BindingEntry entry;
                entry.combo = combo;
                entry.description = m.captured(2);
                entry.isProtected = isCoreBinding(entry.description, combo);
                if (defaultApps.contains(combo)) {
                    entry.isAppBinding = true;
                    entry.appKey = canonicalAppToken(defaultApps.value(combo).first);
                    entry.isWebapp = defaultApps.value(combo).second;
                }
                entries.append(entry);
            }
        }
        for (const auto &b : managed.binds) {
            BindingEntry entry;
            entry.combo = b.combo;
            entry.description = b.description;
            entry.arg = b.command;
            entry.appKey = canonicalAppToken(b.command.section(' ', 0, 0));
            entry.isAppBinding = true;
            entry.isCustom = true;
            entry.isProtected = isCoreBinding(entry.description, entry.combo);
            entries.append(entry);
        }
    }
    return entries;
}

void Backend::computeRecommendations(QList<BindingEntry> &entries, QList<DesktopApp> &apps,
                                     const QHash<QString, int> &usageCounts,
                                     const QSet<QString> &runningClasses) {
    QSet<QString> usedCombos;
    QHash<QString, QString> boundTokenToCombo;

    // Map known Omarchy launcher aliases to real binary names for install checks.
    static const QHash<QString, QString> omarchyBinaryMap = {
        {"spotify", "spotify"},
        {"signal", "signal-desktop"},
        {"1password", "1password"},
        {"obsidian", "obsidian"},
        {"omawrite", "omawrite"},
        {"omacalc", "omacalc"},
        {"cliamp", "cliamp"},
        {"omarchy-launch-docker-tui", "lazydocker"},
    };

    for (BindingEntry &e : entries) {
        usedCombos.insert(e.combo);
        const QString descToken = canonicalAppToken(e.description);
        if (!e.appKey.isEmpty())
            boundTokenToCombo.insert(e.appKey, e.combo);
        if (!descToken.isEmpty())
            boundTokenToCombo.insert(descToken, e.combo);

        int count = 0;
        if (!e.appKey.isEmpty())
            count = qMax(count, usageCounts.value(e.appKey, 0));
        if (!descToken.isEmpty())
            count = qMax(count, usageCounts.value(descToken, 0));
        e.launches = count;
        e.isRunning = (!e.appKey.isEmpty() && runningClasses.contains(e.appKey)) ||
                      (!descToken.isEmpty() && runningClasses.contains(descToken));

        if (e.isAppBinding && !e.isWebapp && !e.isProtected && !e.appKey.isEmpty()) {
            const QString bin = omarchyBinaryMap.value(e.appKey, e.appKey);
            if (!m_config.skipSystemCommands)
                e.isInstalled = binaryExistsOnPath(bin);
        }
    }

    // Score each desktop app and determine whether it's already bound.
    for (DesktopApp &app : apps) {
        const QStringList keys = {
            canonicalAppToken(app.id),
            canonicalAppToken(app.wmClass),
            canonicalAppToken(app.binary),
            canonicalAppToken(app.name)
        };
        int count = 0;
        bool running = false;
        for (const QString &k : keys) {
            if (k.isEmpty())
                continue;
            count = qMax(count, usageCounts.value(k, 0));
            if (runningClasses.contains(k))
                running = true;
            if (!app.isBound && boundTokenToCombo.contains(k)) {
                app.isBound = true;
                app.boundCombo = boundTokenToCombo.value(k);
            }
        }
        // Treat default browser/terminal/file manager categories as already bound.
        const QString binLower = app.binary.toLower();
        if (!app.isBound && (binLower == "foot" || binLower == "alacritty" || binLower == "kitty" ||
                             binLower == "ghostty")) {
            app.isBound = true;
            app.boundCombo = "SUPER + RETURN";
        } else if (!app.isBound && (binLower == "nautilus" || app.id.contains("nautilus"))) {
            app.isBound = true;
            app.boundCombo = "SUPER + SHIFT + F";
        }
        app.launches = count;
        app.isRunning = running;
    }

    // Sort apps by launches desc, running first, then name.
    std::sort(apps.begin(), apps.end(), [](const DesktopApp &a, const DesktopApp &b) {
        const int scoreA = a.launches * 10 + (a.isRunning ? 5 : 0);
        const int scoreB = b.launches * 10 + (b.isRunning ? 5 : 0);
        if (scoreA != scoreB)
            return scoreA > scoreB;
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });

    // Build Bind Recommendations: most-used unbound apps.
    m_bindRecs.clear();
    QSet<QString> reservedCombos = usedCombos;
    for (const DesktopApp &app : apps) {
        if (app.isBound)
            continue;
        if (app.launches <= 0 && !app.isRunning)
            continue;
        const QString suggested = suggestComboForName(app.name, reservedCombos);
        reservedCombos.insert(suggested);

        QVariantMap item;
        item["id"] = app.id;
        item["name"] = app.name;
        item["command"] = app.binary.isEmpty() ? app.exec : app.binary;
        item["exec"] = app.exec;
        item["launches"] = app.launches;
        item["isRunning"] = app.isRunning;
        item["suggestedCombo"] = suggested;
        item["reason"] = app.launches > 0
            ? tr("Launched %n times in 30 days", "", app.launches)
            : tr("Currently open with no shortcut");
        m_bindRecs.append(item);
        if (m_bindRecs.size() >= 10)
            break;
    }

    // Build Unbind Recommendations: unused (or not-installed) non-core app bindings.
    QList<BindingEntry> unused;
    for (const BindingEntry &e : entries) {
        if (!e.isAppBinding || e.isProtected)
            continue;
        if (e.launches == 0 && !e.isRunning)
            unused.append(e);
    }
    std::sort(unused.begin(), unused.end(), [](const BindingEntry &a, const BindingEntry &b) {
        if (a.isInstalled != b.isInstalled)
            return !a.isInstalled; // Not installed first.
        if (a.isWebapp != b.isWebapp)
            return a.isWebapp;     // Unused webapps next.
        return a.description.compare(b.description, Qt::CaseInsensitive) < 0;
    });

    m_unbindRecs.clear();
    for (const BindingEntry &e : unused) {
        QVariantMap item;
        item["combo"] = e.combo;
        item["description"] = e.description;
        item["appKey"] = e.appKey;
        item["isWebapp"] = e.isWebapp;
        item["isInstalled"] = e.isInstalled;
        item["isCustom"] = e.isCustom;
        item["reason"] = !e.isInstalled
            ? tr("App is not installed")
            : (e.isWebapp ? tr("Web app never launched in 30 days")
                          : tr("Not launched in the last 30 days"));
        m_unbindRecs.append(item);
    }

    // Build full bindings list for the All Bindings tab.
    m_bindingsList.clear();
    for (const BindingEntry &e : entries) {
        QVariantMap item;
        item["combo"] = e.combo;
        item["description"] = e.description;
        item["isAppBinding"] = e.isAppBinding;
        item["isProtected"] = e.isProtected;
        item["isCustom"] = e.isCustom;
        item["isInstalled"] = e.isInstalled;
        item["launches"] = e.launches;
        item["isRunning"] = e.isRunning;
        m_bindingsList.append(item);
    }

    // Build unbound overrides list so users can restore any unbound key.
    m_unboundList.clear();
    for (const QString &combo : m_managed.unbinds) {
        QVariantMap item;
        item["combo"] = combo;
        m_unboundList.append(item);
    }

    // Build Most Used Apps list.
    m_mostUsedList.clear();
    int maxLaunches = 1;
    for (const DesktopApp &app : apps)
        maxLaunches = qMax(maxLaunches, app.launches);

    for (const DesktopApp &app : apps) {
        if (app.launches <= 0 && !app.isRunning)
            continue;
        QVariantMap item;
        item["id"] = app.id;
        item["name"] = app.name;
        item["command"] = app.binary.isEmpty() ? app.exec : app.binary;
        item["launches"] = app.launches;
        item["ratio"] = double(qMax(1, app.launches)) / double(maxLaunches);
        item["isRunning"] = app.isRunning;
        item["isBound"] = app.isBound;
        item["boundCombo"] = app.boundCombo;
        item["suggestedCombo"] = app.isBound ? app.boundCombo : suggestComboForName(app.name, usedCombos);
        m_mostUsedList.append(item);
    }
}

void Backend::loadAllData() {
    QFile luaFile(m_config.bindingsLuaPath);
    QString luaContent;
    if (luaFile.open(QIODevice::ReadOnly))
        luaContent = QString::fromUtf8(luaFile.readAll());
    m_managed = parseManagedBlock(luaContent);

    QHash<QString, int> usageCounts;
    QSet<QString> runningClasses;
    if (!m_config.skipSystemCommands) {
        if (!m_config.journalctlPath.isEmpty()) {
            const QString journal = runCommand(
                m_config.journalctlPath,
                {"--user", "--since", "30 days ago", "-t", "systemd", "--no-pager"}, 4000);
            usageCounts = parseJournalUsage(journal);
        }
        if (!m_config.hyprctlPath.isEmpty()) {
            const QString clients = runCommand(m_config.hyprctlPath, {"-j", "clients"}, 2000);
            runningClasses = parseRunningClasses(clients.toUtf8());
        }
    }

    m_entries = loadActiveBindings(m_managed);
    m_apps = scanDesktopApps();
    computeRecommendations(m_entries, m_apps, usageCounts, runningClasses);
}

void Backend::refresh() {
    if (m_demoMode) {
        loadDemoData();
        return;
    }
    m_busy = true;
    emit busyChanged();
    loadAllData();
    m_busy = false;
    emit busyChanged();
    emit changed();
}

bool Backend::reloadHyprland(QString *errorOut) const {
    if (m_config.skipSystemCommands || m_config.hyprctlPath.isEmpty() ||
        qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")) {
        return true;
    }
    runCommand(m_config.hyprctlPath, {"reload"}, 3000);
    const QString errors = runCommand(m_config.hyprctlPath, {"configerrors"}, 2000).trimmed();
    if (!errors.isEmpty()) {
        if (errorOut) *errorOut = errors;
        return false;
    }
    return true;
}

bool Backend::applyManagedState(const ManagedState &newState, bool recordUndo,
                                const QString &successMsg) {
    m_errorMessage.clear();
    if (m_demoMode) {
        if (recordUndo) {
            m_undoStack.append(m_managed);
            m_redoStack.clear();
        }
        m_managed = newState;
        m_statusMessage = successMsg;
        emit statusChanged();
        emit changed();
        return true;
    }

    QFile inFile(m_config.bindingsLuaPath);
    QString originalContent;
    if (inFile.open(QIODevice::ReadOnly))
        originalContent = QString::fromUtf8(inFile.readAll());

    // Create a timestamped backup before our first edit of the session.
    if (!m_backedUpThisSession && QFileInfo::exists(m_config.bindingsLuaPath)) {
        const QString backupPath = QStringLiteral("%1.bak.%2")
                                       .arg(m_config.bindingsLuaPath)
                                       .arg(QDateTime::currentSecsSinceEpoch());
        QFile::copy(m_config.bindingsLuaPath, backupPath);
        m_backedUpThisSession = true;
    }

    const QString updatedContent = writeManagedBlock(originalContent, newState);
    QDir().mkpath(QFileInfo(m_config.bindingsLuaPath).absolutePath());

    QSaveFile outFile(m_config.bindingsLuaPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Text) ||
        outFile.write(updatedContent.toUtf8()) < 0 || !outFile.commit()) {
        m_errorMessage = tr("Could not write %1").arg(m_config.bindingsLuaPath);
        emit statusChanged();
        return false;
    }

    QString hyprErr;
    if (!reloadHyprland(&hyprErr)) {
        // Roll back atomically if Hyprland reports any config error.
        QSaveFile rollback(m_config.bindingsLuaPath);
        if (rollback.open(QIODevice::WriteOnly | QIODevice::Text)) {
            rollback.write(originalContent.toUtf8());
            rollback.commit();
        }
        reloadHyprland(nullptr);
        m_errorMessage = tr("Hyprland rejected the change and it was rolled back: %1").arg(hyprErr);
        emit statusChanged();
        return false;
    }

    if (recordUndo) {
        m_undoStack.append(m_managed);
        m_redoStack.clear();
    }
    m_managed = newState;
    m_statusMessage = successMsg;
    loadAllData();
    emit statusChanged();
    emit changed();
    return true;
}

void Backend::unbindKey(const QString &combo) {
    const QString norm = normalizeCombo(combo);
    if (norm.isEmpty())
        return;
    ManagedState next = m_managed;
    bool removedCustom = false;
    for (int i = next.binds.size() - 1; i >= 0; --i) {
        if (next.binds[i].combo == norm) {
            next.binds.removeAt(i);
            removedCustom = true;
        }
    }
    if (!removedCustom && !next.unbinds.contains(norm))
        next.unbinds.append(norm);

    applyManagedState(next, true, tr("Unbound %1").arg(norm));
}

void Backend::bindApp(const QString &combo, const QString &description,
                      const QString &command, bool isLaunch) {
    const QString norm = normalizeCombo(combo);
    if (norm.isEmpty() || command.trimmed().isEmpty()) {
        m_errorMessage = tr("Shortcut and command are required");
        emit statusChanged();
        return;
    }

    ManagedState next = m_managed;
    next.unbinds.removeAll(norm);
    for (int i = next.binds.size() - 1; i >= 0; --i) {
        if (next.binds[i].combo == norm)
            next.binds.removeAt(i);
    }

    ManagedBind b;
    b.combo = norm;
    b.description = description.trimmed().isEmpty() ? command.trimmed() : description.trimmed();
    b.command = command.trimmed();
    b.isLaunch = isLaunch;
    b.isRebind = !findConflict(norm).isEmpty();
    next.binds.append(b);

    applyManagedState(next, true, tr("Bound %1 to %2").arg(norm, b.description));
}

void Backend::restoreKey(const QString &combo) {
    const QString norm = normalizeCombo(combo);
    ManagedState next = m_managed;
    next.unbinds.removeAll(norm);
    for (int i = next.binds.size() - 1; i >= 0; --i) {
        if (next.binds[i].combo == norm)
            next.binds.removeAt(i);
    }
    applyManagedState(next, true, tr("Restored %1").arg(norm));
}

void Backend::undo() {
    if (m_undoStack.isEmpty())
        return;
    const ManagedState previous = m_undoStack.takeLast();
    m_redoStack.append(m_managed);
    applyManagedState(previous, false, tr("Undid last change"));
}

void Backend::redo() {
    if (m_redoStack.isEmpty())
        return;
    const ManagedState next = m_redoStack.takeLast();
    m_undoStack.append(m_managed);
    applyManagedState(next, false, tr("Redid change"));
}

QString Backend::findConflict(const QString &combo) const {
    const QString norm = normalizeCombo(combo);
    for (const BindingEntry &e : m_entries) {
        if (e.combo == norm)
            return e.description;
    }
    return {};
}

QString Backend::suggestFreeCombo(const QString &appName) const {
    QSet<QString> used;
    for (const BindingEntry &e : m_entries)
        used.insert(e.combo);
    return suggestComboForName(appName, used);
}

void Backend::clearMessages() {
    if (m_statusMessage.isEmpty() && m_errorMessage.isEmpty())
        return;
    m_statusMessage.clear();
    m_errorMessage.clear();
    emit statusChanged();
}

void Backend::loadDemoData() {
    m_demoMode = true;
    m_entries = {
        {"SUPER + RETURN", "Terminal", "exec", "omarchy-launch-terminal", "terminal", true, false, true, false, true, 42, true},
        {"SUPER + SHIFT + B", "Browser", "exec", "omarchy-launch-browser", "chromium", true, false, true, false, true, 28, true},
        {"SUPER + SHIFT + F", "File manager", "exec", "omarchy-launch-nautilus", "nautilus", true, false, true, false, true, 16, false},
        {"SUPER + SHIFT + M", "Music", "exec", "spotify", "spotify", true, false, false, false, true, 9, false},
        {"SUPER + SHIFT + O", "Obsidian", "exec", "obsidian", "obsidian", true, false, false, false, true, 19, true},
        {"SUPER + SHIFT + G", "Signal", "exec", "signal-desktop", "signal", true, false, false, false, false, 0, false},
        {"SUPER + SHIFT + SLASH", "Passwords", "exec", "1password", "1password", true, false, false, false, false, 0, false},
        {"SUPER + SHIFT + X", "X", "exec", "webapp", "x", true, true, false, false, true, 0, false},
        {"SUPER + SHIFT + ALT + A", "Grok", "exec", "webapp", "grok", true, true, false, false, true, 0, false},
        {"SUPER + SHIFT + P", "Google Photos", "exec", "webapp", "google photos", true, true, false, false, true, 0, false},
    };
    m_apps = {
        {"zen-browser", "Zen Browser", "zen-browser", "zen-browser", "zen", "zen", false, 34, true, false, ""},
        {"blender", "Blender", "blender", "blender", "blender", "blender", false, 21, false, false, ""},
        {"inkscape", "Inkscape", "inkscape", "inkscape", "inkscape", "inkscape", false, 14, false, false, ""},
        {"obsidian", "Obsidian", "obsidian", "obsidian", "obsidian", "obsidian", false, 19, true, true, "SUPER + SHIFT + O"},
        {"spotify", "Spotify", "spotify", "spotify", "spotify", "spotify", false, 9, false, true, "SUPER + SHIFT + M"},
    };
    QHash<QString, int> usage{
        {"zen", 34}, {"blender", 21}, {"inkscape", 14}, {"obsidian", 19}, {"spotify", 9},
        {"chromium", 28}, {"nautilus", 16}
    };
    QSet<QString> running{"zen", "obsidian", "chromium"};
    m_config.skipSystemCommands = true;
    computeRecommendations(m_entries, m_apps, usage, running);
    emit changed();
}

QJsonObject Backend::toScanJson() const {
    QJsonObject root;
    root["totalBindings"] = totalBindings();
    root["trackedApps"] = trackedAppsCount();
    root["bindRecommendations"] = QJsonArray::fromVariantList(m_bindRecs);
    root["unbindRecommendations"] = QJsonArray::fromVariantList(m_unbindRecs);
    root["mostUsedApps"] = QJsonArray::fromVariantList(m_mostUsedList);
    return root;
}

QJsonObject Backend::toBindingsJson() const {
    QJsonObject root;
    root["totalBindings"] = totalBindings();
    root["bindings"] = QJsonArray::fromVariantList(m_bindingsList);
    root["unboundOverrides"] = QJsonArray::fromVariantList(m_unboundList);
    return root;
}
