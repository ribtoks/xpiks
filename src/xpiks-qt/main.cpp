/*
 * This file is a part of Xpiks - cross platform application for
 * keywording and uploading images for microstocks
 * Copyright (C) 2014-2016 Taras Kushnir <kushnirTV@gmail.com>
 *
 * Xpiks is distributed under the GNU General Public License, version 3.0
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>

#include <QDir>
#include <QtQml>
#include <QFile>
#include <QUuid>
#include <QtDebug>
#include <QDateTime>
#include <QSettings>
#include <QTextStream>
#include <QQmlContext>
#include <QApplication>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QQmlApplicationEngine>
#include <QDesktopWidget>
//-------------------------------------
#include "SpellCheck/spellchecksuggestionmodel.h"
#include "Models/filteredartitemsproxymodel.h"
#include "MetadataIO/metadataiocoordinator.h"
#include "AutoComplete/autocompleteservice.h"
#include "Conectivity/analyticsuserevent.h"
#include "SpellCheck/spellcheckerservice.h"
#include "AutoComplete/autocompletemodel.h"
#include "Models/recentdirectoriesmodel.h"
#include "MetadataIO/backupsaverservice.h"
#include "QMLExtensions/triangleelement.h"
#include "Suggestion/keywordssuggestor.h"
#include "Models/combinedartworksmodel.h"
#include "Conectivity/telemetryservice.h"
#include "Helpers/globalimageprovider.h"
#include "Models/uploadinforepository.h"
#include "Conectivity/ftpcoordinator.h"
#include "Helpers/helpersqmlwrapper.h"
#include "Encryption/secretsmanager.h"
#include "Models/artworksrepository.h"
#include "QMLExtensions/colorsmodel.h"
#include "Warnings/warningsservice.h"
#include "UndoRedo/undoredomanager.h"
#include "Helpers/clipboardhelper.h"
#include "Commands/commandmanager.h"
#include "Suggestion/locallibrary.h"
#include "Models/artworkuploader.h"
#include "Warnings/warningsmodel.h"
#include "Plugins/pluginmanager.h"
#include "Helpers/loggingworker.h"
#include "Models/languagesmodel.h"
#include "Helpers/updateservice.h"
#include "Models/artitemsmodel.h"
#include "Models/settingsmodel.h"
#include "Helpers/appsettings.h"
#include "Models/ziparchiver.h"
#include "Helpers/constants.h"
#include "Helpers/runguard.h"
#include "Models/logsmodel.h"
#include "Helpers/logger.h"
#include "Common/version.h"
#include "Common/defines.h"

void myMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    Q_UNUSED(context);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    QString logLine = qFormatLogMessage(type, context, msg);
#else
    QString msgType;
    switch (type) {
    case QtDebugMsg:
        msgType = "debug";
        break;
    case QtWarningMsg:
        msgType = "warning";
        break;
    case QtCriticalMsg:
        msgType = "critical";
        break;
    case QtFatalMsg:
        msgType = "fatal";
        break;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 1))
    case QtInfoMsg:
        msgType = "info";
        break;
#endif
    }
    // %{time hh:mm:ss.zzz} %{type} T#%{threadid} %{function} - %{message}
    QString time = QDateTime::currentDateTimeUtc().toString("hh:mm:ss.zzz");
    QString logLine = QString("%1 %2 T#%3 %4 - %5")
            .arg(time).arg(msgType)
            .arg(0).arg(context.function)
            .arg(msg);
#endif

    Helpers::Logger &logger = Helpers::Logger::getInstance();
    logger.log(logLine);

    if (type == QtFatalMsg) {
        abort();
    }
}

#define STRINGIZE_(x) #x
#define STRINGIZE(x) STRINGIZE_(x)

void initQSettings() {
    QCoreApplication::setOrganizationName(Constants::ORGANIZATION_NAME);
    QCoreApplication::setOrganizationDomain(Constants::ORGANIZATION_DOMAIN);
    QCoreApplication::setApplicationName(Constants::APPLICATION_NAME);
    QString appVersion(STRINGIZE(BUILDNUMBER));
    QCoreApplication::setApplicationVersion(STRINGIZE(XPIKS_VERSION) " " STRINGIZE(XPIKS_VERSION_SUFFIX) " - " +
                                            appVersion.left(10));
}

void ensureUserIdExists(Helpers::AppSettings *settings) {
    QLatin1String userIdKey = QLatin1String(Constants::USER_AGENT_ID);
    if (!settings->contains(userIdKey)) {
        QUuid uuid = QUuid::createUuid();
        settings->setValue(userIdKey, uuid.toString());
    }
}

static const char *setHighDpiEnvironmentVariable()
{
    const char* envVarName = 0;
    static const char ENV_VAR_QT_DEVICE_PIXEL_RATIO[] = "QT_DEVICE_PIXEL_RATIO";

#ifdef Q_OS_WIN
    bool isWindows = true;
#else
    bool isWindows = false;
#endif

#if (QT_VERSION < QT_VERSION_CHECK(5, 6, 0))
    if (isWindows
            && !qEnvironmentVariableIsSet(ENV_VAR_QT_DEVICE_PIXEL_RATIO)) {
        envVarName = ENV_VAR_QT_DEVICE_PIXEL_RATIO;
        qputenv(envVarName, "auto");
    }
#else
    if (isWindows
            && !qEnvironmentVariableIsSet(ENV_VAR_QT_DEVICE_PIXEL_RATIO) // legacy in 5.6, but still functional
            && !qEnvironmentVariableIsSet("QT_AUTO_SCREEN_SCALE_FACTOR")
            && !qEnvironmentVariableIsSet("QT_SCALE_FACTOR")
            && !qEnvironmentVariableIsSet("QT_SCREEN_SCALE_FACTORS")) {
        QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    }
#endif // < Qt 5.6
    return envVarName;
}

int main(int argc, char *argv[]) {
#ifdef QT_NO_DEBUG
    const QString runGuardName = "xpiks";
#else
    const QString runGuardName = "xpiks-debug";
#endif
    Helpers::RunGuard guard(runGuardName);
    if (!guard.tryToRun()) {
        std::cerr << "Xpiks is already running";
        return -1;
    }

    const char *highDpiEnvironmentVariable = setHighDpiEnvironmentVariable();

    initQSettings();
    Helpers::AppSettings appSettings;
    ensureUserIdExists(&appSettings);

    Suggestion::LocalLibrary localLibrary;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#else
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
#endif
    if (!appDataPath.isEmpty()) {
        QDir appDataDir(appDataPath);

        QString libraryFilePath = appDataDir.filePath(Constants::LIBRARY_FILENAME);
        localLibrary.setLibraryPath(libraryFilePath);
    } else {
        std::cerr << "AppDataPath is empty!";
    }

#ifdef WITH_LOGS
    const QString &logFileDir = QDir::cleanPath(appDataPath + QDir::separator() + "logs");
    if (!logFileDir.isEmpty()) {
        QDir dir(logFileDir);
        if (!dir.exists()) {
            bool created = QDir().mkpath(logFileDir);
            Q_UNUSED(created);
        }

        QString time = QDateTime::currentDateTimeUtc().toString("ddMMyyyy-hhmmss-zzz");
        QString logFilename = QString("xpiks-qt-%1.log").arg(time);

        QString logFilePath = dir.filePath(logFilename);

        Helpers::Logger &logger = Helpers::Logger::getInstance();
        logger.setLogFilePath(logFilePath);
    }

#endif

    QMLExtensions::ColorsModel colorsModel;
    Models::LogsModel logsModel(&colorsModel);
    logsModel.startLogging();

    qSetMessagePattern("%{time hh:mm:ss.zzz} %{type} T#%{threadid} %{function} - %{message}");
    qInstallMessageHandler(myMessageHandler);

    LOG_INFO << "Log started. Today is" << QDateTime::currentDateTimeUtc().toString("dd.MM.yyyy");
    LOG_INFO << "Xpiks" << XPIKS_VERSION_STRING << "-" << STRINGIZE(BUILDNUMBER);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 4, 0))
    LOG_INFO << QSysInfo::productType() << QSysInfo::productVersion() << QSysInfo::currentCpuArchitecture();
#else
#ifdef Q_OS_WIN
    LOG_INFO << QLatin1String("Windows Qt<5.4");
#elsif Q_OS_DARWIN
    LOG_INFO << QLatin1String("OS X Qt<5.4");
#else
    LOG_INFO << QLatin1String("LINUX Qt<5.4");
#endif
#endif

    QApplication app(argc, argv);

    LOG_DEBUG << "Working directory of Xpiks is:" << QDir::currentPath();

    if (highDpiEnvironmentVariable) {
        qunsetenv(highDpiEnvironmentVariable);
    }

    localLibrary.loadLibraryAsync();

    QString userId = appSettings.value(QLatin1String(Constants::USER_AGENT_ID)).toString();
    userId.remove(QRegExp("[{}-]."));

    Models::ArtworksRepository artworkRepository;
    Models::ArtItemsModel artItemsModel;
    Models::CombinedArtworksModel combinedArtworksModel;
    Models::UploadInfoRepository uploadInfoRepository;
    Warnings::WarningsService warningsService;
    Models::SettingsModel settingsModel;
    settingsModel.readAllValues();
    Encryption::SecretsManager secretsManager;
    UndoRedo::UndoRedoManager undoRedoManager;
    Models::ZipArchiver zipArchiver;
    Suggestion::KeywordsSuggestor keywordsSuggestor(&localLibrary);
    Models::FilteredArtItemsProxyModel filteredArtItemsModel;
    filteredArtItemsModel.setSourceModel(&artItemsModel);
    Models::RecentDirectoriesModel recentDirectorieModel;
    Conectivity::FtpCoordinator *ftpCoordinator = new Conectivity::FtpCoordinator(
                settingsModel.getMaxParallelUploads(), settingsModel.getUploadTimeout());
    Models::ArtworkUploader artworkUploader(ftpCoordinator);
    SpellCheck::SpellCheckerService spellCheckerService;
    SpellCheck::SpellCheckSuggestionModel spellCheckSuggestionModel;
    MetadataIO::BackupSaverService metadataSaverService;
    Warnings::WarningsModel warningsModel;
    warningsModel.setSourceModel(&artItemsModel);
    Models::LanguagesModel languagesModel;
    AutoComplete::AutoCompleteModel autoCompleteModel;
    AutoComplete::AutoCompleteService autoCompleteService(&autoCompleteModel);

    bool checkForUpdates = appSettings.value(Constants::CHECK_FOR_UPDATES, true).toBool();
    Helpers::UpdateService updateService(checkForUpdates);

    MetadataIO::MetadataIOCoordinator metadataIOCoordinator;
#ifdef TELEMETRY_ENABLED
    bool telemetryEnabled = appSettings.value(Constants::USER_STATISTICS, true).toBool();
#else
    bool telemetryEnabled = appSettings.value(Constants::USER_STATISTICS, false).toBool();
#endif
    Conectivity::TelemetryService telemetryService(userId, telemetryEnabled);

    Plugins::PluginManager pluginManager;
    Plugins::PluginsWithActionsModel pluginsWithActions;
    pluginsWithActions.setSourceModel(&pluginManager);

    Commands::CommandManager commandManager;
    commandManager.InjectDependency(&artworkRepository);
    commandManager.InjectDependency(&artItemsModel);
    commandManager.InjectDependency(&filteredArtItemsModel);
    commandManager.InjectDependency(&combinedArtworksModel);
    commandManager.InjectDependency(&artworkUploader);
    commandManager.InjectDependency(&uploadInfoRepository);
    commandManager.InjectDependency(&warningsService);
    commandManager.InjectDependency(&secretsManager);
    commandManager.InjectDependency(&undoRedoManager);
    commandManager.InjectDependency(&zipArchiver);
    commandManager.InjectDependency(&keywordsSuggestor);
    commandManager.InjectDependency(&settingsModel);
    commandManager.InjectDependency(&recentDirectorieModel);
    commandManager.InjectDependency(&spellCheckerService);
    commandManager.InjectDependency(&spellCheckSuggestionModel);
    commandManager.InjectDependency(&metadataSaverService);
    commandManager.InjectDependency(&telemetryService);
    commandManager.InjectDependency(&updateService);
    commandManager.InjectDependency(&logsModel);
    commandManager.InjectDependency(&localLibrary);
    commandManager.InjectDependency(&metadataIOCoordinator);
    commandManager.InjectDependency(&pluginManager);
    commandManager.InjectDependency(&languagesModel);
    commandManager.InjectDependency(&colorsModel);
    commandManager.InjectDependency(&autoCompleteService);

    commandManager.ensureDependenciesInjected();

    // other initializations
    secretsManager.setMasterPasswordHash(appSettings.value(Constants::MASTER_PASSWORD_HASH, "").toString());
    uploadInfoRepository.initFromString(appSettings.value(Constants::UPLOAD_HOSTS, "").toString());
    recentDirectorieModel.deserializeFromSettings(appSettings.value(Constants::RECENT_DIRECTORIES, "").toString());

    commandManager.connectEntitiesSignalsSlots();

    languagesModel.initFirstLanguage();

    qmlRegisterType<Helpers::ClipboardHelper>("xpiks", 1, 0, "ClipboardHelper");
    qmlRegisterType<QMLExtensions::TriangleElement>("xpiks", 1, 0, "TriangleElement");

    QQmlApplicationEngine engine;
    Helpers::GlobalImageProvider *globalProvider = new Helpers::GlobalImageProvider(QQmlImageProviderBase::Image);

    Helpers::HelpersQmlWrapper helpersQmlWrapper(&commandManager);

    QQmlContext *rootContext = engine.rootContext();
    rootContext->setContextProperty("artItemsModel", &artItemsModel);
    rootContext->setContextProperty("artworkRepository", &artworkRepository);
    rootContext->setContextProperty("combinedArtworks", &combinedArtworksModel);
    rootContext->setContextProperty("appSettings", &appSettings);
    rootContext->setContextProperty("artworkUploader", &artworkUploader);
    rootContext->setContextProperty("uploadInfos", &uploadInfoRepository);
    rootContext->setContextProperty("logsModel", &logsModel);
    rootContext->setContextProperty("secretsManager", &secretsManager);
    rootContext->setContextProperty("undoRedoManager", &undoRedoManager);
    rootContext->setContextProperty("zipArchiver", &zipArchiver);
    rootContext->setContextProperty("keywordsSuggestor", &keywordsSuggestor);
    rootContext->setContextProperty("settingsModel", &settingsModel);
    rootContext->setContextProperty("filteredArtItemsModel", &filteredArtItemsModel);
    rootContext->setContextProperty("helpersWrapper", &helpersQmlWrapper);
    rootContext->setContextProperty("recentDirectories", &recentDirectorieModel);
    rootContext->setContextProperty("updateService", &updateService);
    rootContext->setContextProperty("spellCheckerService", &spellCheckerService);
    rootContext->setContextProperty("spellCheckSuggestionModel", &spellCheckSuggestionModel);
    rootContext->setContextProperty("metadataIOCoordinator", &metadataIOCoordinator);
    rootContext->setContextProperty("pluginManager", &pluginManager);
    rootContext->setContextProperty("pluginsWithActions", &pluginsWithActions);
    rootContext->setContextProperty("warningsModel", &warningsModel);
    rootContext->setContextProperty("languagesModel", &languagesModel);
    rootContext->setContextProperty("i18", &languagesModel);
    rootContext->setContextProperty("Colors", &colorsModel);
    rootContext->setContextProperty("acSource", &autoCompleteModel);
    rootContext->setContextProperty("autoCompleteService", &autoCompleteService);

#ifdef QT_DEBUG
    rootContext->setContextProperty("debug", true);
#else
    rootContext->setContextProperty("debug", false);
#endif

    engine.addImageProvider("global", globalProvider);
    LOG_DEBUG << "About to load main view...";
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    LOG_DEBUG << "Main view loaded";

    pluginManager.getUIProvider()->setQmlEngine(&engine);
    QQuickWindow *window = qobject_cast<QQuickWindow*>(engine.rootObjects().at(0));
    pluginManager.getUIProvider()->setRoot(window->contentItem());

#ifdef QT_DEBUG
    if (argc > 1) {
        QStringList pathes;
        for (int i = 1; i < argc; ++i) {
            pathes.append(QString(argv[i]));
        }

        commandManager.addInitialArtworks(pathes, QStringList());
    }
#endif

    return app.exec();
}
