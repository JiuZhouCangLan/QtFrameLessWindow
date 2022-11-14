#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "NativeWindowTemplate.hpp"
#include <QString>
#include <QMainWindow>

namespace Ui
{
    class MainWindow;
}

class MainWindow : public NativeWindowTemplate<QMainWindow>
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_btnMin_clicked();
    void on_btnMax_clicked();
    void on_btnClose_clicked();
    void on_btnResizeable_clicked();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
