﻿#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QRect>
#include <QtDebug>

MainWindow::MainWindow(QWidget *parent) :
    NativeWindowTemplate<QMainWindow>(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
#ifdef Q_OS_WIN
    //feel free to change this number to see how it works
    setResizeableAreaWidth(8);

    //set titlebar widget, wo we can drag MainWindow by it
    setTitleBar(ui->widgetTitlebar);

    //labelTitleText is a child widget of widgetTitlebar
    //add labelTitleText to ignore list, so we can drag MainWindow by it too
    //addIgnoreWidget(ui->labelTitleText);

    //further more, btnMin/btnMax... are child widgets of widgetTitlebar too
    //but we DO NOT want to drag MainWindow by them
#endif
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_btnMin_clicked()
{
    showMinimized();
}

void MainWindow::on_btnMax_clicked()
{
    if (isMaximized()) showNormal();
    else showMaximized();
}

void MainWindow::on_btnClose_clicked()
{
    close();
}

void MainWindow::on_btnResizeable_clicked()
{
    setResizeable(!isResizeable());
}

