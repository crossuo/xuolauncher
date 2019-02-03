VERSION = 1.0.2

DEFINES += APP_VERSION=\\\"$$VERSION\\\"

CONFIG(release, debug|release) {
    DEFINES += USE_RELEASE=1
}
CONFIG(debug, debug|release) {
    DEFINES += USE_DEBUG=1
}
unix:!macx {
    DEFINES += USE_RELAUNCH=1 BUILD_LINUX=1 BUILD_MACOS=0 BUILD_WINDOWS=0
}
win32|win64 {
    DEFINES += USE_RELAUNCH=1 BUILD_WINDOWS=1 BUILD_LINUX=0 BUILD_MACOS=0
}
macx {
    DEFINES += BUILD_MACOS=1 BUILD_LINUX=0 BUILD_WINDOWS=0
}

QT       += core gui  network widgets
# concurrent
TARGET = xuomanager
TEMPLATE = app

CONFIG += c++11

include($$PWD/updater/updater.pri)

SOURCES += manager.cpp\
        managerwindow.cpp

HEADERS  += managerwindow.h

FORMS    += managerwindow.ui

OTHER_FILES += launcher.rc

RC_FILE = launcher.rc

RESOURCES += \
    res.qrc
