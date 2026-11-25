#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVariantList>

struct ManagedBind {
    QString combo;
    QString description;
    QString command;
    bool isLaunch = true;
    bool isRebind = false;

    bool operator==(const ManagedBind &other) const {
        return combo == other.combo && description == other.description &&
               command == other.command && isLaunch == other.isLaunch &&
               isRebind == other.isRebind;
    }
};

struct ManagedState {
    QStringList unbinds;
    QList<ManagedBind> binds;

    bool operator==(const ManagedState &other) const {
        return unbinds == other.unbinds && binds == other.binds;
    }
};

struct BindingEntry {
    QString combo;
    QString description;
    QString dispatcher;
    QString arg;
    QString appKey;
    bool isAppBinding = false;
    bool isWebapp = false;
    bool isProtected = false;
    bool isCustom = false;
    bool isInstalled = true;
    int launches = 0;
    bool isRunning = false;
};

struct DesktopApp {
    QString id;
    QString name;
    QString exec;
    QString binary;
    QString wmClass;
    QString icon;
    bool isWebapp = false;
    int launches = 0;
    bool isRunning = false;
    bool isBound = false;
    QString boundCombo;
};

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList bindRecommendations READ bindRecommendations NOTIFY changed)
    Q_PROPERTY(QVariantList unbindRecommendations READ unbindRecommendations NOTIFY changed)
    Q_PROPERTY(QVariantList bindings READ bindings NOTIFY changed)
    Q_PROPERTY(QVariantList unboundOverrides READ unboundOverrides NOTIFY changed)
    Q_PROPERTY(QVariantList mostUsedApps READ mostUsedApps NOTIFY changed)
    Q_PROPERTY(QVariantList allApps READ allApps NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY changed)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY statusChanged)
    Q_PROPERTY(int totalBindings READ totalBindings NOTIFY changed)
    Q_PROPERTY(int trackedAppsCount READ trackedAppsCount NOTIFY changed)

public:
    struct Config {
        QString bindingsLuaPath;
        QString defaultAppsLuaPath;
        QStringList desktopDirs;
        QString hyprctlPath;
        QString journalctlPath;
        QString keybindingsCmdPath;
        bool skipSystemCommands = false;
    };

    explicit Backend(QObject *parent = nullptr);
    explicit Backend(const Config &config, QObject *parent = nullptr);

    QVariantList bindRecommendations() const { return m_bindRecs; }
    QVariantList unbindRecommendations() const { return m_unbindRecs; }
    QVariantList bindings() const { return m_bindingsList; }
    QVariantList unboundOverrides() const { return m_unboundList; }
    QVariantList mostUsedApps() const { return m_mostUsedList; }
    QVariantList allApps() const { return m_allAppsList; }
    bool busy() const { return m_busy; }
    bool canUndo() const { return !m_undoStack.isEmpty(); }
    bool canRedo() const { return !m_redoStack.isEmpty(); }
    QString statusMessage() const { return m_statusMessage; }
    QString errorMessage() const { return m_errorMessage; }
    int totalBindings() const { return m_bindingsList.size(); }
    int trackedAppsCount() const { return m_mostUsedList.size(); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void unbindKey(const QString &combo);
    Q_INVOKABLE void bindApp(const QString &combo, const QString &description,
                             const QString &command, bool isLaunch = true);
    Q_INVOKABLE void restoreKey(const QString &combo);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE QString findConflict(const QString &combo) const;
    Q_INVOKABLE QString suggestFreeCombo(const QString &appName) const;
    Q_INVOKABLE void clearMessages();

    void loadDemoData();
    QJsonObject toScanJson() const;
    QJsonObject toBindingsJson() const;

    // Static helpers exposed for unit tests.
    static QString normalizeCombo(const QString &combo);
    static ManagedState parseManagedBlock(const QString &luaContent);
    static QString writeManagedBlock(const QString &luaContent, const ManagedState &state);
    static QHash<QString, int> parseJournalUsage(const QString &journalText);
    static QSet<QString> parseRunningClasses(const QByteArray &hyprClientsJson);
    static QString normalizeToken(const QString &raw);
    static QString suggestComboForName(const QString &appName, const QSet<QString> &usedCombos);

signals:
    void changed();
    void busyChanged();
    void statusChanged();

private:
    Config defaultConfig() const;
    void loadAllData();
    QList<DesktopApp> scanDesktopApps() const;
    QList<BindingEntry> loadActiveBindings(const ManagedState &managed) const;
    QHash<QString, QPair<QString, bool>> parseDefaultAppBindings() const;
    void computeRecommendations(QList<BindingEntry> &entries, QList<DesktopApp> &apps,
                                const QHash<QString, int> &usageCounts,
                                const QSet<QString> &runningClasses);
    bool applyManagedState(const ManagedState &newState, bool recordUndo, const QString &successMsg);
    bool reloadHyprland(QString *errorOut) const;
    QString runCommand(const QString &program, const QStringList &args, int timeoutMs = 4000,
                       bool *ok = nullptr) const;
    static QString luaEscape(const QString &text);
    static QString unescapeSystemdName(const QString &escaped);

    Config m_config;
    bool m_demoMode = false;
    bool m_busy = false;
    bool m_backedUpThisSession = false;

    ManagedState m_managed;
    QList<ManagedState> m_undoStack;
    QList<ManagedState> m_redoStack;

    QList<BindingEntry> m_entries;
    QList<DesktopApp> m_apps;
    QVariantList m_bindRecs;
    QVariantList m_unbindRecs;
    QVariantList m_bindingsList;
    QVariantList m_unboundList;
    QVariantList m_mostUsedList;
    QVariantList m_allAppsList;

    QString m_statusMessage;
    QString m_errorMessage;
};
