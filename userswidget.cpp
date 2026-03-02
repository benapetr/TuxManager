#include "userswidget.h"
#include "ui_userswidget.h"

UsersWidget::UsersWidget(QWidget *parent) : QWidget(parent), ui(new Ui::UsersWidget)
{
    this->ui->setupUi(this);
}

UsersWidget::~UsersWidget()
{
    delete this->ui;
}
