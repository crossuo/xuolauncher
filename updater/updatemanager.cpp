/*
* UpdateManager.cpp
* Copyright (c) 2018 Danny Angelo Carminati Grein
* This file is released under the GPL license
*/
#include "updatemanager.h"
#include "xxhash.h"
#include <assert.h>
#include <QProgressDialog>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDirIterator>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QTemporaryFile>
#include "qzipwriter_p.h"

ProgressDialog::ProgressDialog(const QString &filename, QWidget *parent)
    : QProgressDialog(parent)
{
    setWindowTitle(tr("Download Progress"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
    setCancelButton(nullptr);
    setLabelText(tr("Downloading to %1.").arg(filename));
    setMinimum(0);
    setValue(0);
    setMinimumDuration(0);
    setMinimumSize(QSize(400, 75));
}

void ProgressDialog::networkReplyProgress(qint64 bytesRead, qint64 totalBytes)
{
    setMaximum(int(totalBytes));
    setValue(int(bytesRead));
}

UpdateManager::UpdateManager(const QString &host, const QString &userAgent, QObject *parent)
    : QObject(parent)
    , host(host)
    , userAgent(userAgent)
{
    manager = new QNetworkAccessManager(this);
}

UpdateManager::~UpdateManager()
{
    delete manager;
}

void UpdateManager::setCacheDirectory(const QString &cachePath)
{
    QDir dir(cachePath);
    dir.mkpath(cachePath);
    this->cachePath = cachePath;
}

QByteArray UpdateManager::getHash(const QString &fileName) const
{
    QFile f(fileName);
    if (f.open(QFile::ReadOnly))
    {
        auto len = f.size();
        auto ptr = f.map(0, len);
        auto hash = XXH64(static_cast<void *>(ptr), static_cast<size_t>(len), 0x2593);
        QByteArray byteArray;
        QDataStream stream(&byteArray, QIODevice::WriteOnly);
        stream << hash;
        return byteArray;
    }
    return QByteArray();
}

void UpdateManager::getManifest(const QString &endpoint)
{
    get(endpoint, [this](QNetworkReply *reply) {
        const auto data = readReply(reply);
        readManifest(data);
        QList<CFileInfo> releaseFiles;

        for (const auto &p : releaseList.keys())
        {
            for (const auto &r : releaseList[p])
            {
                if (r.latest == "true")
                {
                    for (const auto &file : r.FileList)
                    {
                        releaseFiles.push_back(file);
                    }
                }
            }
        }

        emit packageListReceived(releaseList);
        emit updatesListReceived(releaseFiles);
    });
}

void UpdateManager::getChangelog(const QString &endpoint)
{
    get(endpoint, [this](QNetworkReply *reply) {
        const auto data = readReply(reply);
        emit changelogReceived(data);
    });
}

template <typename F>
void UpdateManager::get(const QString &endpoint, F func)
{
    QNetworkRequest request;
    request.setUrl(QUrl(host + endpoint));
    request.setRawHeader("User-Agent", userAgent.toUtf8());

    auto reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [=]() { func(reply); });
}

bool UpdateManager::validateFile(const QString &filename, const QByteArray &hash) const
{
    return getHash(filename) == hash;
}

void UpdateManager::getFile(
    const QString &endpoint,
    const QString &filename,
    const CFileInfo &info,
    std::function<void(const QString &)> finishedCb,
    QWidget *parent,
    bool silent)
{
    QString dir{ cachePath + "/" + filename };
    auto file = new QFile{ dir };
    if (file->exists() && validateFile(dir, info.ZipHash))
    {
        finishedCb(dir);
        return;
    }

    QDir(cachePath + "/").mkdir(QFileInfo(*file).absolutePath());

    file->open(QIODevice::WriteOnly);
    const auto url = QUrl(host + endpoint + "/" + filename);
    QNetworkRequest request;
    request.setUrl(url);
    request.setRawHeader("User-Agent", userAgent.toUtf8());

    auto reply = manager->get(request);
    if (!silent)
    {
        auto progressDialog = new ProgressDialog(filename, parent);
        progressDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(
            reply,
            &QNetworkReply::downloadProgress,
            progressDialog,
            &ProgressDialog::networkReplyProgress);
        connect(reply, &QNetworkReply::finished, progressDialog, &ProgressDialog::hide);
        progressDialog->show();
    }
    connect(
        reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesRead, qint64 totalBytes) {
            emit downloadProgress(bytesRead, totalBytes);
        });
    connect(
        reply, &QIODevice::readyRead, this, [file, reply, info] { file->write(reply->readAll()); });

    connect(reply, &QNetworkReply::finished, this, [=] {
        QFileInfo fi;
        QString filename;
        if (file)
        {
            filename = file->fileName();
            fi.setFile(filename);
            file->close();
            delete file;
        }

        if (reply->error())
        {
            auto err = reply->errorString();
            QFile::remove(fi.absoluteFilePath());
            reply->deleteLater();
            return;
        }

        if (validateFile(filename, info.Hash))
        {
            finishedCb(dir);
        }
    });
}

QByteArray UpdateManager::readReply(QNetworkReply *reply)
{
    if (httpRequestAborted || reply->error())
    {
        auto err = reply->errorString();
        reply->deleteLater();
        reply = nullptr;
        return {};
    }
    auto data = reply->readAll();
    reply->deleteLater();
    reply = nullptr;
    return data;
}

void UpdateManager::streamReadyRead(QNetworkReply *reply, QFile *file)
{
    if (file)
        file->write(reply->readAll());
}

void UpdateManager::readManifest(const QString &xmlData)
{
#define ReadAttribute(var, name)                                                                   \
    if (attributes.hasAttribute(name))                                                             \
    var = attributes.value(name).toString()

    releaseList.clear();

    bool launcher = false;
    QString tmp;
    QString version;
    QString prod;
    QXmlStreamReader reader(xmlData);
    while (!reader.atEnd() && !reader.hasError())
    {
        if (reader.isStartElement())
        {
            auto attributes = reader.attributes();
            if (reader.name().toString().trimmed().toLower() == "release")
            {
                CReleaseInfo release;
                ReadAttribute(release.Name, "name");
                ReadAttribute(release.Version, "version");
                ReadAttribute(release.latest, "latest");

                prod = release.Name;
                launcher = prod == "X:UO Launcher";
                version = release.Version;
                releaseList[prod][version] = release;
            }
            else if (reader.name().toString().trimmed().toLower() == "file")
            {
                if (version.isEmpty() || prod == "all")
                    return;

                CFileInfo info;
                info.inLauncher = launcher;

                ReadAttribute(info.Name, "name");
                info.Name = info.Name.replace("\\", "/");

                ReadAttribute(tmp, "hash");
                info.Hash = QByteArray::fromHex(tmp.toLatin1());

                ReadAttribute(info.ZipFileName, "data");

                ReadAttribute(tmp, "datahash");
                info.ZipHash = QByteArray::fromHex(tmp.toLatin1());

                releaseList[prod][version].FileList.push_back(info);
            }
        }
        reader.readNext();
    }
}

void UpdateManager::writeManifest(const QString &filename)
{
    QList<CFileInfo> updateList;

    QFile file{ filename };
    file.open(QIODevice::WriteOnly);
    QXmlStreamWriter stream(&file);

    stream.setAutoFormatting(true);
    stream.setAutoFormattingIndent(4);
    stream.writeStartDocument();
    stream.writeStartElement("manifest");

    for (const auto &file : updateList)
    {
        stream.writeStartElement("file");
        stream.writeAttribute("name", file.Name);
        stream.writeAttribute("hash", file.Hash.toHex());
        stream.writeAttribute("data", file.ZipFileName);
        stream.writeAttribute("datahash", file.ZipHash.toHex());
        stream.writeEndElement(); // file
    }

    for (const auto &prod : releaseList.keys())
    {
        for (const auto &rel : releaseList[prod])
        {
            stream.writeStartElement("release");
            stream.writeAttribute("name", rel.Name);
            stream.writeAttribute("version", rel.Version);
            stream.writeAttribute("latest",rel.latest);

            for (const auto &file : rel.FileList)
            {
                stream.writeStartElement("file");
                stream.writeAttribute("name", file.Name);
                stream.writeAttribute("hash", file.Hash.toHex());
                stream.writeAttribute("data", file.ZipFileName);
                stream.writeAttribute("datahash", file.ZipHash.toHex());
                stream.writeEndElement(); // file
            }
            stream.writeEndElement(); // release
        }
    }

    stream.writeEndElement(); // manifest
    stream.writeEndDocument();

    file.close();
}

QMap<QString, QString> UpdateManager::readCache(const QString &path) const
{
    QMap<QString, QString> r;

    QFile file(path + "/cache.txt");
    if (!file.open(QIODevice::ReadOnly))
        return r;

    QTextStream in(&file);
    while(!in.atEnd())
    {
        QString line = in.readLine();
        QStringList fields = line.split(",");
        if (fields.length() != 2)
            continue;
        r[fields[0]] = fields[1]; // file,hash
    }
    return r;
}

void UpdateManager::writeCache(const QString &path, QMap<QString, QString> cache) const
{
    QFile file(path + "/cache.txt");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QTextStream out(&file);
    for (auto e : cache.keys())
    {
        out << e << "," << cache.value(e) << endl;
    }
}

void UpdateManager::generateUpdate(const QString &path, const QString &plat, const QString &product, const QString &version, QWidget *)
{
    auto prodFolder = product.toLower().remove(' ').remove(':');
    auto cache = readCache(path + "/" + plat);
    QMap<QString, QString> changes;
    QList<CFileInfo> changesInfo;
    QDir(path).mkdir(path + "/../update/");

    struct Entry
    {
        QString srcfile;
        QString file;
        QByteArray hash;
        QString base;
    };

    auto createEntry = [&](const Entry &e) -> CFileInfo {
        auto hex = e.hash.toHex();
        CFileInfo info;
        info.Name = e.file;
        info.Hash = e.hash;
        auto b = QString(hex).right(2);
        info.ZipFileName = b + "/" + e.base.toLower() + "_" + hex;
        auto zipFile = path + "/../update/" + info.ZipFileName;
        QDir(path).mkdir(path + "/../update/" + b);
        QFile update(zipFile);
        if (!update.exists())
        {
            QZipWriter dst(zipFile);
            QFile src(e.srcfile);
            src.open(QFile::ReadOnly);
            const char *bytes = (const char *)src.map(0, src.size());
            assert(bytes);
            QByteArray ba = QByteArray::fromRawData(bytes, int(src.size()));
            dst.addFile(e.file, ba);
        }
        info.ZipHash = getHash(zipFile);

        if (cache[e.file.toLower()] != hex)
        {
            bool replaced = false;
            changes[e.file.toLower()] = hex;
            for (auto &it : changesInfo)
            {
                if (it.Name == info.Name)
                {
                    it = info;
                    replaced = true;
                }
            }
            if (!replaced)
            {
                changesInfo.push_back(info);
            }
        }

        return info;
    };

    auto collectEntries = [&](const QString &dirName) -> QList<Entry> {
        QList<Entry> entries;
        QDirIterator dir(path + "/" + dirName + "/", QDirIterator::Subdirectories);
        while (dir.hasNext())
        {
            auto srcfile = dir.next();
            auto fi = dir.fileInfo();
            if (fi.isDir())
                continue;

            auto name = fi.fileName();
            auto file = srcfile;
            file.remove(path + "/" + dirName + "/");

            auto hash = getHash(srcfile);
            entries.push_back({srcfile, file, hash, name});
        }
        return entries;
    };

    auto data = collectEntries(plat + "/" + prodFolder);
    auto manifest = path + "/" + plat + ".manifest.xml";

    QFile xml(manifest);
    xml.open(QFile::ReadWrite);
    QTextStream xmlData(&xml);
    readManifest(xmlData.readAll());

    for (const auto &prod : releaseList.keys())
    {
        for (const auto &rel : releaseList[prod])
        {
            if (prod == product)
            {
                releaseList[prod][rel.Version].latest = "false";
            }
        }
    }

    changesInfo = releaseList["all"]["incremental"].FileList;

    CReleaseInfo release;
    release.Name = product;
    release.latest = "true";
    if (plat.endsWith("-beta"))
    {
        release.Version = version + " (beta)";
    }
    else
    {
        release.Version = version;
    }

    for (auto e : data)
    {
        release.FileList.push_back(createEntry(e));
    }
    releaseList[product][release.Version] = release;

    releaseList["all"]["incremental"].Name = "all";
    releaseList["all"]["incremental"].latest = "false";
    releaseList["all"]["incremental"].Version = "incremental";
    releaseList["all"]["incremental"].FileList = changesInfo;

    writeManifest(manifest);

    if (!changes.isEmpty())
    {
        for (auto k : changes.keys())
        {
            cache[k] = changes[k];
        }
        writeCache(path + "/" + plat, cache);
    }
}
