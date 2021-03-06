/***********************************************************************************
**
** ProxyListItem.cpp
**
** Copyright (C) December 2016 Hotride
** Copyright (C) December 2018 Danny Angelo Carminati Grein
**
************************************************************************************
*/
#include "proxylistitem.h"

CProxyListItem::CProxyListItem(const QString &name)
    : QListWidgetItem(name)
{
}

CProxyListItem::CProxyListItem(
    const QString &name,
    const QString &address,
    const QString &port,
    const bool &socks5,
    const QString &account,
    const QString &password)
    : QListWidgetItem(name)
    , m_Address(address)
    , m_ProxyPort(port)
    , m_Socks5(socks5)
    , m_Account(account)
    , m_Password(password)
{
}
