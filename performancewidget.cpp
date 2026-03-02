#include "performancewidget.h"
#include "ui_performancewidget.h"

PerformanceWidget::PerformanceWidget(QWidget *parent) : QWidget(parent), ui(new Ui::PerformanceWidget)
{
    ui->setupUi(this);
}

PerformanceWidget::~PerformanceWidget()
{
    delete ui;
}
