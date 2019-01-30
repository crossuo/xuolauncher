/*
* UpdateManager.h
* Copyright (c) 2018 Danny Angelo Carminati Grein
* This file is released under the GPL license
*/
#pragma once

#include <QObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QList>
#include <QFile>
#include <QProgressDialog>
#include <functional>
#include "updateinfo.h"

class ProgressDialog : public QProgressDialog
{
    Q_OBJECT

public:
    explicit ProgressDialog(const QString &filename, QWidget *parent = nullptr);

public slots:
    void networkReplyProgress(qint64 bytesRead, qint64 totalBytes);
};

class UpdateManager : public QObject
{
    Q_OBJECT
    QNetworkAccessManager *manager = nullptr;
    bool httpRequestAborted = false;
    QString cachePath;
    QString host;
    QString userAgent;
    QMap<QString, QMap<QString, CReleaseInfo>> releaseList; // product,version,files

    template <typename F>
    void get(const QString &url, F func);

    QByteArray readReply(QNetworkReply *reply);
    void readManifest(const QString &xmlName);
    bool validateFile(const QString &filename, const QByteArray &hash) const;
    QMap<QString, QString> readCache(const QString &path) const;
    void writeCache(const QString &path, QMap<QString, QString> cache) const;

public:
    explicit UpdateManager(
        const QString &host, const QString &userAgent, QObject *parent = nullptr);
    virtual ~UpdateManager();

    void setCacheDirectory(const QString &str);
    QByteArray getHash(const QString &fileName) const;
    void writeManifest(const QString &filename);
    void generateUpdate(const QString &path, const QString &plat, const QString &product, const QString &version, QWidget *parent);

    void getManifest(const QString &url);
    void getChangelog(const QString &url);
    void getFile(const QString &filename,
        const QString &url, const CFileInfo &info,
        std::function<void(const QString &)> finishedCb,
        QWidget *parent = nullptr,
        bool silent = false);

signals:
    void packageListReceived(QMap<QString, QMap<QString, CReleaseInfo>> &);
    void updatesListReceived(QList<CFileInfo> &);
    void changelogReceived(const QString &);
    void downloadProgress(qint64, qint64);

private slots:
    void streamReadyRead(QNetworkReply *, QFile *);
};
