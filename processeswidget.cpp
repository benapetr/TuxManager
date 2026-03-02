#include "processeswidget.h"
#include "ui_processeswidget.h"

ProcessesWidget::ProcessesWidget(QWidget *parent) : QWidget(parent), ui(new Ui::ProcessesWidget)
{
    this->ui->setupUi(this);
}

ProcessesWidget::~ProcessesWidget()
{
    delete this->ui;
}
