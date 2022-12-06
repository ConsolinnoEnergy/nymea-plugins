include($$[QT_INSTALL_PREFIX]/include/nymea/plugin.pri)


QT += network

QMAKE_CXXFLAGS_WARN_ON += -Wno-reorder

SOURCES += \
    femsconnection.cpp \
    femsnetworkreply.cpp \
    integrationpluginfems.cpp

HEADERS += \
    femsconnection.h \
    femsnetworkreply.h \
    integrationpluginfems.h

