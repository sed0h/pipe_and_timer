#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_Tester_UD.h"
#include <mutex>

class Tester_UD : public QMainWindow
{
    Q_OBJECT

public:
    Tester_UD(QWidget *parent = Q_NULLPTR);
    ~Tester_UD();
    void progress_indicator_thread();
    void PrintStringInTextBrowser();

private:
    Ui::Tester_UDClass ui;
    std::mutex ui_mutex;
    std::thread *_download_progress_indicator_thread;
    volatile bool _running;

private slots:
    void on_pushButton_clicked();
};
