#pragma once

#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>

class SelectionToolbar : public QWidget
{
    Q_OBJECT

public:
    SelectionToolbar(QWidget *parent = nullptr);

    void positionNear(const QRect &selection);

signals:
    void copyRequested();
    void saveRequested();
    void ocrRequested();
    void cancelRequested();

private:
    void setupUI();

    QPushButton *copyBtn;
    QPushButton *saveBtn;
    QPushButton *ocrBtn;
    QPushButton *cancelBtn;
};