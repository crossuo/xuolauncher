/***********************************************************************************
**
** ServerListItem.h
**
** Copyright (C) October 2016 Hotride
** Copyright (C) December 2018 Danny Angelo Carminati Grein
**
************************************************************************************
*/
#pragma once

#include <QListWidgetItem>

class CServerListItem : public QListWidgetItem
{
private:
    QString m_Address;
    QString m_Account;
    QString m_Password;
    QString m_Character;
    QString m_ClientVersion;
    QString m_ClientPath;
    QString m_CrossUoPath;
    QString m_ClientType;
    QString m_Proxy;
    QString m_Command;
    bool m_UseProxy{ false };
    bool m_UseCrypt{ false };
    bool m_OptionAutologin{ false };
    bool m_OptionSavePassword{ false };
    bool m_OptionSaveAero{ false };
    bool m_OptionFastLogin{ false };
    bool m_OptionRunUOAM{ false };

public:
    CServerListItem(const QString &name);
    CServerListItem(
        const QString &name,
        const QString &address,
        const QString &account,
        const QString &password,
        const QString &character,
        const QString &clientVersion,
        const QString &clientPath,
        const QString &crossUoPath,
        const int &clientType,
        const QString &serverProxy,
        const bool &serveUseProxy,
        const bool &serverUseCrypt);
    virtual ~CServerListItem() {}

    QString GetAddress() const { return m_Address; }
    void SetAddress(const QString &address) { m_Address = address; }

    QString GetAccount() const { return m_Account; }
    void SetAccount(const QString &account) { m_Account = account; }

    QString GetPassword() const { return m_Password; }
    void SetPassword(const QString &password) { m_Password = password; }

    QString GetCharacter() const { return m_Character; }
    void SetCharacter(const QString &character) { m_Character = character; }

    QString GetClientVersion() const { return m_ClientVersion; }
    void SetClientVersion(const QString &clientVersion) { m_ClientVersion = clientVersion; }

    QString GetClientPath() const { return m_ClientPath; }
    void SetClientPath(const QString &clientPath) { m_ClientPath = clientPath; }

    QString GetCrossUoPath() const { return m_CrossUoPath; }
    void SetCrossUoPath(const QString &crossUoPath) { m_CrossUoPath = crossUoPath; }

    QString GetClientTypeString() const { return m_ClientType; }
    void SetClientTypeFromString(const QString &clientType) { m_ClientType = clientType.toLower(); }
    int GetClientType()
    {
        if (m_ClientType == "t2a")
            return 1;
        if (m_ClientType == "re")
            return 2;
        if (m_ClientType == "td")
            return 3;
        if (m_ClientType == "lbr")
            return 4;
        if (m_ClientType == "aos")
            return 5;
        if (m_ClientType == "se")
            return 6;
        if (m_ClientType == "sa")
            return 7;
        return 0;
    }
    void SetClientType(int index)
    {
        switch (index)
        {
            case 1: m_ClientType = "t2a"; break;
            case 2: m_ClientType = "re"; break;
            case 3: m_ClientType = "td"; break;
            case 4: m_ClientType = "lbr"; break;
            case 5: m_ClientType = "aos"; break;
            case 6: m_ClientType = "se"; break;
            case 7: m_ClientType = "sa"; break;
            default:
                break;
        }
    }

    QString GetCommand() const { return m_Command; }
    void SetCommand(const QString &command) { m_Command = command; }

    QString GetProxy() const { return m_Proxy; }
    void SetProxy(const QString &proxy) { m_Proxy = proxy; }

    bool GetUseProxy() const { return m_UseProxy; }
    void SetUseProxy(const bool &useProxy) { m_UseProxy = useProxy; }

    bool GetUseCrypt() const { return m_UseCrypt; }
    void SetUseCrypt(const bool &useCrypt) { m_UseCrypt = useCrypt; }

    bool GetOptionAutologin() const { return m_OptionAutologin; }
    void SetOptionAutologin(const bool &autologin) { m_OptionAutologin = autologin; }

    bool GetOptionSavePassword() const { return m_OptionSavePassword; }
    void SetOptionSavePassword(const bool &savePassword) { m_OptionSavePassword = savePassword; }

    bool GetOptionSaveAero() const { return m_OptionSaveAero; }
    void SetOptionSaveAero(const bool &saveAero) { m_OptionSaveAero = saveAero; }

    bool GetOptionFastLogin() const { return m_OptionFastLogin; }
    void SetOptionFastLogin(const bool &fastLogin) { m_OptionFastLogin = fastLogin; }

    bool GetOptionRunUOAM() const { return m_OptionRunUOAM; }
    void SetOptionRunUOAM(const bool &runUOAM) { m_OptionRunUOAM = runUOAM; }
};
