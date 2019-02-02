/**
** @file UpdateInfo.hpp
**
** @brief Класс с информацией о обновлении
**
** @author Мустакимов Т.Р.
**
** Copyright (C) December 2018 Danny Angelo Carminati Grein
**
**/
#pragma once

#include <QListWidgetItem>

class CFileInfo
{
public:
    CFileInfo() = default;
    QString Name;
    QString ZipFileName;
    QByteArray Hash;
    QByteArray ZipHash;
    bool inLauncher;
};

class CReleaseInfo
{
public:
    QString Name;
    QString Version;
    QString latest;
    QList<CFileInfo> FileList;
};

class CChangelogInfo
{
public:
    CChangelogInfo() = default;
    QString Name;
    QString Description;
};

class CUpdateInfoListWidgetItem : public QListWidgetItem
{
public:
    CUpdateInfoListWidgetItem(const CFileInfo &info)
        : QListWidgetItem()
        , m_Info(info)
    {
        setText(info.Name);
    }

    virtual ~CUpdateInfoListWidgetItem() {}

    CFileInfo m_Info;
};

class CPackageInfoListWidgetItem : public QListWidgetItem
{
public:
    CPackageInfoListWidgetItem(const CReleaseInfo &package)
        : QListWidgetItem()
        , m_Package(package)
    {
        setText(package.Name + " " + package.Version);
    }

    virtual ~CPackageInfoListWidgetItem() {}

    CReleaseInfo m_Package;
};
