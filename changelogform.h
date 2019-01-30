/***********************************************************************************
**
** ChangelogForm.h
**
** Copyright (C) October 2017 Hotride
** Copyright (C) December 2018 Danny Angelo Carminati Grein
**
************************************************************************************
*/
#pragma once

#include <QMainWindow>
#include <QUrl>
#include "updater/updatemanager.h"
#include "updater/updateinfo.h"

namespace Ui
{
class ChangelogForm;
}

class ChangelogForm : public QMainWindow
{
    Q_OBJECT

public:
    explicit ChangelogForm(QWidget *parent = nullptr);
    ~ChangelogForm();

public slots:
    void onChangelogReceived(const QString &str);
    void onStartLink(const QUrl &url);

signals:
    void changelogReceived(const QString &);

private:
    Ui::ChangelogForm *ui = nullptr;
};
