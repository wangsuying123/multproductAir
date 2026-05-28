QT       += core widgets sql charts serialbus network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

HEADERS += aboutpage.h \
           airtightnessparamsdao.h \
           airtightparamsetting.h \
           chartdialog.h \
           commspage.h \
           databasemanager.h \
           login.h \
           logmanager.h \
           mainControlSetting.h \
           mainwindow.h \
           realtimemonitor.h \
           splashscreen.h \
           systemsetting.h \
           tcpcommunicationmanager.h \
           testresultshow.h \
           usermanagement.h \
           enum/fillTypeUnit.h \
           enum/leakUnit.h \
           enum/pressureUnit.h \
           enum/VolumeEncoding.h \
           enum/volumeUnit.h \
           TcpServerManager.h \
           machinelock/machinefingerprint.h \
           machinelock/machinelockmanager.h
FORMS += aboutpage.ui \
         airtightparamsetting.ui \
         commspage.ui \
         login.ui \
         mainControlSetting.ui \
         mainwindow.ui \
         realtimemonitor.ui \
         systemsetting.ui \
         testresultshow.ui \
         userdialog.ui \
         usermanagement.ui
SOURCES += aboutpage.cpp \
           airtightnessparamsdao.cpp \
           airtightparamsetting.cpp \
           chartdialog.cpp \
           commspage.cpp \
           databasemanager.cpp \
           login.cpp \
           logmanager.cpp \
           main.cpp \
           mainControlSetting.cpp \
           mainwindow.cpp \
           realtimemonitor.cpp \
           splashscreen.cpp \
           systemsetting.cpp \
           tcpcommunicationmanager.cpp \
           testresultshow.cpp \
           usermanagement.cpp \
           tcpservermanager.cpp \
           enum/fillTypeUnit.cpp \
           enum/leakUnit.cpp \
           enum/pressureUnit.cpp \
           enum/volumeUnit.cpp \
           machinelock/machinefingerprint.cpp \
           machinelock/machinelockmanager.cpp
RESOURCES += resources.qrc

# Windows应用程序图标
win32:RC_FILE = app_icon.rc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
