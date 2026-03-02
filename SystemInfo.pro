QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    configuration.cpp \
    logger.cpp \
    processeswidget.cpp \
    performancewidget.cpp \
    userswidget.cpp \
    serviceswidget.cpp \
    os/process.cpp \
    os/processmodel.cpp \
    os/processfilterproxy.cpp \
    os/processhelper.cpp \
    perf/perfdataprovider.cpp \
    perf/graphwidget.cpp \
    perf/sidepanelitem.cpp \
    perf/sidepanel.cpp \
    perf/cpugrapharea.cpp \
    perf/cpudetailwidget.cpp \
    perf/memorybar.cpp \
    perf/memorydetailwidget.cpp

HEADERS += \
    mainwindow.h \
    configuration.h \
    logger.h \
    processeswidget.h \
    performancewidget.h \
    userswidget.h \
    serviceswidget.h \
    os/process.h \
    os/processmodel.h \
    os/processfilterproxy.h \
    os/processhelper.h \
    perf/perfdataprovider.h \
    perf/graphwidget.h \
    perf/sidepanelitem.h \
    perf/sidepanel.h \
    perf/cpugrapharea.h \
    perf/cpudetailwidget.h \
    perf/memorybar.h \
    perf/memorydetailwidget.h

FORMS += \
    mainwindow.ui \
    processeswidget.ui \
    performancewidget.ui \
    userswidget.ui \
    serviceswidget.ui \
    perf/cpudetailwidget.ui \
    perf/memorydetailwidget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
