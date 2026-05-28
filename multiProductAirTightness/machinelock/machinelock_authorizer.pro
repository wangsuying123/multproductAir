QT += core widgets

CONFIG += c++17

SOURCES += \
    main.cpp \
    machinelockauthorizer.cpp \
    machinelockmanager.cpp \
    machinefingerprint.cpp

HEADERS += \
    machinelockauthorizer.h \
    machinelockmanager.h \
    machinefingerprint.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
