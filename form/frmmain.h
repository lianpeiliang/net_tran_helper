#ifndef FRMMAIN_H
#define FRMMAIN_H

#include "framelesswindow.h"

namespace Ui {
class frmMain;
}

class frmMain : public FramelessWindow
{
    Q_OBJECT

public:
    explicit frmMain(QWidget *parent = 0);
    ~frmMain();

private slots:
    void on_tabWidget_currentChanged(int index);


private:
    Ui::frmMain *ui;
};

#endif // FRMMAIN_H
