#pragma once

#include <QMainWindow>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void takeScreenshot();
    void showWidget();
    void hideWidget();

private:
    QWidget *centralWidget;
    QVBoxLayout *layout;
    QPushButton *screenshotButton;
    QLabel *statusLabel;
};