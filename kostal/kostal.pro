include($$[QT_INSTALL_PREFIX]/include/nymea/plugin.pri)

QT += network
QT += core

QMAKE_CXXFLAGS_WARN_ON += -Wno-reorder

SOURCES += \
    integrationpluginkostal.cpp \
    kostalnetworkreply.cpp \
    kostalpicoconnection.cpp

HEADERS += \
    integrationpluginkostal.h \
    kostalnetworkreply.h \
    kostalpicoconnection.h

