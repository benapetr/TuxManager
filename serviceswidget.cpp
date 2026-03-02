#include "serviceswidget.h"
#include "ui_serviceswidget.h"

ServicesWidget::ServicesWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::ServicesWidget)
{
    ui->setupUi(this);
}

ServicesWidget::~ServicesWidget()
{
    delete ui;
}
