/***********************************************************************************
**
** LauncherWindow.h
**
** Copyright (C) December 2016 Hotride
** Copyright (C) December 2018 Danny Angelo Carminati Grein
**
************************************************************************************
*/
#pragma once

#include <QMainWindow>
#include <QCloseEvent>
#include <QComboBox>
#include <QKeyEvent>
#include "changelogform.h"
#include "updater/updatemanager.h"
#include <QTimer>

namespace Ui
{
class LauncherWindow;
}

class LauncherWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit LauncherWindow(QWidget *parent = nullptr);
    ~LauncherWindow();

protected:
    void closeEvent(QCloseEvent *event);
    void keyPressEvent(QKeyEvent *event);

private slots:
    void on_lw_ProfileList_clicked(const QModelIndex &index);
    void on_lw_ProfileList_doubleClicked(const QModelIndex &index);
    void on_cb_ServerShowPassword_clicked();
    void on_pb_ServerAdd_clicked();
    void on_pb_ServerSave_clicked();
    void on_pb_ServerRemove_clicked();
    void on_le_CommandLine_textChanged(const QString &arg1);
    void on_lw_ProxyList_clicked(const QModelIndex &index);
    void on_cb_ProxyShowPassword_clicked();
    void on_pb_ProxyAdd_clicked();
    void on_pb_ProxySave_clicked();
    void on_pb_ProxyRemove_clicked();
    void on_tb_SetXuoPath_clicked();
    void on_pb_Launch_clicked();
    void on_cb_LaunchAutologin_clicked();
    void on_cb_LaunchSavePassword_clicked();
    void on_cb_LaunchSaveAero_clicked();
    void on_cb_LaunchFastLogin_clicked();
    void on_cb_LaunchRunUOAM_clicked();
    void on_pb_CheckUpdates_clicked();
    void on_cb_XuoPath_currentIndexChanged(int index);
    void on_pb_ApplyUpdates_clicked();
    void on_pb_RestoreSelectedVersion_clicked();
    void on_pb_ShowChangelog_clicked();
    void on_lw_Packages_doubleClicked(const QModelIndex &index);
    void on_tb_SetClientPath_clicked();
    void on_cb_Beta_clicked();
    void on_pb_Github_clicked();
    void on_pb_TeamMember_clicked();
    void on_pb_Website_clicked();
    void on_pb_Discord_clicked();

    void onUpdatesListReceived(const QList<CFileInfo> &list);
    void onPackageListReceived(const QMap<QString, QMap<QString, CReleaseInfo>> &packages);
    void onFileReceived(const QString &name);
    void onUpdatesTimer();
    void onDownloadProgress(qint64 bytesRead, qint64 totalBytes);
    void unzipPackage(const QString &filename, const QString &toPath);

signals:
    void updatesListReceived(const QList<CFileInfo> &);
    void packageListReceived(const QMap<QString, QMap<QString, CReleaseInfo>> &);
    void changelogReceived(const QString &);
    void fileReceived(const QDir &);
    void autoUpdateProgress(int);
    void autoUpdateNotification();

private:
    Ui::LauncherWindow *ui = nullptr;
    int m_FilesToUpdateCount = 0;
    int m_DownloadingPackageTotal = 0;
    bool m_Loading = true;
    bool m_LauncherFoundInUpdates = false;
    ChangelogForm *m_ChangelogForm = nullptr;
    UpdateManager *m_UpdateManager = nullptr;
    QTimer m_UpdatesTimer;

    void updateServerFields(const int &index);
    void writeCfg() const;
    void saveProxyList();
    void saveServerList();
    void loadProxyList();
    void loadServerList();
    bool checkUoDataPath(const QDir &path) const;
    bool validateProfile(bool checkSelected = false);

    void unselectCombobox(QComboBox *w);
    void unselectList(QListWidget *w);
    void setTextColor(QWidget *w, Qt::GlobalColor color);
    QString boolToText(const bool &value) const;
    bool rawStringToBool(QString value) const;

    QString decodeArgumentString(const char *text, const int &length) const;
    QString encodeArgumentString(const char *text, const int &length) const;

    void runProgram(const QString &exePath, const QStringList &args, const QString &directory);
};
