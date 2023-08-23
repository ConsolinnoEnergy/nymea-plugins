include(../plugins.pri)

QT += network
QT += core

TARGET = $$qtLibraryTarget(nymea_integrationplugikostalpico)

QMAKE_CXXFLAGS_WARN_ON += -Wno-reorder

SOURCES += \
    integrationpluginkostalpico.cpp \
    kostalnetworkreply.cpp \
    kostalpicoconnection.cpp

HEADERS += \
    integrationpluginkostalpico.h \
    kostalnetworkreply.h \
    kostalpicoconnection.h

