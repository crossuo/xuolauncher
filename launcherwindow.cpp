/***********************************************************************************
**
** LauncherWindow.cpp
**
** Copyright (C) December 2016 Hotride
** Copyright (C) December 2018 Danny Angelo Carminati Grein
**
************************************************************************************
*/

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

#include "launcherwindow.h"
#include "proxylistitem.h"
#include "serverlistitem.h"
#include "updater/qzipreader_p.h"
#include "ui_launcherwindow.h"

#define LAUNCHER_TITLE "X:UO Launcher v" APP_VERSION

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

LauncherWindow::LauncherWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::LauncherWindow)
{
    ui->setupUi(this);

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    QDir dir(appData);
    if (!dir.exists()){
      dir.mkdir(appData);
    }

    m_ChangelogForm = new ChangelogForm(nullptr);

    m_UpdateManager = new UpdateManager(UPDATER_HOST, LAUNCHER_TITLE, this);
    m_UpdateManager->setCacheDirectory(appData + "/cache");
    connect(
        m_UpdateManager,
        &UpdateManager::changelogReceived,
        m_ChangelogForm,
        &ChangelogForm::onChangelogReceived);
    connect(
        m_UpdateManager,
        &UpdateManager::packageListReceived,
        this,
        &LauncherWindow::onPackageListReceived);
    connect(
        m_UpdateManager,
        &UpdateManager::updatesListReceived,
        this,
        &LauncherWindow::onUpdatesListReceived);
    connect(
        m_UpdateManager,
        &UpdateManager::downloadProgress,
        this,
        &LauncherWindow::onDownloadProgress);

    /*connect(
        this,
        SIGNAL(updatesListReceived(const QList<CUpdateInfo> &)),
        this,
        SLOT(onUpdatesListReceived(const QList<CUpdateInfo> &)));*/
    connect(
        this,
        SIGNAL(packageListReceived(const QMap<QString, QMap<QString, CReleaseInfo>> &)),
        this,
        SLOT(onPackageListReceived(const QMap<QString, QMap<QString, CReleaseInfo>> &)));
    //connect(this, SIGNAL(fileReceived(const QDir &)), this, SLOT(onFileReceived(const QDir &)));
    connect(&m_UpdatesTimer, SIGNAL(timeout()), this, SLOT(onUpdatesTimer()));

    setFixedSize(size());

    loadProxyList();
    loadServerList();

#if !BUILD_WINDOWS
    ui->cb_LaunchRunUOAM->setEnabled(false);
    ui->cb_LaunchRunUOAM->setVisible(false);
    ui->cb_LaunchSaveAero->setEnabled(false);
    ui->cb_LaunchSaveAero->setVisible(false);
#endif

    ui->tw_Main->setCurrentIndex(0);
    ui->tw_Server->setCurrentIndex(0);

    setWindowTitle(LAUNCHER_TITLE);
    m_Loading = false;

    if (!ui->cb_XuoPath->currentText().length())
    {
        ui->cb_XuoPath->addItem(appData);
        ui->cb_XuoPath->setCurrentIndex(ui->cb_XuoPath->count() - 1);
    }

    if (ui->lw_ProfileList->count() == 0)
    {
        ui->pb_ServerAdd->setEnabled(false);
    }

    on_cb_XuoPath_currentIndexChanged(ui->cb_XuoPath->currentIndex());
    m_UpdatesTimer.start(15 * 60 * 1000);
}

LauncherWindow::~LauncherWindow()
{
    delete ui;

    delete m_ChangelogForm;
    m_ChangelogForm = nullptr;

    delete m_UpdateManager;
    m_UpdateManager = nullptr;

    m_UpdatesTimer.stop();
}

bool LauncherWindow::checkUoDataPath(const QDir &path) const
{
    const auto clientExe = QFileInfo(path.filePath("client.exe"));
    const auto loginCfg1 = QFileInfo(path.filePath("Login.cfg"));
    const auto loginCfg2 = QFileInfo(path.filePath("login.cfg"));

    return clientExe.exists() && (loginCfg1.exists() || loginCfg2.exists());
}

void LauncherWindow::setTextColor(QWidget *w, Qt::GlobalColor color)
{
    auto palette = w->palette();
    palette.setColor(w->foregroundRole(), color);
    w->setPalette(palette);
}

void LauncherWindow::unselectCombobox(QComboBox *w)
{
    w->setCurrentIndex(0);
}

void LauncherWindow::unselectList(QListWidget *w)
{
    auto selected = w->currentItem();
    if (selected)
    {
        auto model = w->selectionModel();
        model->reset();
    }
}

void LauncherWindow::onUpdatesTimer()
{
    if (ui->cb_CheckUpdates->isChecked())
        on_pb_CheckUpdates_clicked();
}

void LauncherWindow::closeEvent(QCloseEvent *event)
{
    saveServerList();
    saveProxyList();
    if (m_ChangelogForm != nullptr)
        m_ChangelogForm->close();

    event->accept();
}

void LauncherWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
        return;

    if (event->key() == Qt::Key_Delete)
    {
        QWidget *focused = QApplication::focusWidget();

        if (focused == ui->lw_ProfileList) // Servers list
            on_pb_ServerRemove_clicked();
        else if (focused == ui->lw_ProxyList) // Proxy list
            on_pb_ProxyRemove_clicked();
    }

    event->accept();
}

void LauncherWindow::on_lw_ProfileList_clicked(const QModelIndex &index)
{
    updateServerFields(index.row());
}

void LauncherWindow::updateServerFields(const int &index)
{
    auto item = static_cast<CServerListItem *>(ui->lw_ProfileList->item(index));
    if (item != nullptr)
    {
        ui->le_ProfileName->setText(item->text());
        ui->le_ServerAddress->setText(item->GetAddress());
        ui->le_ServerAccount->setText(item->GetAccount());
        ui->le_ServerPassword->setText(item->GetPassword());
        ui->le_ServerCharacter->setText(item->GetCharacter());
        ui->le_CommandLine->setText(item->GetCommand());

        ui->le_ServerClientVersion->setText(item->GetClientVersion());
        ui->le_ServerClientPath->setText(item->GetClientPath());
        ui->cb_ServerClientType->setCurrentIndex(item->GetClientType());
        ui->cb_ServerUseCrypt->setChecked(item->GetUseCrypt());

        ui->cb_XuoPath->setCurrentText(item->GetCrossUoPath());

        ui->cb_LaunchAutologin->setChecked(item->GetOptionAutologin());
        ui->cb_LaunchSavePassword->setChecked(item->GetOptionSavePassword());
        ui->cb_LaunchSaveAero->setChecked(item->GetOptionSaveAero());
        ui->cb_LaunchFastLogin->setChecked(item->GetOptionFastLogin());
        ui->cb_LaunchRunUOAM->setChecked(item->GetOptionRunUOAM());

        ui->cb_ServerUseProxy->setChecked(item->GetUseProxy());
        ui->cb_ServerProxy->setCurrentText(item->GetProxy());
    }
}

void LauncherWindow::on_lw_ProfileList_doubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index);

    on_pb_Launch_clicked();
}

void LauncherWindow::on_cb_ServerShowPassword_clicked()
{
    if (ui->cb_ServerShowPassword->isChecked())
        ui->le_ServerPassword->setEchoMode(QLineEdit::EchoMode::Normal);
    else
        ui->le_ServerPassword->setEchoMode(QLineEdit::EchoMode::Password);
}

bool LauncherWindow::validateProfile(bool checkSelected)
{
    auto selected = static_cast<CServerListItem *>(ui->lw_ProfileList->currentItem());
    if (checkSelected && selected == nullptr)
    {
        QMessageBox::critical(this, tr("No profile selected"), tr("Select a profile where to save."));
        return false;
    }

    auto name = ui->le_ProfileName->text().toLower();
    if (!name.length())
    {
        QMessageBox::critical(this, tr("Profile name is empty"), tr("Please enter the profile name."));
        setTextColor(ui->lb_profileName, Qt::red);
        ui->le_ProfileName->focusWidget();
        return false;
    }

    bool uniqueName = true;
    for (int i = 0; i < ui->lw_ProfileList->count(); i++)
    {
        auto item = ui->lw_ProfileList->item(i);
        if (item->text().toLower() == name)
        {
            uniqueName &= checkSelected ? selected && item == selected : false;
        }
    }

    if (!uniqueName)
    {
        QMessageBox::critical(this, tr("Profile name already exists"), tr("Profile name already exists, please chose a new one."));
        setTextColor(ui->lb_profileName, Qt::red);
        ui->le_ProfileName->focusWidget();
        return false;
    }
    setTextColor(ui->lb_profileName, Qt::black);

    auto uoDataPath = ui->le_ServerClientPath->text();
    if (!uoDataPath.length())
    {
        QMessageBox::critical(this, tr("Ultima Online(tm) install folder"), tr("Please enter the path where to find the Ultima Online(tm) installation."));
        setTextColor(ui->lb_clientPath, Qt::red);
        ui->le_ServerClientPath->focusWidget();
        return false;
    }

    auto validPath = checkUoDataPath(uoDataPath);
    if (!validPath)
    {
        auto q = QMessageBox::question(this, tr("Invalid Ultima Online(tm) Path"),
                    tr("Couldn't find Ultima Online(tm) files (client.exe or login.cfg) in this path, do you want to continue?"),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (q == QMessageBox::No)
        {
            setTextColor(ui->lb_clientPath, Qt::red);
            ui->le_ServerClientPath->focusWidget();
            return false;
        }
    }
    setTextColor(ui->lb_clientPath, Qt::black);
    return true;
}

void LauncherWindow::on_pb_ServerAdd_clicked()
{
    unselectList(ui->lw_ProfileList);

    ui->le_ProfileName->clear();
    ui->le_ServerAddress->clear();
    ui->le_ServerAccount->clear();
    ui->le_ServerCharacter->clear();
    ui->le_ServerPassword->clear();
    ui->le_ServerClientVersion->clear();
    ui->le_ServerClientPath->clear();

    unselectCombobox(ui->cb_ServerClientType);
    unselectCombobox(ui->cb_ServerProxy);

    ui->cb_ServerUseProxy->setChecked(false);
    ui->cb_ServerUseCrypt->setChecked(false);
}

void LauncherWindow::on_pb_ServerSave_clicked()
{
    auto selected = static_cast<CServerListItem *>(ui->lw_ProfileList->currentItem());
    const auto isNew = selected == nullptr;
    if (!validateProfile(!isNew))
    {
        return;
    }

    if (isNew)
    {
        auto item = new CServerListItem(
            ui->le_ProfileName->text(),
            ui->le_ServerAddress->text(),
            ui->le_ServerAccount->text(),
            ui->le_ServerPassword->text(),
            ui->le_ServerCharacter->text(),
            ui->le_ServerClientVersion->text(),
            ui->le_ServerClientPath->text(),
            ui->cb_XuoPath->currentText(),
            ui->cb_ServerClientType->currentIndex(),
            ui->cb_ServerProxy->currentText(),
            ui->cb_ServerUseProxy->isChecked(),
            ui->cb_ServerUseCrypt->isChecked());
            ui->lw_ProfileList->addItem(item);

        const auto pos = ui->lw_ProfileList->count() - 1;
        selected = static_cast<CServerListItem *>(ui->lw_ProfileList->item(pos));
    }

    selected->setText(ui->le_ProfileName->text());
    selected->SetAddress(ui->le_ServerAddress->text());
    selected->SetAccount(ui->le_ServerAccount->text());
    selected->SetCharacter(ui->le_ServerCharacter->text());
    selected->SetPassword(ui->le_ServerPassword->text());
    selected->SetClientVersion(ui->le_ServerClientVersion->text());
    selected->SetClientPath(ui->le_ServerClientPath->text());
    selected->SetCrossUoPath(ui->cb_XuoPath->currentText());
    selected->SetClientType(ui->cb_ServerClientType->currentIndex());
    selected->SetProxy(ui->cb_ServerProxy->currentText());
    selected->SetUseProxy(ui->cb_ServerUseProxy->isChecked());
    selected->SetUseCrypt(ui->cb_ServerUseCrypt->isChecked());

    if (ui->lw_ProfileList->count() > 0)
    {
        ui->pb_ServerAdd->setEnabled(true);
    }

    saveServerList();
}

void LauncherWindow::on_pb_ServerRemove_clicked()
{
    QListWidgetItem *item = ui->lw_ProfileList->currentItem();

    if (item != nullptr)
    {
        item = ui->lw_ProfileList->takeItem(ui->lw_ProfileList->row(item));

        if (item != nullptr)
        {
            delete item;

            if (ui->lw_ProfileList->count() == 0)
            {
                ui->pb_ServerAdd->setEnabled(false);
            }

            saveServerList();
        }
    }
}

void LauncherWindow::on_le_CommandLine_textChanged(const QString &arg1)
{
    auto selected = static_cast<CServerListItem *>(ui->lw_ProfileList->currentItem());

    if (selected != nullptr)
        selected->SetCommand(arg1);
}

void LauncherWindow::on_lw_ProxyList_clicked(const QModelIndex &index)
{
    auto item = static_cast<CProxyListItem *>(ui->lw_ProxyList->item(index.row()));
    if (item != nullptr)
    {
        ui->le_ProxyName->setText(item->text());
        ui->le_ProxyAddress->setText(item->GetAddress());
        ui->le_ProxyPort->setText(item->GetProxyPort());
        ui->gb_ProxySocks5->setChecked(item->GetSocks5());
        ui->le_ProxyAccount->setText(item->GetAccount());
        ui->le_ProxyPassword->setText(item->GetPassword());
    }
}

void LauncherWindow::on_cb_ProxyShowPassword_clicked()
{
    if (ui->cb_ProxyShowPassword->isChecked())
        ui->le_ProxyPassword->setEchoMode(QLineEdit::EchoMode::Normal);
    else
        ui->le_ProxyPassword->setEchoMode(QLineEdit::EchoMode::Password);
}

void LauncherWindow::on_pb_ProxyAdd_clicked()
{
    auto name = ui->le_ProxyName->text().toLower();
    if (!name.length())
    {
        QMessageBox::critical(this, tr("Proxy name is empty"), tr("Enter the proxy server name."));
        return;
    }

    for (int i = 0; i < ui->lw_ProxyList->count(); i++)
    {
        auto item = ui->lw_ProxyList->item(i);
        if (item != nullptr && item->text().toLower() == name)
        {
            QMessageBox::critical(
                this, tr("Name is already exists"), tr("Proxy server name is already exists!"));
            return;
        }
    }

    ui->lw_ProxyList->addItem(new CProxyListItem(
        ui->le_ProxyName->text(),
        ui->le_ProxyAddress->text(),
        ui->le_ProxyPort->text(),
        ui->gb_ProxySocks5->isChecked(),
        ui->le_ProxyAccount->text(),
        ui->le_ProxyPassword->text()));

    ui->lw_ProxyList->setCurrentRow(ui->lw_ProxyList->count() - 1);

    saveProxyList();

    ui->cb_ServerProxy->addItem(ui->le_ProxyName->text());
}

void LauncherWindow::on_pb_ProxySave_clicked()
{
    QString name = ui->le_ProxyName->text().toLower();
    if (!name.length())
    {
        QMessageBox::critical(this, tr("Name is empty"), tr("Enter the proxy server name."));
        return;
    }

    auto selected = static_cast<CProxyListItem *>(ui->lw_ProxyList->currentItem());
    if (selected == nullptr)
    {
        QMessageBox::critical(this, tr("No item selected"), tr("No proxy selected."));
        return;
    }

    for (int i = 0; i < ui->lw_ProxyList->count(); i++)
    {
        auto item = ui->lw_ProxyList->item(i);
        if (item != nullptr && item->text().toLower() == name)
        {
            if (item != nullptr && item != selected)
            {
                QMessageBox::critical(
                    this,
                    tr("Name already exists"),
                    tr("Proxy server name already exists (not this item)."));
                return;
            }

            break;
        }
    }

    selected->setText(ui->le_ProxyName->text());
    selected->SetAddress(ui->le_ProxyAddress->text());
    selected->SetProxyPort(ui->le_ProxyPort->text());
    selected->SetSocks5(ui->gb_ProxySocks5->isChecked());
    selected->SetAccount(ui->le_ProxyAccount->text());
    selected->SetPassword(ui->le_ProxyPassword->text());
    saveProxyList();

    ui->cb_ServerProxy->setItemText(ui->lw_ProxyList->currentRow(), ui->le_ProxyName->text());
}

void LauncherWindow::on_pb_ProxyRemove_clicked()
{
    auto item = ui->lw_ProxyList->currentItem();
    if (item != nullptr)
    {
        int index = ui->lw_ProxyList->row(item);
        item = ui->lw_ProxyList->takeItem(index);

        if (item != nullptr)
        {
            ui->cb_ServerProxy->removeItem(index);
            delete item;
            saveProxyList();
        }
    }
}

QString LauncherWindow::boolToText(const bool &value) const
{
    return value ? "true" : "false";
}

bool LauncherWindow::rawStringToBool(QString value) const
{
    value = value.toLower();
    bool result = false;

    if (value == "true" || value == "on")
        result = true;
    else
        result = (value.toInt() != 0);

    return result;
}

void LauncherWindow::writeCfg() const
{
    auto item = static_cast<CServerListItem *>(ui->lw_ProfileList->currentItem());
    if (item == nullptr)
    {
        return;
    }
    const auto clientPath = ui->cb_XuoPath->currentText();
    QFile file(clientPath + "/crossuo.cfg");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream stream(&file);
        stream << "AcctID=" << item->GetAccount() << endl;
        if (item->GetOptionSavePassword())
        {
            stream << "AcctPassword=" << item->GetPassword() << endl;
            stream << "RememberAcctPW=yes" << endl;
        }
        stream << "AutoLogin=" << (item->GetOptionFastLogin() ? "yes" : "no") << endl;
        stream << "Crypt=" << (item->GetUseCrypt() ? "yes" : "no") << endl;

        if (!item->GetClientTypeString().isEmpty())
            stream << "ClientType=" << item->GetClientTypeString() << endl;
        stream << "ClientVersion=" << item->GetClientVersion() << endl;
        stream << "CustomPath=" << item->GetClientPath() << endl;

        if (!item->GetAddress().isEmpty())
        {
            stream << "LoginServer=" << item->GetAddress() << endl;
        }
    }
}

void LauncherWindow::saveProxyList()
{
    QFile file(QCoreApplication::applicationDirPath() + "/proxy.xml");

    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QXmlStreamWriter writer(&file);
        writer.setAutoFormatting(true);
        writer.writeStartDocument();

        int count = ui->lw_ProxyList->count();
        writer.writeStartElement("proxylist");
        writer.writeAttribute("version", "0");
        writer.writeAttribute("size", QString::number(count));

        for (int i = 0; i < count; i++)
        {
            auto item = static_cast<CProxyListItem *>(ui->lw_ProxyList->item(i));
            if (item != nullptr)
            {
                writer.writeStartElement("proxy");
                writer.writeAttribute("name", item->text());
                writer.writeAttribute("address", item->GetAddress());
                writer.writeAttribute("port", item->GetProxyPort());
                writer.writeAttribute("socks5", boolToText(item->GetSocks5()));
                writer.writeAttribute("account", item->GetAccount());
                writer.writeAttribute("password", item->GetPassword());
                writer.writeEndElement(); // proxy
            }
        }
        writer.writeEndElement(); // proxylist
        writer.writeEndDocument();
        file.close();
    }
}

void LauncherWindow::saveServerList()
{
    QFile file(QCoreApplication::applicationDirPath() + "/server.xml");
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QXmlStreamWriter writer(&file);
        writer.setAutoFormatting(true);
        writer.writeStartDocument();

        int count = ui->lw_ProfileList->count();
        writer.writeStartElement("serverlist");
        writer.writeAttribute("version", "0");
        writer.writeAttribute("size", QString::number(count));
        writer.writeAttribute(
            "closeafterlaunch", boolToText(ui->cb_LaunchCloseAfterLaunch->isChecked()));
        writer.writeAttribute("lastserver", QString::number(ui->lw_ProfileList->currentRow()));
        writer.writeAttribute("checkupdates", boolToText(ui->cb_CheckUpdates->isChecked()));
        writer.writeAttribute("beta", boolToText(ui->cb_Beta->isChecked()));

        for (int i = 0; i < ui->cb_XuoPath->count(); i++)
        {
            writer.writeStartElement("clientpath");
            writer.writeAttribute("path", ui->cb_XuoPath->itemText(i));
            writer.writeEndElement(); // clientpath
        }

        for (int i = 0; i < count; i++)
        {
            auto item = static_cast<CServerListItem *>(ui->lw_ProfileList->item(i));
            if (item != nullptr)
            {
                writer.writeStartElement("server");
                writer.writeAttribute("name", item->text());
                writer.writeAttribute("address", item->GetAddress());
                writer.writeAttribute("account", item->GetAccount());
                writer.writeAttribute("password", item->GetPassword());
                writer.writeAttribute("character", item->GetCharacter());
                writer.writeAttribute("command", item->GetCommand());
                writer.writeAttribute("useproxy", boolToText(item->GetUseProxy()));
                writer.writeAttribute("proxyname", item->GetProxy());
                writer.writeAttribute("optionautologin", boolToText(item->GetOptionAutologin()));
                writer.writeAttribute(
                    "optionsavepassword", boolToText(item->GetOptionSavePassword()));
                writer.writeAttribute("optionsaveaero", boolToText(item->GetOptionSaveAero()));
                writer.writeAttribute("optionfastlogin", boolToText(item->GetOptionFastLogin()));
                writer.writeAttribute("optionrunuoam", boolToText(item->GetOptionRunUOAM()));

                writer.writeAttribute("clientversion", item->GetClientVersion());
                writer.writeAttribute("clientpath", item->GetClientPath());                
                writer.writeAttribute("clienttype", item->GetClientTypeString());
                writer.writeAttribute("usecrypt", boolToText(item->GetUseCrypt()));
                writer.writeAttribute("crossuopath", item->GetCrossUoPath());

                writer.writeEndElement(); // server
            }
        }
        writer.writeEndElement(); // serverlist
        writer.writeEndDocument();
        file.close();
    }
}

void LauncherWindow::loadProxyList()
{
    ui->lw_ProxyList->clear();

    QFile file(QCoreApplication::applicationDirPath() + "/proxy.xml");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QXmlStreamReader reader(&file);
        int version = 0;
        int count = 0;
        while (!reader.atEnd() && !reader.hasError())
        {
            if (reader.isStartElement())
            {
                auto attributes = reader.attributes();
                if (reader.name() == "proxylist")
                {
                    if (attributes.hasAttribute("version"))
                        version = attributes.value("version").toInt();

                    if (attributes.hasAttribute("size"))
                        count = attributes.value("size").toInt();
                }
                else if (reader.name() == "proxy")
                {
                    if (attributes.hasAttribute("name"))
                    {
                        auto item = new CProxyListItem(attributes.value("name").toString());

                        if (attributes.hasAttribute("address"))
                            item->SetAddress(attributes.value("address").toString());

                        if (attributes.hasAttribute("port"))
                            item->SetProxyPort(attributes.value("port").toString());

                        if (attributes.hasAttribute("socks5"))
                            item->SetSocks5(rawStringToBool(attributes.value("socks5").toString()));

                        if (attributes.hasAttribute("account"))
                            item->SetAccount(attributes.value("account").toString());

                        if (attributes.hasAttribute("password"))
                            item->SetPassword(attributes.value("password").toString());

                        ui->lw_ProxyList->addItem(item);
                        ui->cb_ServerProxy->addItem(attributes.value("name").toString());
                    }
                }
            }
            reader.readNext();
        }
        file.close();
    }
}

void LauncherWindow::loadServerList()
{
    ui->lw_ProfileList->clear();

    QFile file(QCoreApplication::applicationDirPath()+ "/server.xml");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QXmlStreamReader reader(&file);

        int version = 0;
        int count = 0;
        int clientindex = -1;
        int lastServer = -1;

        ui->cb_XuoPath->clear();
        while (!reader.atEnd() && !reader.hasError())
        {
            if (reader.isStartElement())
            {
                auto attributes = reader.attributes();
                if (reader.name() == "serverlist")
                {
                    if (attributes.hasAttribute("version"))
                        version = attributes.value("version").toInt();

                    if (attributes.hasAttribute("size"))
                        count = attributes.value("size").toInt();

                    if (attributes.hasAttribute("clientindex"))
                        clientindex = attributes.value("clientindex").toInt();

                    if (attributes.hasAttribute("closeafterlaunch"))
                        ui->cb_LaunchCloseAfterLaunch->setChecked(
                            rawStringToBool(attributes.value("closeafterlaunch").toString()));

                    if (attributes.hasAttribute("lastserver"))
                        lastServer = attributes.value("lastserver").toInt();

                    if (attributes.hasAttribute("path"))
                    {
                        auto path = attributes.value("path").toString().trimmed();
                        QFileInfo fi(path);
                        ui->cb_XuoPath->addItem(fi.absoluteFilePath());
                        clientindex = 0;
                    }

                    if (attributes.hasAttribute("checkupdates"))
                        ui->cb_CheckUpdates->setChecked(
                            rawStringToBool(attributes.value("checkupdates").toString()));

                    if (attributes.hasAttribute("beta"))
                        ui->cb_Beta->setChecked(
                            rawStringToBool(attributes.value("beta").toString()));
                }
                else if (reader.name() == "clientpath")
                {
                    if (attributes.hasAttribute("path"))
                    {
                        auto path = attributes.value("path").toString().trimmed();
                        QFileInfo fi(path);
                        ui->cb_XuoPath->addItem(fi.absoluteFilePath());
                        path = fi.absoluteFilePath();
                        bool found = false;
                        for (int i = 0; i < ui->cb_XuoPath->count(); i++)
                        {
                            if (path == ui->cb_XuoPath->itemText(i))
                            {
                                found = true;
                                break;
                            }
                        }

                        if (!found)
                            ui->cb_XuoPath->addItem(path);
                    }
                }
                else if (reader.name() == "server")
                {
                    if (attributes.hasAttribute("name"))
                    {
                        auto item = new CServerListItem(attributes.value("name").toString());

                        if (attributes.hasAttribute("address"))
                            item->SetAddress(attributes.value("address").toString());

                        if (attributes.hasAttribute("account"))
                            item->SetAccount(attributes.value("account").toString());

                        if (attributes.hasAttribute("password"))
                            item->SetPassword(attributes.value("password").toString());

                        if (attributes.hasAttribute("character"))
                            item->SetCharacter(attributes.value("character").toString());

                        if (attributes.hasAttribute("clientversion"))
                            item->SetClientVersion(attributes.value("clientversion").toString());

                        if (attributes.hasAttribute("clientpath"))
                            item->SetClientPath(attributes.value("clientpath").toString());

                        if (attributes.hasAttribute("clienttype"))
                            item->SetClientTypeFromString(attributes.value("clienttype").toString());

                        if (attributes.hasAttribute("crossuopath"))
                            item->SetCrossUoPath(attributes.value("crossuopath").toString());

                        if (attributes.hasAttribute("command"))
                            item->SetCommand(attributes.value("command").toString());

                        if (attributes.hasAttribute("usecrypt"))
                            item->SetUseCrypt(
                                rawStringToBool(attributes.value("usecrypt").toString()));

                        if (attributes.hasAttribute("useproxy"))
                            item->SetUseProxy(
                                rawStringToBool(attributes.value("useproxy").toString()));

                        if (attributes.hasAttribute("proxyname"))
                            item->SetProxy(attributes.value("proxyname").toString());

                        if (attributes.hasAttribute("optionautologin"))
                            item->SetOptionAutologin(
                                rawStringToBool(attributes.value("optionautologin").toString()));

                        if (attributes.hasAttribute("optionsavepassword"))
                            item->SetOptionSavePassword(
                                rawStringToBool(attributes.value("optionsavepassword").toString()));

                        if (attributes.hasAttribute("optionsaveaero"))
                            item->SetOptionSaveAero(
                                rawStringToBool(attributes.value("optionsaveaero").toString()));

                        if (attributes.hasAttribute("optionfastlogin"))
                            item->SetOptionFastLogin(
                                rawStringToBool(attributes.value("optionfastlogin").toString()));

                        if (attributes.hasAttribute("optionrunuoam"))
                            item->SetOptionRunUOAM(
                                rawStringToBool(attributes.value("optionrunuoam").toString()));

                        ui->lw_ProfileList->addItem(item);
                    }
                }
            }
            reader.readNext();
        }

        if (clientindex >= 0 && clientindex < ui->cb_XuoPath->count())
            ui->cb_XuoPath->setCurrentIndex(clientindex);

        if (lastServer >= 0 && lastServer < ui->lw_ProfileList->count())
        {
            ui->lw_ProfileList->setCurrentRow(lastServer);
            updateServerFields(lastServer);
        }
        file.close();
    }
}

void LauncherWindow::on_tb_SetClientPath_clicked()
{
    auto clientPath = QCoreApplication::applicationDirPath();
    auto selectedPath = ui->le_ServerClientPath->text();
    if (!selectedPath.isEmpty())
    {
        clientPath = selectedPath;
    }

    auto r = QMessageBox::Yes;
    auto path = clientPath;
    do
    {
        path = QFileDialog::getExistingDirectory(nullptr, tr("Select UO Client Directory"), clientPath);
        if (path.isEmpty())
        {
            return;
        }
        const QDir p(path);
        const bool isValid = checkUoDataPath(p);
        const auto outlands = QFileInfo(p.filePath("OutlandsUO.exe"));
        if (outlands.exists())
        {
            auto q = QMessageBox::question(this, tr("Is this Outlands?"), tr("Is this a Outlands Client Installation?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (q == QMessageBox::Yes)
            {
                ui->le_ServerClientVersion->setText("7.0.15.1");
                ui->cb_ServerClientType->setCurrentIndex(5); // FIXME: find the correct by name
                ui->le_ServerAddress->setText("play.uooutlands.com,2593");
            }
        }
        if (!isValid)
        {
            r = QMessageBox::warning(this, tr("WARNING"), tr("Couldn't find 'client.exe' or 'login.cfg'!\nAre you sure you want to use this path?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        }
    } while (r != QMessageBox::Yes);

    if (path.length())
    {
        ui->le_ServerClientPath->setText(path);
    }
}

void LauncherWindow::on_tb_SetXuoPath_clicked()
{
    auto startPath = ui->cb_XuoPath->currentText();
    if (!startPath.length())
        startPath = QCoreApplication::applicationDirPath();

    auto r = QMessageBox::Yes;
    auto path = startPath;
    do
    {
        path = QFileDialog::getExistingDirectory(nullptr, tr("Select CrossUO client directory"), startPath);
        if (path.isEmpty())
        {
            return;
        }
        if (checkUoDataPath(path))
        {
            r = QMessageBox::warning(this, "WARNING", tr("Setting crossuopath to the same as the original Ultima Online Client is not recommended!\nAre you sure to use this path?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        }
    } while (r != QMessageBox::Yes);

    if (path.length())
    {
        for (int i = 0; i < ui->cb_XuoPath->count(); i++)
        {
            if (path == ui->cb_XuoPath->itemText(i))
            {
                ui->cb_XuoPath->setCurrentIndex(i);
                return;
            }
        }
        ui->cb_XuoPath->addItem(path);
        ui->cb_XuoPath->setCurrentIndex(ui->cb_XuoPath->count() - 1);
    }
}

QString LauncherWindow::decodeArgumentString(const char *text, const int &length) const
{
    QString result;
    for (int i = 0; i < length; i += 2)
    {
        char buf[3] = { text[i], text[i + 1], 0 };
        result += char(QString(buf).toInt(nullptr, 16));
    }
    return result;
}

QString LauncherWindow::encodeArgumentString(const char *text, const int &length) const
{
    QString result;
    for (int i = 0; i < length; i++)
    {
        QString buf;
        buf.sprintf("%02X", text[i]);
        result += buf;
    }
    return result;
}

void LauncherWindow::runProgram(const QString &exePath, const QStringList &args, const QString &directory)
{
    QProcess process;
    QDir::setCurrent(directory);
    const auto success = process.startDetached(exePath, args, directory);
    if (!success)
    {
        QString err(QString("Could not launch application:\n" + exePath + "\nReason:\n"));
        QMessageBox::critical(this, "Error", err + process.errorString());
    }
}

void LauncherWindow::on_pb_Launch_clicked()
{
    writeCfg();

    auto clientPath = ui->cb_XuoPath->currentText();
    if (!ui->lw_ProfileList->count())
    {
        QMessageBox::critical(this, "Error", tr("Configuration not found."));
        ui->tw_Server->setCurrentIndex(1);
        return;
    }

    auto serverItem = static_cast<CServerListItem *>(ui->lw_ProfileList->currentItem());
    if (serverItem == nullptr)
    {
        QMessageBox::critical(this, tr("Error"), tr("Please select a configuration profile!"));
        return;
    }

    if (serverItem->GetClientVersion().trimmed().isEmpty())
    {
        QMessageBox::critical(this, tr("Error"), tr("Please set a Client Version!"));
        ui->tw_Server->setCurrentIndex(1);
        return;
    }

#if BUILD_WINDOWS
    const auto cwd = "\"" + clientPath + "\\crossuo" EXE_EXTENSION + "\"";
#else
    auto assist = QFileInfo(QDir(clientPath).filePath("xuoassist"));
    QString cwd;
    if (assist.exists())
    {
        cwd = "./xuoassist";
    }
    else
    {
        cwd = "./crossuo";
    }
#endif

    const auto program = cwd;
    QStringList args;
    if (!ui->le_CommandLine->text().trimmed().isEmpty())
        args.push_back(ui->le_CommandLine->text());

    if (ui->cb_LaunchFastLogin->isChecked())
        args.push_back("--fastlogin");

    if (ui->cb_LaunchAutologin->isChecked())
        args.push_back("--autologin");
    else
        args.push_back("--autologin=0");

    if (ui->cb_LaunchSavePassword->isChecked())
        args.push_back("--savepassword");
    else
        args.push_back("--savepassword=0");

#if BUILD_WINDOWS
    if (ui->cb_LaunchSaveAero->isChecked())
        args.push_back("--aero");
#endif

    args.push_back("--login=\"" + serverItem->GetAddress() + "\"");

    QString account = serverItem->GetAccount();
    QString password = serverItem->GetPassword();
    args.push_back(
        "--account=" + encodeArgumentString(account.toStdString().c_str(), account.length()) + "," +
        encodeArgumentString(password.toStdString().c_str(), password.length()));

    QString character = serverItem->GetCharacter();
    if (character.length())
        args.push_back("--autologinname=" +
                   encodeArgumentString(character.toStdString().c_str(), character.length()));

    if (serverItem->GetUseProxy())
    {
        QString proxyName = serverItem->GetProxy().toLower();
        for (int i = 0; i < ui->lw_ProxyList->count(); i++)
        {
            auto proxy = static_cast<CProxyListItem *>(ui->lw_ProxyList->item(i));
            if (proxy != nullptr && proxy->text().toLower() == proxyName)
            {
                args.push_back("-proxyhost=\"" + proxy->GetAddress() + "," + proxy->GetProxyPort() + "\"");
                if (proxy->GetSocks5())
                {
                    QString proxyAccount = proxy->GetAccount();
                    QString proxyPassword = proxy->GetPassword();
                    args.push_back("--proxyaccount=\"" +
                               encodeArgumentString(
                                   proxyAccount.toStdString().c_str(), proxyAccount.length()) +
                               "," +
                               encodeArgumentString(
                                   proxyPassword.toStdString().c_str(), proxyPassword.length()) + "\"");
                }
                break;
            }
        }
    }

    runProgram(program, args, clientPath);

#if BUILD_WINDOWS
    if (ui->cb_LaunchRunUOAM->isChecked())
    {
        clientPath += "/map";
        runProgram(clientPath + "/enhancedmap" EXE_EXTENSION, QStringList(), clientPath);
    }
#endif

    if (ui->cb_LaunchCloseAfterLaunch->isChecked())
    {
        saveServerList();
        saveProxyList();
        qApp->exit(0);
    }
}

void LauncherWindow::on_cb_LaunchAutologin_clicked()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ProfileList->currentItem());
    if (item != nullptr)
        item->SetOptionAutologin(ui->cb_LaunchAutologin->isChecked());
}

void LauncherWindow::on_cb_LaunchSavePassword_clicked()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ProfileList->currentItem());
    if (item != nullptr)
        item->SetOptionSavePassword(ui->cb_LaunchSavePassword->isChecked());
}

void LauncherWindow::on_cb_LaunchSaveAero_clicked()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ProfileList->currentItem());
    if (item != nullptr)
        item->SetOptionSaveAero(ui->cb_LaunchSaveAero->isChecked());
}

void LauncherWindow::on_cb_LaunchFastLogin_clicked()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ProfileList->currentItem());
    if (item != nullptr)
        item->SetOptionFastLogin(ui->cb_LaunchFastLogin->isChecked());
}

void LauncherWindow::on_cb_LaunchRunUOAM_clicked()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ProfileList->currentItem());
    if (item != nullptr)
        item->SetOptionRunUOAM(ui->cb_LaunchRunUOAM->isChecked());
}

void LauncherWindow::on_cb_Beta_clicked()
{
    on_pb_CheckUpdates_clicked();
}

void LauncherWindow::on_cb_XuoPath_currentIndexChanged(int index)
{
    Q_UNUSED(index);
    if (!m_Loading)
    {
        if (ui->cb_CheckUpdates->isChecked())
            on_pb_CheckUpdates_clicked();
    }
}

void LauncherWindow::onUpdatesListReceived(const QList<CFileInfo> &list)
{
    ui->lw_AvailableUpdates->clear();
    const auto clientPath = ui->cb_XuoPath->currentText();

    ui->pb_UpdateProgress->setValue(0);
    int n = 0;
    for (const auto &info : list)
    {
        auto fullPath = clientPath + "/" + info.Name;
        auto hash = m_UpdateManager->getHash(fullPath);

        if (info.inLauncher)
        {
            fullPath = qApp->applicationDirPath() + "/" + info.Name;
            hash = m_UpdateManager->getHash(fullPath);
        }

        //const bool wantUpdate = (info.Hash.length() && !hash.isEmpty() && info.Hash != hash);
        const bool wantUpdate = info.Hash.length() && info.Hash != hash; // even if we don't have the file locally
        if (wantUpdate)
            ui->lw_AvailableUpdates->addItem(new CUpdateInfoListWidgetItem(info));
        n++;
        ui->pb_UpdateProgress->setValue(100 * n / list.size());
    }

    if (ui->lw_AvailableUpdates->count())
        ui->tw_Main->setCurrentIndex(1);

    ui->pb_CheckUpdates->setEnabled(true);
    ui->pb_ApplyUpdates->setEnabled(true);
    ui->lw_Packages->setEnabled(true);
    ui->pb_RestoreSelectedVersion->setEnabled(true);
    ui->pb_ShowChangelog->setEnabled(true);
    ui->pb_UpdateProgress->setValue(100);
}

void LauncherWindow::onPackageListReceived(const QMap<QString, QMap<QString, CReleaseInfo>> &packages)
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

void LauncherWindow::on_pb_CheckUpdates_clicked()
{
    if (!ui->pb_CheckUpdates->isEnabled())
        return;

    ui->pb_CheckUpdates->setEnabled(false);
    ui->pb_ApplyUpdates->setEnabled(false);
    ui->lw_Packages->setEnabled(false);
    ui->pb_RestoreSelectedVersion->setEnabled(false);
    ui->pb_ShowChangelog->setEnabled(false);
    ui->pb_UpdateProgress->setValue(0);
    ui->lw_AvailableUpdates->clear();
    ui->lw_Packages->clear();
    auto beta = ui->cb_Beta->isChecked() ? "-beta" : "";
    m_UpdateManager->getManifest(QString("release/%1%2.manifest.xml").arg(GetPlatformName()).arg(beta));
}

void LauncherWindow::onFileReceived(const QString &name)
{
    Q_UNUSED(name);

    m_FilesToUpdateCount--;

    if (m_DownloadingPackageTotal > 0)
    {
        ui->pb_UpdateProgress->setValue(
            ((m_DownloadingPackageTotal - m_FilesToUpdateCount) * 100) /
            m_DownloadingPackageTotal);

        if (m_FilesToUpdateCount > 0)
            return;
    }

    if (m_FilesToUpdateCount <= 0 || ui->lw_AvailableUpdates->count() <= 0)
    {
        ui->pb_CheckUpdates->setEnabled(true);
        ui->pb_ApplyUpdates->setEnabled(true);
        ui->lw_Packages->setEnabled(true);
        ui->pb_RestoreSelectedVersion->setEnabled(true);
        ui->pb_ShowChangelog->setEnabled(true);
        ui->pb_UpdateProgress->setValue(100);
        m_FilesToUpdateCount = 0;
#if BUILD_WINDOWS
        if (m_LauncherFoundInUpdates)
        {
            saveServerList();
            saveProxyList();

            if (QMessageBox::question(
                    this,
                    "Updates notification",
                    "An update for the launcher is available, do you want to update it now?") != QMessageBox::Yes)
                return;
            qApp->exit(0);
        }
        else
#endif
            on_pb_CheckUpdates_clicked();
    }
    else
    {
        ui->pb_UpdateProgress->setValue(
            ((ui->lw_AvailableUpdates->count() - m_FilesToUpdateCount) * 100) /
            ui->lw_AvailableUpdates->count());
    }
}

void LauncherWindow::on_pb_ApplyUpdates_clicked()
{
    if (!ui->pb_CheckUpdates->isEnabled())
        return;

    if (ui->lw_AvailableUpdates->count() < 1)
    {
        ui->pb_UpdateProgress->setValue(100);
        return;
    }

    ui->pb_UpdateProgress->setValue(0);

    if (QMessageBox::question(
            this,
            tr("Updates notification"),
            tr("Close all CrossUO client instances and press 'Yes'.\nPress 'No' for cancel."),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
      return;

    ui->pb_CheckUpdates->setEnabled(false);
    ui->pb_ApplyUpdates->setEnabled(false);
    ui->lw_Packages->setEnabled(false);
    ui->pb_RestoreSelectedVersion->setEnabled(false);
    ui->pb_ShowChangelog->setEnabled(false);
    m_LauncherFoundInUpdates = false;
    m_FilesToUpdateCount = 0;

    QList<CUpdateInfoListWidgetItem *> updateList;
    for (int i = 0; i < ui->lw_AvailableUpdates->count(); i++)
    {
        auto item = static_cast<CUpdateInfoListWidgetItem *>(ui->lw_AvailableUpdates->item(i));
        if (item == nullptr)
            continue;

        m_FilesToUpdateCount++;
        updateList.push_back(item);
    }

    if (m_FilesToUpdateCount)
    {
        for (auto item : updateList)
        {
            auto dst = ui->cb_XuoPath->currentText();
            if (item->m_Info.inLauncher)
            {
                m_LauncherFoundInUpdates = true;
                dst = qApp->applicationDirPath();
            }
            ui->pb_UpdateProgress->setValue(0);
            const auto src = item->m_Info.ZipFileName;
            auto cb = [this, dst](const QString &f) {
                unzipPackage(f, dst);
                onFileReceived(f);
            };
            const bool silent = true;
            m_UpdateManager->getFile("update", src, item->m_Info, cb, this, silent);
        }
    }
    else
    {
        ui->pb_CheckUpdates->setEnabled(true);
        ui->pb_ApplyUpdates->setEnabled(true);
        ui->lw_Packages->setEnabled(true);
        ui->pb_RestoreSelectedVersion->setEnabled(true);
        ui->pb_ShowChangelog->setEnabled(true);
        ui->pb_UpdateProgress->setValue(100);
    }
}

void LauncherWindow::on_pb_RestoreSelectedVersion_clicked()
{
    const auto item = static_cast<CPackageInfoListWidgetItem *>(ui->lw_Packages->currentItem());
    if (item == nullptr)
        return;

    m_FilesToUpdateCount = item->m_Package.FileList.count();
    m_DownloadingPackageTotal = m_FilesToUpdateCount;
    m_LauncherFoundInUpdates = item->m_Package.Name == "X:UO Launcher";
    for (auto file : item->m_Package.FileList)
    {
        ui->pb_UpdateProgress->setValue(0);
        auto dst = ui->cb_XuoPath->currentText();
        if (file.inLauncher)
        {
            dst = qApp->applicationDirPath();
        }
        const auto src = file.ZipFileName;
        auto cb = [this, dst](const QString &f) {
            unzipPackage(f, dst);
            onFileReceived(f);
        };
        m_UpdateManager->getFile("update", src, file, cb, this);
    }
    m_DownloadingPackageTotal = 0;
}

void LauncherWindow::unzipPackage(const QString &filename, const QString &toPath)
{
    QZipReader zipReader{ filename };
    QDir dir{ toPath };
    const auto path = dir.canonicalPath();
    for (auto it : zipReader.fileInfoList())
    {
        auto target = QFileInfo(path + "/" + it.filePath).absolutePath();
        QDir(path).mkpath(target);
    }
    if (!zipReader.extractAll(path))
    {
        qDebug() << "Failed to unpack file:" << filename;
    }
#if !BUILD_WINDOWS
    for (auto it : zipReader.fileInfoList())
    {
        // FIXME: set executable only what was executable before packaging
        QFile f(path + "/" + it.filePath);
        auto p = f.permissions();
        f.setPermissions(p | QFileDevice::ExeOwner);
    }
#endif
    zipReader.close();
}

void LauncherWindow::onDownloadProgress(qint64 bytesRead, qint64 totalBytes)
{
    ui->pb_UpdateProgress->setValue(100 * int(bytesRead / float(totalBytes)));
}

void LauncherWindow::on_pb_ShowChangelog_clicked()
{
    assert(m_ChangelogForm != nullptr);

    if (m_ChangelogForm->isVisible())
        m_ChangelogForm->activateWindow();
    else
        m_ChangelogForm->show();

    emit m_ChangelogForm->changelogReceived("Loading...");

    m_UpdateManager->getChangelog("release/changelog.html");
}

void LauncherWindow::on_lw_Packages_doubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    on_pb_RestoreSelectedVersion_clicked();
}

void LauncherWindow::on_pb_Github_clicked()
{
    QDesktopServices::openUrl(QUrl("https://github.com/crossuo"));
}

void LauncherWindow::on_pb_TeamMember_clicked()
{
    QDesktopServices::openUrl(QUrl("https://github.com/orgs/crossuo/teams/crossuo-team/members"));
}

void LauncherWindow::on_pb_Website_clicked()
{
     QDesktopServices::openUrl(QUrl("http://crossuo.com"));
}

void LauncherWindow::on_pb_Discord_clicked()
{
    QDesktopServices::openUrl(QUrl("https://discord.gg/FVy46dx"));
}
