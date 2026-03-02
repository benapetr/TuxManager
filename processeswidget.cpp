#include "processeswidget.h"
#include "ui_processeswidget.h"

ProcessesWidget::ProcessesWidget(QWidget *parent) : QWidget(parent), ui(new Ui::ProcessesWidget)
{
    ui->setupUi(this);
}

ProcessesWidget::~ProcessesWidget()
{
    delete ui;
}
