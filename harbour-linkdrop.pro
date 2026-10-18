TARGET = harbour-linkdrop
CONFIG += sailfishapp c++11
QT += network

# The RPM spec passes VERSION=%{version}; fall back for IDE builds.
isEmpty(VERSION): VERSION = 0.1.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

SOURCES += \
    src/harbour-linkdrop.cpp \
    src/uploader.cpp

HEADERS += src/uploader.h

DISTFILES += \
    harbour-linkdrop.desktop \
    qml/harbour-linkdrop.qml \
    qml/pages/MainPage.qml \
    qml/pages/AboutPage.qml \
    qml/cover/CoverPage.qml \
    rpm/harbour-linkdrop.spec \
    README.md \
    LICENSE

# Ship the licence with the app data (Harbour only allows files under
# /usr/share/harbour-linkdrop, so not /usr/share/licenses).
license.files = LICENSE
license.path = /usr/share/$${TARGET}
INSTALLS += license

# sailfishapp.prf installs icons/<size>/harbour-linkdrop.png for each size.
SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172
