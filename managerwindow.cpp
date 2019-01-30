// GPLv3 License
// Copyright (c) 2019 Danny Angelo Carminati Grein

#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QApplication>
#include <QtWidgets>
#include <assert.h>

#include "managerwindow.h"
#include "updater/qzipreader_p.h"
#include "ui_managerwindow.h"

#define MANAGER_TITLE "X:UO Manager v" APP_VERSION

#if defined(QT_NO_DEBUG)
#define UPDATER_HOST "http://update.crossuo.com/"
#else
#define UPDATER_HOST "http://update.crossuo.com/"
//#define UPDATER_HOST "http://192.168.2.14:8089/"
#endif

#if BUILD_WINDOWS
#define EXE_EXTENSION ".exe"
#define GetPlatformName() "win64" // FIXME
#else
#define EXE_EXTENSION ""
#endif

#if BUILD_LINUX
static QString distroName;

QString GetPlatformName()
{
    if (!distroName.isEmpty())
        return distroName;

    QString distro;
    distroName = "ubuntu";
    QFile file("/etc/lsb-release");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        while (!in.atEnd())
        {
           auto line = in.readLine().split("=");
           if (line.count() == 2 && line[0] == "DISTRIB_ID")
           {
               distro = line[1];
               distroName = line[1].toLower();
               break;
           }
        }
    }

    if (distroName != "manjarolinux" && distroName != "ubuntu")
    {
        QMessageBox::warning(nullptr, "Warning", QString("The %1 distribution is unsupported, you may find issues trying to use this binary").arg(distro));
    }

    return distroName;
}
#endif

#if BUILD_MACOS
#define GetPlatformName() "osx" // FIXME
#endif

ManagerWindow::ManagerWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::ManagerWindow)
{
    ui->setupUi(this);

    m_UpdateManager = new UpdateManager(UPDATER_HOST, MANAGER_TITLE, this);
    m_UpdateManager->setCacheDirectory(qApp->applicationDirPath() + "/cache");
    connect(
        m_UpdateManager,
        &UpdateManager::packageListReceived,
        this,
        &ManagerWindow::onPackageListReceived);
    connect(
        m_UpdateManager,
        &UpdateManager::updatesListReceived,
        this,
        &ManagerWindow::onUpdatesListReceived);

    connect(
        this,
        SIGNAL(updatesListReceived(const QList<CUpdateInfo> &)),
        this,
        SLOT(onUpdatesListReceived(const QList<CUpdateInfo> &)));
    connect(
        this,
        SIGNAL(packageListReceived(const QMap<QString, QMap<QString, CReleaseInfo>> &)),
        this,
        SLOT(onPackageListReceived(const QMap<QString, QMap<QString, CReleaseInfo>> &)));
    connect(&m_UpdatesTimer, SIGNAL(timeout()), this, SLOT(onUpdatesTimer()));

    setFixedSize(size());

    updateXUOAFeaturesCode();
    updateXuoFeaturesCode();
    setWindowTitle(MANAGER_TITLE);
    m_Loading = false;
    m_UpdatesTimer.start(15 * 60 * 1000);
}

ManagerWindow::~ManagerWindow()
{
    delete ui;
    delete m_UpdateManager;
    m_UpdateManager = nullptr;
    m_UpdatesTimer.stop();
}

void ManagerWindow::on_tb_SetReleasePath_clicked()
{
    auto path = ui->le_ReleasePath->text();
    if (!path.length())
        path = QCoreApplication::applicationDirPath();

    path = QFileDialog::getExistingDirectory(nullptr, tr("Select release directory"), path);
    if (path.length())
    {
        auto fullname = path + "/win64.manifest.xml";
        auto fi = QFileInfo(fullname);
        if (fi.exists())
        {
            ui->le_ReleasePath->setText(path);
            ui->pb_Process->setEnabled(true);
        }
        else
        {
            ui->pb_Process->setEnabled(false);
        }
    }
}

void ManagerWindow::onUpdatesTimer()
{
    on_pb_CheckUpdates_clicked();
}

void ManagerWindow::closeEvent(QCloseEvent *event)
{
    event->accept();
}

void ManagerWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    if (event->key() == Qt::Key_Delete)
    {
        //QWidget *focused = QApplication::focusWidget();

    }

    event->accept();
}

bool ManagerWindow::rawStringToBool(QString value)
{
    value = value.toLower();
    bool result = false;

    if (value == "true" || value == "on")
        result = true;
    else
        result = (value.toInt() != 0);

    return result;
}

void ManagerWindow::on_pb_Process_clicked()
{
    if (ui->pb_Process->isEnabled())
    {
        ui->pb_Process->setEnabled(false);
        QMessageBox::information(this, "Publishing", tr("Please wait, this may take some time."));
        const auto &path = ui->le_ReleasePath->text();
        const auto &plat = ui->cb_ReleasePlatform->currentText();
        const auto &prod = ui->cb_ReleaseProduct->currentText();
        const auto &ver = ui->le_ReleaseVersion->text();
        m_UpdateManager->generateUpdate(path, plat, prod, ver, this);
        QMessageBox::information(this, "Publishing", tr("Done!"));
        ui->pb_Process->setEnabled(true);
    }
}

void ManagerWindow::on_lw_XUOAFeaturesOptions_clicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    updateXUOAFeaturesCode();
}

void ManagerWindow::on_lw_XUOAFeaturesScripts_clicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    updateXUOAFeaturesCode();
}

void ManagerWindow::on_rb_XUOAFeaturesSphere_clicked()
{
    updateXUOAFeaturesCode();
}

void ManagerWindow::on_rb_XUOAFeaturesRunUO_clicked()
{
    updateXUOAFeaturesCode();
}

void ManagerWindow::on_rb_XUOAFeaturesPOL_clicked()
{
    updateXUOAFeaturesCode();
}

void ManagerWindow::updateXUOAFeaturesCode()
{
    quint64 featuresFlags = 0;
    quint64 scriptGroupsFlags = 0;

    for (auto i = 0; i < ui->lw_XUOAFeaturesOptions->count(); i++)
    {
        auto item = ui->lw_XUOAFeaturesOptions->item(i);
        if (item != nullptr && item->checkState() == Qt::Checked)
            featuresFlags |= (quint64(1) << i);
    }

    for (auto i = 0; i < ui->lw_XUOAFeaturesScripts->count(); i++)
    {
        auto item = ui->lw_XUOAFeaturesScripts->item(i);
        if (item != nullptr && item->checkState() == Qt::Checked)
            scriptGroupsFlags |= (quint64(1) << i);
    }

    QString code;
    if (ui->rb_XUOAFeaturesSphere->isChecked())
    {
        code.sprintf(
            "//data for sendpacket\nB0FC W015 W0A001 D0%08X D0%08X D0%08X D0%08X",
            uint((featuresFlags >> 32) & 0xFFFFFFFF),
            uint(featuresFlags & 0xFFFFFFFF),
            uint((scriptGroupsFlags >> 32) & 0xFFFFFFFF),
            uint(scriptGroupsFlags & 0xFFFFFFFF));
    }
    else if (ui->rb_XUOAFeaturesRunUO->isChecked())
    {
        code.sprintf(
            "public sealed class XUOAFeatures : Packet\n"
            "{\n"
            "public XUOAFeatures() : base(0xFC)\n"
            "{\n"
            "EnsureCapacity(21);\n"
            "m_Stream.Write((ushort)0xA001);\n"
            "m_Stream.Write((uint)0x%08X);\n"
            "m_Stream.Write((uint)0x%08X);\n"
            "m_Stream.Write((uint)0x%08X);\n"
            "m_Stream.Write((uint)0x%08X);\n"
            "}\n"
            "}",
            uint((featuresFlags >> 32) & 0xFFFFFFFF),
            uint(featuresFlags & 0xFFFFFFFF),
            uint((scriptGroupsFlags >> 32) & 0xFFFFFFFF),
            uint(scriptGroupsFlags & 0xFFFFFFFF));
    }
    else if (ui->rb_XUOAFeaturesPOL->isChecked())
    {
        code.sprintf(
            "program XUOAFeatures_sendpacket(who)\n"
            "var res := SendPacket(who, \"FC0015A001%08X%08X%08X%08X\");\n"
            "if (!res)\n"
            "print(\"SendPacket error: \" + res.errortext );\n"
            "endif\n"
            "endprogram",
            uint((featuresFlags >> 32) & 0xFFFFFFFF),
            uint(featuresFlags & 0xFFFFFFFF),
            uint((scriptGroupsFlags >> 32) & 0xFFFFFFFF),
            uint(scriptGroupsFlags & 0xFFFFFFFF));
    }
    ui->pte_XUOAFeaturesCode->setPlainText(code);
}

void ManagerWindow::onUpdatesListReceived(const QList<CFileInfo> &list)
{
    ui->lw_AvailableUpdates->clear();
    for (const auto &info : list)
    {
        ui->lw_AvailableUpdates->addItem(new CUpdateInfoListWidgetItem(info));
    }

    if (ui->lw_AvailableUpdates->count())
        ui->tw_Main->setCurrentIndex(1);

    ui->lw_Packages->setEnabled(true);
}

void ManagerWindow::onPackageListReceived(const QMap<QString, QMap<QString, CReleaseInfo>> &packages)
{
    ui->lw_Packages->clear();
    for (const auto &p : packages.keys())
    {
        if (p == "all")
            continue;

        for (const auto &v : packages[p].keys())
        {
            ui->lw_Packages->addItem(new CPackageInfoListWidgetItem(packages[p][v]));
        }
    }
    ui->lw_Packages->sortItems(Qt::SortOrder::DescendingOrder);
}

void ManagerWindow::on_pb_CheckUpdates_clicked()
{
    ui->lw_Packages->setEnabled(false);
    ui->lw_AvailableUpdates->clear();
    ui->lw_Packages->clear();
    m_UpdateManager->getManifest(QString("release/%1%2.manifest.xml").arg(GetPlatformName()).arg(""));
}

void ManagerWindow::on_lw_XuoFeaturesOptions_clicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    updateXuoFeaturesCode();
}

void ManagerWindow::on_rb_XuoFeaturesSphere_clicked()
{
    updateXuoFeaturesCode();
}

void ManagerWindow::on_rb_XuoFeaturesRunUO_clicked()
{
    updateXuoFeaturesCode();
}

void ManagerWindow::on_rb_XuoFeaturesPOL_clicked()
{
    updateXuoFeaturesCode();
}

void ManagerWindow::updateXuoFeaturesCode()
{
    uint featuresFlags = 0;

    for (int i = 0; i < ui->lw_XuoFeaturesOptions->count(); i++)
    {
        const auto item = ui->lw_XuoFeaturesOptions->item(i);
        if (item != nullptr && item->checkState() == Qt::Checked)
            featuresFlags |= (1 << i);
    }

    QString code;
    if (ui->rb_XuoFeaturesSphere->isChecked())
    {
        code.sprintf("//data for sendpacket\nB0FC W0009 W0032 D0%08X", featuresFlags);
    }
    else if (ui->rb_XuoFeaturesRunUO->isChecked())
    {
        code.sprintf(
            "public sealed class XUOAFeatures : Packet\n"
            "{\n"
            "public XUOAFeatures() : base(0xFC)\n"
            "{\n"
            "EnsureCapacity(9);\n"
            "m_Stream.Write((ushort)0x0032);\n"
            "m_Stream.Write((uint)0x%08X);\n"
            "}\n"
            "}",
            featuresFlags);
    }
    else if (ui->rb_XuoFeaturesPOL->isChecked())
    {
        code.sprintf(
            "program XUOAFeatures_sendpacket(who)\n"
            "var res := SendPacket(who, \"FC00090032%08X\");\n"
            "if (!res)\n"
            "print(\"SendPacket error: \" + res.errortext );\n"
            "endif\n"
            "endprogram",
            featuresFlags);
    }

    ui->pte_XuoFeaturesCode->setPlainText(code);
}
