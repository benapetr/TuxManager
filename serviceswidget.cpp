#include "serviceswidget.h"
#include "ui_serviceswidget.h"

ServicesWidget::ServicesWidget(QWidget *parent) : QWidget(parent), ui(new Ui::ServicesWidget)
{
    this->ui->setupUi(this);
}

ServicesWidget::~ServicesWidget()
{
    delete this->ui;
}
