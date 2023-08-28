include(../plugins.pri)

QT += network
QT += core

TARGET = $$qtLibraryTarget(nymea_integrationpluginkostalpiko)

QMAKE_CXXFLAGS_WARN_ON += -Wno-reorder

SOURCES += \
    integrationpluginkostalpiko.cpp \
    kostalnetworkreply.cpp \
    kostalpikoconnection.cpp

HEADERS += \
    integrationpluginkostalpiko.h \
    kostalnetworkreply.h \
    kostalpikoconnection.h

