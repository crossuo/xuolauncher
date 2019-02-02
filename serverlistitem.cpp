/***********************************************************************************
**
** ServerListItem.cpp
**
** Copyright (C) December 2016 Hotride
** Copyright (C) December 2018 Danny Angelo Carminati Grein
**
************************************************************************************
*/
#include "serverlistitem.h"

CServerListItem::CServerListItem(const QString &name)
    : QListWidgetItem(name)
{
}

CServerListItem::CServerListItem(const QString &name,
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
    const bool &serverUseCrypt)
    : QListWidgetItem(name)
    , m_Address(address)
    , m_Account(account)
    , m_Password(password)
    , m_Character(character)
    , m_ClientVersion(clientVersion)
    , m_ClientPath(clientPath)
    , m_CrossUoPath(crossUoPath)
    , m_ClientType(clientType)
    , m_Proxy(serverProxy)
    , m_Command("")
    , m_UseProxy(serveUseProxy)
    , m_UseCrypt(serverUseCrypt)
{
}
