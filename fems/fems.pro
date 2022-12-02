include($$[QT_INSTALL_PREFIX]/include/nymea/plugin.pri)


QT += network


SOURCES += \
    femsconnection.cpp \
    femsnetworkreply.cpp \
    integrationpluginfems.cpp

HEADERS += \
    femsconnection.h \
    femsnetworkreply.h \
    integrationpluginfems.h

