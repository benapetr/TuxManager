#include "userswidget.h"
#include "ui_userswidget.h"

UsersWidget::UsersWidget(QWidget *parent) : QWidget(parent), ui(new Ui::UsersWidget)
{
    ui->setupUi(this);
}

UsersWidget::~UsersWidget()
{
    delete ui;
}
