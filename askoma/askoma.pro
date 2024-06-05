include(../plugins.pri)

QT += network

include(../modbus.pri)

HEADERS += \
    integrationpluginaskoma.h \
    askoheat.h

SOURCES += \
    integrationpluginaskoma.cpp \
    askoheat.cpp