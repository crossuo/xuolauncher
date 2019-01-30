// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include "managerwindow.h"
#include <QApplication>
#include <QProcess>
#include <QFileInfo>
#include <QThread>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ManagerWindow w;
    w.show();
    return a.exec();
}
