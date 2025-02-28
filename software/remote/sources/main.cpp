#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QFontDatabase>
#include <QQmlContext>
#include <QFile>
#include <QFileInfo>
#include <QtDebug>

#include "launcher.h"
#include "jsonfile.h"
#include "translation.h"

#include "hardware/display_control.h"
#include "hardware/touchdetect.h"
#include "hardware/interrupt_handler.h"
#include "hardware/drv2605.h"
#include "hardware/bq27441.h"
#include "hardware/proximity_gesture_control.h"

#include "integrations/integration.h"
#include "entities/entities.h"

#include "notifications.h"
#include "bluetootharea.h"

#include "fileio.h"
#include "yioapi.h"

int main(int argc, char *argv[])
{
    qputenv("QML2_IMPORT_PATH", "/keyboard");
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    qputenv("QT_VIRTUALKEYBOARD_LAYOUT_PATH", "qrc:/keyboard/layouts");
    qputenv("QT_VIRTUALKEYBOARD_STYLE", "remotestyle");

    //    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    // Get the applications dir path and expose it to QML (prevents setting the JSON config variable)
    QString appPath = app.applicationDirPath();
    QString macPath = QFileInfo(appPath + "/../").canonicalPath() + QString("/Contents/Resources");

    // In case the translation file does not exist at root level (which is copied via the .pro file)
    // we are probably running on a mac, which gives a different result for the applicationDirPath
    // (actually points to '.../remote.app/Contents/MacOS/remote')
    if ((!QFile::exists(appPath + "/translations.json")) && (QFile::exists(macPath + "/translations.json"))) {
        appPath = macPath;
    }
    engine.rootContext()->setContextProperty("appPath", appPath);

    // Determine configuration path
    QString configPath = appPath;
    if (QFile::exists("/mnt/boot/config.json")) {
        configPath = "/mnt/boot";
    }
    engine.rootContext()->setContextProperty("configPath", configPath);

    // LOADING FONTS
    QFontDatabase::addApplicationFont(QStringLiteral("/usr/lib/fonts/OpenSans-Light.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral("/usr/lib/fonts/OpenSans-Regular.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral("/usr/lib/fonts/OpenSans-SemiBold.ttf"));
    QFontDatabase::addApplicationFont(QStringLiteral("/usr/lib/fonts/OpenSans-Bold.ttf"));

    // LOADING CUSTOM COMPONENTS
    qmlRegisterType<Integration>("Integration", 1, 0, "Integration");
    qmlRegisterType<Launcher>("Launcher", 1, 0, "Launcher");
    qmlRegisterType<JsonFile>("JsonFile", 1, 0, "JsonFile");
    qmlRegisterType<DisplayControl>("DisplayControl", 1, 0, "DisplayControl");
    qmlRegisterType<TouchEventFilter>("TouchEventFilter", 1, 0, "TouchEventFilter");
    qmlRegisterType<InterruptHandler>("InterruptHandler", 1, 0, "InterruptHandler");
    qmlRegisterType<drv2605>("Haptic", 1, 0, "Haptic");
    qmlRegisterType<BQ27441>("Battery", 1, 0, "Battery");
    qmlRegisterType<ProximityGestureControl>("Proximity", 1, 0, "Proximity");

    // BLUETOOTH AREA
    BluetoothArea bluetoothArea;
    engine.rootContext()->setContextProperty("bluetoothArea", &bluetoothArea);

    // TRANSLATION
    TranslationHandler transHndl(&engine);
    engine.rootContext()->setContextProperty("translateHandler", &transHndl);

    // ENTITIES
    Entities entities;
    engine.rootContext()->setContextProperty("entities", &entities);

    // NOTIFICATIONS
    Notifications notifications(&engine);
    engine.rootContext()->setContextProperty("notifications", &notifications);

    // FILE IO
    FileIO fileIO;
    engine.rootContext()->setContextProperty("fileio", &fileIO);

    // YIO API
    YioAPI yioapi(&engine);
    engine.rootContext()->setContextProperty("api", &yioapi);

    engine.addImportPath("qrc:/");

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
