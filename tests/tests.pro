# Desktop Qt 5 test harness for the upload logic (not part of the RPM).
TEMPLATE = app
TARGET = tst_uploader
QT += core network testlib
QT -= gui
CONFIG += c++11 console testcase
CONFIG -= app_bundle
INCLUDEPATH += ../src
SOURCES += tst_uploader.cpp ../src/uploader.cpp
HEADERS += ../src/uploader.h
DEFINES += APP_VERSION=\\\"test\\\" LINKDROP_TESTING
