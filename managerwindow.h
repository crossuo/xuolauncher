// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#pragma once

#include <QMainWindow>
#include <QCloseEvent>
#include <QKeyEvent>
#include "updater/updatemanager.h"
#include <QTimer>

namespace Ui
{
class ManagerWindow;
}

class ManagerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ManagerWindow(QWidget *parent = nullptr);
    ~ManagerWindow();

protected:
    void closeEvent(QCloseEvent *event);
    void keyPressEvent(QKeyEvent *event);

private slots:
    void on_lw_XUOAFeaturesOptions_clicked(const QModelIndex &index);
    void on_lw_XUOAFeaturesScripts_clicked(const QModelIndex &index);
    void on_rb_XUOAFeaturesSphere_clicked();
    void on_rb_XUOAFeaturesRunUO_clicked();
    void on_rb_XUOAFeaturesPOL_clicked();

    void on_lw_XuoFeaturesOptions_clicked(const QModelIndex &index);
    void on_rb_XuoFeaturesSphere_clicked();
    void on_rb_XuoFeaturesRunUO_clicked();
    void on_rb_XuoFeaturesPOL_clicked();

    void on_pb_Process_clicked();
    void on_tb_SetReleasePath_clicked();
    void on_tw_Main_currentChanged(int index);

    void onUpdatesListReceived(const QList<CFileInfo> &list);
    void onPackageListReceived(const QMap<QString, QMap<QString, CReleaseInfo>> &packages);

signals:
    void updatesListReceived(const QList<CFileInfo> &);
    void packageListReceived(const QMap<QString, QMap<QString, CReleaseInfo>> &);
    void fileReceived(const QDir &);
    void autoUpdateProgress(int);
    void autoUpdateNotification();

private:
    Ui::ManagerWindow *ui = nullptr;
    UpdateManager *m_UpdateManager = nullptr;

    QString boolToText(const bool &value);
    bool rawStringToBool(QString value);

    void refreshUpdates();
    void updateXUOAFeaturesCode();
    void updateXuoFeaturesCode();
};
