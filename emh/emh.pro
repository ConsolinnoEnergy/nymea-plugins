include(../plugins.pri)

QT += network

SOURCES += \
    integrationpluginemh.cpp \

HEADERS += \
    integrationpluginemh.h \

LIBS += -ljsoncpp -lcurl
