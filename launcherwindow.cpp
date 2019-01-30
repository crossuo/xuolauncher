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

    m_ChangelogForm = new ChangelogForm(nullptr);

    m_UpdateManager = new UpdateManager(UPDATER_HOST, LAUNCHER_TITLE, this);
    m_UpdateManager->setCacheDirectory(qApp->applicationDirPath() + "/cache");
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
    connect(this, SIGNAL(fileReceived(const QDir &)), this, SLOT(onFileReceived(const QDir &)));
    connect(&m_UpdatesTimer, SIGNAL(timeout()), this, SLOT(onUpdatesTimer()));

    setFixedSize(size());

    loadProxyList();
    loadServerList();

#if defined(QT_NO_DEBUG)
    ui->tw_Main->removeTab(5);
#else

#endif

#if !BUILD_WINDOWS
    ui->cb_LaunchRunUOAM->setEnabled(false);
    ui->cb_LaunchRunUOAM->setVisible(false);
    ui->cb_LaunchSaveAero->setEnabled(false);
    ui->cb_LaunchSaveAero->setVisible(false);
#endif

    ui->tw_Main->setCurrentIndex(0);
    ui->tw_Server->setCurrentIndex(0);

    updateXUOAFeaturesCode();
    updateXuoFeaturesCode();

    setWindowTitle(LAUNCHER_TITLE);
    m_Loading = false;

    if (!ui->cb_XuoPath->currentText().length())
        on_tb_SetXuoPath_clicked();

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

        if (focused == ui->lw_ServerList) // Servers list
            on_pb_ServerRemove_clicked();
        else if (focused == ui->lw_ProxyList) // Proxy list
            on_pb_ProxyRemove_clicked();
    }

    event->accept();
}

void LauncherWindow::on_lw_ServerList_clicked(const QModelIndex &index)
{
    updateServerFields(index.row());
}

void LauncherWindow::updateServerFields(const int &index)
{
    auto item = static_cast<CServerListItem *>(ui->lw_ServerList->item(index));
    if (item != nullptr)
    {
        ui->le_ServerName->setText(item->text());
        ui->le_ServerAddress->setText(item->GetAddress());
        ui->le_ServerAccount->setText(item->GetAccount());
        ui->le_ServerPassword->setText(item->GetPassword());
        ui->le_ServerCharacter->setText(item->GetCharacter());
        ui->le_CommandLine->setText(item->GetCommand());

        ui->le_ServerClientVersion->setText(item->GetClientVersion());
        ui->le_ServerClientPath->setText(item->GetClientPath());
        ui->cb_ServerClientType->setCurrentIndex(item->GetClientType());
        ui->cb_ServerUseCrypt->setChecked(item->GetUseCrypt());

        ui->cb_LaunchAutologin->setChecked(item->GetOptionAutologin());
        ui->cb_LaunchSavePassword->setChecked(item->GetOptionSavePassword());
        ui->cb_LaunchSaveAero->setChecked(item->GetOptionSaveAero());
        ui->cb_LaunchFastLogin->setChecked(item->GetOptionFastLogin());
        ui->cb_LaunchRunUOAM->setChecked(item->GetOptionRunUOAM());

        ui->cb_ServerUseProxy->setChecked(item->GetUseProxy());
        ui->cb_ServerProxy->setCurrentText(item->GetProxy());
    }
}

void LauncherWindow::on_lw_ServerList_doubleClicked(const QModelIndex &index)
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

void LauncherWindow::on_pb_ServerAdd_clicked()
{
    QString name = ui->le_ServerName->text().toLower();

    if (!name.length())
    {
        QMessageBox::critical(this, "Name is empty", "Enter the server name!");
        return;
    }

    for (int i = 0; i < ui->lw_ServerList->count(); i++)
    {
        auto item = ui->lw_ServerList->item(i);
        if (item != nullptr && item->text().toLower() == name)
        {
            QMessageBox::critical(this, "Name is already exists", "Server name is already exists!");
            return;
        }
    }

    auto item = new CServerListItem(
        ui->le_ServerName->text(),
        ui->le_ServerAddress->text(),
        ui->le_ServerAccount->text(),
        ui->le_ServerPassword->text(),
        ui->le_ServerCharacter->text(),
        ui->le_ServerClientVersion->text(),
        ui->le_ServerClientPath->text(),
        ui->cb_ServerClientType->currentIndex(),
        ui->cb_ServerProxy->currentText(),
        ui->cb_ServerUseProxy->isChecked(),
        ui->cb_ServerUseCrypt->isChecked());
    item->SetUseProxy(ui->cb_ServerUseProxy->isChecked());
    item->SetProxy(ui->cb_ServerProxy->currentText());

    ui->lw_ServerList->addItem(item);
    ui->lw_ServerList->setCurrentRow(ui->lw_ServerList->count() - 1);
    saveServerList();

    updateServerFields(ui->lw_ServerList->count() - 1);
}

void LauncherWindow::on_pb_ServerSave_clicked()
{
    QString name = ui->le_ServerName->text().toLower();

    if (!name.length())
    {
        QMessageBox::critical(this, "Name is empty", "Enter the server name!");
        return;
    }

    auto selected = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());

    if (selected == nullptr)
    {
        QMessageBox::critical(this, "No selected item", "No selected server!");
        return;
    }

    for (int i = 0; i < ui->lw_ServerList->count(); i++)
    {
        auto item = ui->lw_ServerList->item(i);
        if (item != nullptr && item->text().toLower() == name)
        {
            if (item != nullptr && item != selected)
            {
                QMessageBox::critical(
                    this,
                    "Name is already exists",
                    "Server name is already exists (not this item)!");
                return;
            }

            break;
        }
    }

    selected->setText(ui->le_ServerName->text());
    selected->SetAddress(ui->le_ServerAddress->text());
    selected->SetAccount(ui->le_ServerAccount->text());
    selected->SetCharacter(ui->le_ServerCharacter->text());
    selected->SetPassword(ui->le_ServerPassword->text());
    selected->SetClientVersion(ui->le_ServerClientVersion->text());
    selected->SetClientPath(ui->le_ServerClientPath->text());
    selected->SetClientType(ui->cb_ServerClientType->currentIndex());
    selected->SetProxy(ui->cb_ServerProxy->currentText());
    selected->SetUseProxy(ui->cb_ServerUseProxy->isChecked());
    selected->SetUseCrypt(ui->cb_ServerUseCrypt->isChecked());

    saveServerList();
}

void LauncherWindow::on_pb_ServerRemove_clicked()
{
    QListWidgetItem *item = ui->lw_ServerList->currentItem();

    if (item != nullptr)
    {
        item = ui->lw_ServerList->takeItem(ui->lw_ServerList->row(item));

        if (item != nullptr)
        {
            delete item;

            saveServerList();
        }
    }
}

void LauncherWindow::on_le_CommandLine_textChanged(const QString &arg1)
{
    auto selected = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());

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
        QMessageBox::critical(this, "Name is empty", "Enter the proxy server name!");
        return;
    }

    for (int i = 0; i < ui->lw_ProxyList->count(); i++)
    {
        auto item = ui->lw_ProxyList->item(i);
        if (item != nullptr && item->text().toLower() == name)
        {
            QMessageBox::critical(
                this, "Name is already exists", "Proxy server name is already exists!");
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
        QMessageBox::critical(this, "Name is empty", "Enter the proxy server name!");
        return;
    }

    auto selected = static_cast<CProxyListItem *>(ui->lw_ProxyList->currentItem());
    if (selected == nullptr)
    {
        QMessageBox::critical(this, "No selected item", "No selected proxy!");
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
                    "Name is already exists",
                    "Proxy server name is already exists (not this item)!");
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

QString LauncherWindow::boolToText(const bool &value)
{
    return value ? "true" : "false";
}

bool LauncherWindow::rawStringToBool(QString value)
{
    value = value.toLower();
    bool result = false;

    if (value == "true" || value == "on")
        result = true;
    else
        result = (value.toInt() != 0);

    return result;
}

void LauncherWindow::writeCfg()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());
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

        int count = ui->lw_ServerList->count();
        writer.writeStartElement("serverlist");
        writer.writeAttribute("version", "0");
        writer.writeAttribute("size", QString::number(count));
        writer.writeAttribute(
            "closeafterlaunch", boolToText(ui->cb_LaunchCloseAfterLaunch->isChecked()));
        writer.writeAttribute("lastserver", QString::number(ui->lw_ServerList->currentRow()));
        writer.writeAttribute("checkupdates", boolToText(ui->cb_CheckUpdates->isChecked()));
        writer.writeAttribute(
            "noclientwarnings", boolToText(ui->cb_NoClientWarnings->isChecked()));
        writer.writeAttribute("beta", boolToText(ui->cb_Beta->isChecked()));

        for (int i = 0; i < ui->cb_XuoPath->count(); i++)
        {
            writer.writeStartElement("clientpath");
            writer.writeAttribute("path", ui->cb_XuoPath->itemText(i));
            writer.writeEndElement(); // clientpath
        }

        for (int i = 0; i < count; i++)
        {
            auto item = static_cast<CServerListItem *>(ui->lw_ServerList->item(i));
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
    ui->lw_ServerList->clear();

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

                    if (attributes.hasAttribute("noclientwarnings"))
                        ui->cb_NoClientWarnings->setChecked(
                            rawStringToBool(attributes.value("noclientwarnings").toString()));

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

                        ui->lw_ServerList->addItem(item);
                    }
                }
            }
            reader.readNext();
        }

        if (clientindex >= 0 && clientindex < ui->cb_XuoPath->count())
            ui->cb_XuoPath->setCurrentIndex(clientindex);

        if (lastServer >= 0 && lastServer < ui->lw_ServerList->count())
        {
            ui->lw_ServerList->setCurrentRow(lastServer);
            updateServerFields(lastServer);
        }
        file.close();
    }
}

void LauncherWindow::on_tb_SetClientPath_clicked()
{
    auto clientPath = QCoreApplication::applicationDirPath();

    auto r = QMessageBox::Yes;
    auto path = clientPath;
    do
    {
        path = QFileDialog::getExistingDirectory(nullptr, tr("Select UO Client Directory"), clientPath);
        if (path.isEmpty())
            return;
        const QDir p(path);
        const auto clientExe = QFileInfo(p.filePath("client.exe"));
        const auto loginCfg1 = QFileInfo(p.filePath("Login.cfg"));
        const auto loginCfg2 = QFileInfo(p.filePath("login.cfg"));
        const auto outlands = QFileInfo(p.filePath("OutlandsUO.exe"));
        const bool isValid = clientExe.exists() && (loginCfg1.exists() || loginCfg2.exists());
        if (outlands.exists())
        {
            auto q = QMessageBox::question(this, "IS this Outlands?", tr("Is this a Outlands Client Installation?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (q == QMessageBox::Yes)
            {
                ui->le_ServerClientVersion->setText("7.0.15.1");
                ui->cb_ServerClientType->setCurrentIndex(5); // FIXME: find the correct by name
                ui->le_ServerAddress->setText("play.uooutlands.com,2593");
            }
        }
        if (!isValid)
        {
            r = QMessageBox::warning(this, "WARNING", tr("Couldn't find 'client.exe' or 'login.cfg'!\nAre you sure you want to use this path?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        }
    } while (r != QMessageBox::Yes);


    if (path.length())
    {
        ui->le_ServerClientPath->setText(path);
    }
}

void LauncherWindow::on_tb_SetReleasePath_clicked()
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

void LauncherWindow::on_pb_Process_clicked()
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
            return;
        const QDir p(path);
        const auto clientExe = QFileInfo(p.filePath("client.exe"));
        const auto loginCfg1 = QFileInfo(p.filePath("Login.cfg"));
        const auto loginCfg2 = QFileInfo(p.filePath("login.cfg"));
        const bool isClientPath = clientExe.exists() && (loginCfg1.exists() || loginCfg2.exists());
        if (isClientPath)
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

QString LauncherWindow::decodeArgumentString(const char *text, const int &length)
{
    QString result;
    for (int i = 0; i < length; i += 2)
    {
        char buf[3] = { text[i], text[i + 1], 0 };
        result += char(QString(buf).toInt(nullptr, 16));
    }
    return result;
}

QString LauncherWindow::encodeArgumentString(const char *text, const int &length)
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

void LauncherWindow::on_pb_GenerateConfig_clicked()
{
    writeCfg();
}

void LauncherWindow::on_pb_Launch_clicked()
{
    writeCfg();

    auto clientPath = ui->cb_XuoPath->currentText();
    if (!ui->lw_ServerList->count())
    {
        QMessageBox::critical(this, "Error", tr("Configuration not found."));
        ui->tw_Server->setCurrentIndex(1);
        return;
    }

    auto serverItem = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());
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
    const auto cwd = "\"" + ui->cb_XuoPath->currentText() + "\\crossuo" EXE_EXTENSION + "\"";
#else
    const auto cwd = QString("./crossuo");
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

    args.push_back("\"--login=" + serverItem->GetAddress() + "\"");

    QString account = serverItem->GetAccount();
    QString password = serverItem->GetPassword();
    args.push_back(
        "--account=" + encodeArgumentString(account.toStdString().c_str(), account.length()) + "," +
        encodeArgumentString(password.toStdString().c_str(), password.length()));

    QString character = serverItem->GetCharacter();
    if (character.length())
        args.push_back("--autologinname=" +
                   encodeArgumentString(character.toStdString().c_str(), character.length()));

    if (ui->cb_NoClientWarnings->isChecked())
        args.push_back("--nowarnings");

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
    auto item = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());
    if (item != nullptr)
        item->SetOptionAutologin(ui->cb_LaunchAutologin->isChecked());
}

void LauncherWindow::on_cb_LaunchSavePassword_clicked()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());
    if (item != nullptr)
        item->SetOptionSavePassword(ui->cb_LaunchSavePassword->isChecked());
}

void LauncherWindow::on_cb_LaunchSaveAero_clicked()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());
    if (item != nullptr)
        item->SetOptionSaveAero(ui->cb_LaunchSaveAero->isChecked());
}

void LauncherWindow::on_cb_LaunchFastLogin_clicked()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());
    if (item != nullptr)
        item->SetOptionFastLogin(ui->cb_LaunchFastLogin->isChecked());
}

void LauncherWindow::on_cb_LaunchRunUOAM_clicked()
{
    auto item = static_cast<CServerListItem *>(ui->lw_ServerList->currentItem());
    if (item != nullptr)
        item->SetOptionRunUOAM(ui->cb_LaunchRunUOAM->isChecked());
}

void LauncherWindow::on_cb_Beta_clicked()
{
    on_pb_CheckUpdates_clicked();
}

void LauncherWindow::on_lw_XUOAFeaturesOptions_clicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    updateXUOAFeaturesCode();
}

void LauncherWindow::on_lw_XUOAFeaturesScripts_clicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    updateXUOAFeaturesCode();
}

void LauncherWindow::on_rb_XUOAFeaturesSphere_clicked()
{
    updateXUOAFeaturesCode();
}

void LauncherWindow::on_rb_XUOAFeaturesRunUO_clicked()
{
    updateXUOAFeaturesCode();
}

void LauncherWindow::on_rb_XUOAFeaturesPOL_clicked()
{
    updateXUOAFeaturesCode();
}

void LauncherWindow::updateXUOAFeaturesCode()
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

        if (info.Name.toLower() == "launcher" EXE_EXTENSION || info.inLauncher)
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
            "Updates notification",
            "Close all XuoUO instances and press 'Yes'.\nPress "
            "'No' for cancel.") != QMessageBox::Yes)
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
            if (item->text().toLower() == "launcher" EXE_EXTENSION || item->m_Info.inLauncher)
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
    for (auto file : item->m_Package.FileList)
    {
        ui->pb_UpdateProgress->setValue(0);
        const auto src = file.ZipFileName;
        const auto dst = ui->cb_XuoPath->currentText();
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

void LauncherWindow::on_lw_XuoFeaturesOptions_clicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    updateXuoFeaturesCode();
}

void LauncherWindow::on_rb_XuoFeaturesSphere_clicked()
{
    updateXuoFeaturesCode();
}

void LauncherWindow::on_rb_XuoFeaturesRunUO_clicked()
{
    updateXuoFeaturesCode();
}

void LauncherWindow::on_rb_XuoFeaturesPOL_clicked()
{
    updateXuoFeaturesCode();
}

void LauncherWindow::updateXuoFeaturesCode()
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
