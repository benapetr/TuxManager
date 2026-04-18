QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
TARGET = tux-manager

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    aboutdialog.cpp \
    colorschemedialog.cpp \
    colorscheme.cpp \
    main.cpp \
    mainwindow.cpp \
    metrics.cpp \
    misc.cpp \
    system/cpu.cpp \
    system/gpu.cpp \
    system/kernel.cpp \
    system/memory.cpp \
    system/network.cpp \
    system/storage.cpp \
    system/swap.cpp \
    ui/widgetstyle.cpp \
    ui/uihelper.cpp \
    configuration.cpp \
    logger.cpp \
    processeswidget.cpp \
    performancewidget.cpp \
    runtaskdialog.cpp \
    userswidget.cpp \
    serviceswidget.cpp \
    os/proc.cpp \
    os/process.cpp \
    os/processrefreshservice.cpp \
    os/processtreemodel.cpp \
    os/processmodel.cpp \
    os/processfilterproxy.cpp \
    os/processhelper.cpp \
    os/service.cpp \
    os/servicemodel.cpp \
    os/servicefilterproxy.cpp \
    os/servicehelper.cpp \
    perf/graphwidget.cpp \
    perf/sidepanelitem.cpp \
    perf/sidepanel.cpp \
    perf/sidepanelgroup.cpp \
    perf/sidepanelorderdialog.cpp \
    perf/cpugrapharea.cpp \
    perf/cpudetailwidget.cpp \
    perf/diskdetailwidget.cpp \
    perf/networkdetailwidget.cpp \
    perf/gpudetailwidget.cpp \
    perf/swapgrapharea.cpp \
    perf/swapdetailwidget.cpp \
    perf/memorybar.cpp \
    perf/memorydetailwidget.cpp

HEADERS += \
    aboutdialog.h \
    colorschemedialog.h \
    colorscheme.h \
    globals.h \
    mainwindow.h \
    metrics.h \
    misc.h \
    system/cpu.h \
    system/gpu.h \
    system/kernel.h \
    system/memory.h \
    system/network.h \
    system/storage.h \
    system/swap.h \
    ui/widgetstyle.h \
    ui/uihelper.h \
    configuration.h \
    historybuffer.h \
    logger.h \
    processeswidget.h \
    performancewidget.h \
    runtaskdialog.h \
    userswidget.h \
    serviceswidget.h \
    os/proc.h \
    os/process.h \
    os/processrefreshservice.h \
    os/processtreemodel.h \
    os/processmodel.h \
    os/processfilterproxy.h \
    os/processhelper.h \
    os/service.h \
    os/servicemodel.h \
    os/servicefilterproxy.h \
    os/servicehelper.h \
    perf/graphwidget.h \
    perf/sidepanelitem.h \
    perf/sidepanel.h \
    perf/sidepanelgroup.h \
    perf/sidepanelorderdialog.h \
    perf/cpugrapharea.h \
    perf/cpudetailwidget.h \
    perf/diskdetailwidget.h \
    perf/networkdetailwidget.h \
    perf/gpudetailwidget.h \
    perf/swapgrapharea.h \
    perf/swapdetailwidget.h \
    perf/memorybar.h \
    perf/memorydetailwidget.h

FORMS += \
    aboutdialog.ui \
    colorschemedialog.ui \
    mainwindow.ui \
    processeswidget.ui \
    performancewidget.ui \
    runtaskdialog.ui \
    userswidget.ui \
    serviceswidget.ui \
    perf/sidepanelorderdialog.ui \
    perf/cpudetailwidget.ui \
    perf/diskdetailwidget.ui \
    perf/networkdetailwidget.ui \
    perf/gpudetailwidget.ui \
    perf/swapdetailwidget.ui \
    perf/memorydetailwidget.ui

RESOURCES += \
    resources.qrc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
