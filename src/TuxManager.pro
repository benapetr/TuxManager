QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
TARGET = tux-manager

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    colorschemedialog.cpp \
    colorscheme.cpp \
    main.cpp \
    mainwindow.cpp \
    misc.cpp \
    ui/widgetstyle.cpp \
    ui/uihelper.cpp \
    configuration.cpp \
    logger.cpp \
    processeswidget.cpp \
    performancewidget.cpp \
    userswidget.cpp \
    serviceswidget.cpp \
    os/proc.cpp \
    os/process.cpp \
    os/processtreemodel.cpp \
    os/processmodel.cpp \
    os/processfilterproxy.cpp \
    os/processhelper.cpp \
    os/service.cpp \
    os/servicemodel.cpp \
    os/servicefilterproxy.cpp \
    os/servicehelper.cpp \
    perf/perfdataprovider.cpp \
    perf/graphwidget.cpp \
    perf/sidepanelitem.cpp \
    perf/sidepanel.cpp \
    perf/cpugrapharea.cpp \
    perf/cpudetailwidget.cpp \
    perf/diskdetailwidget.cpp \
    perf/networkdetailwidget.cpp \
    perf/gpudetailwidget.cpp \
    perf/swapdetailwidget.cpp \
    perf/memorybar.cpp \
    perf/memorydetailwidget.cpp

HEADERS += \
    colorschemedialog.h \
    colorscheme.h \
    mainwindow.h \
    misc.h \
    ui/widgetstyle.h \
    ui/uihelper.h \
    configuration.h \
    logger.h \
    processeswidget.h \
    performancewidget.h \
    userswidget.h \
    serviceswidget.h \
    os/proc.h \
    os/process.h \
    os/processtreemodel.h \
    os/processmodel.h \
    os/processfilterproxy.h \
    os/processhelper.h \
    os/service.h \
    os/servicemodel.h \
    os/servicefilterproxy.h \
    os/servicehelper.h \
    perf/perfdataprovider.h \
    perf/historybuffer.h \
    perf/graphwidget.h \
    perf/sidepanelitem.h \
    perf/sidepanel.h \
    perf/cpugrapharea.h \
    perf/cpudetailwidget.h \
    perf/diskdetailwidget.h \
    perf/networkdetailwidget.h \
    perf/gpudetailwidget.h \
    perf/swapdetailwidget.h \
    perf/memorybar.h \
    perf/memorydetailwidget.h

FORMS += \
    colorschemedialog.ui \
    mainwindow.ui \
    processeswidget.ui \
    performancewidget.ui \
    userswidget.ui \
    serviceswidget.ui \
    perf/cpudetailwidget.ui \
    perf/diskdetailwidget.ui \
    perf/networkdetailwidget.ui \
    perf/memorydetailwidget.ui

RESOURCES += \
    resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
