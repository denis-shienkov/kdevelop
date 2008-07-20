/***************************************************************************
 *   This file was partly taken from KDevelop's cvs plugin                 *
 *   Copyright 2007 Robert Gruber <rgruber@users.sourceforge.net>          *
 *                                                                         *
 *   Adapted for DVCS                                                      *
 *   Copyright 2008 Evgeniy Ivanov <powerfox@kde.ru>                       *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "idvcsexecutor.h"

#include <QFileInfo>
#include <QDir>

#include <KDebug>
#include <KShell>

#include "dvcsjob.h"

using KDevelop::IDVCSexecutor;


bool IDVCSexecutor::prepareJob(DVCSjob* job, const QString& repository, enum RequestedOperation op)
{
    // Only do this check if it's a normal operation like diff, log ...
    // For other operations like "git clone" isValidDirectory() would fail as the
    // directory is not yet under git control
    if (op == NormalOperation &&
        !isValidDirectory(repository)) {
        kDebug(9500) << repository << " is not a valid repository";
        return false;
    }

    // clear commands and args from a possible previous run
    job->clear();

    //repository is sent by ContextMenu, so we check if it is a file and use it's path
    //if it's a dir then we use it as a working dir and use "git add ."
    QFileInfo repoInfo = QFileInfo(repository);
    if (repoInfo.isFile())
        job->setDirectory(repoInfo.path());
    else
        job->setDirectory(repository);

    return true;
}

bool IDVCSexecutor::addFileList(DVCSjob* job, const KUrl::List& urls)
{
    QStringList args;

    foreach(KUrl url, urls) {
        ///@todo this is ok for now, but what if some of the urls are not
        ///      to the given repository
        QFileInfo fileInfo = QFileInfo(url.path());
        QString file = url.path();
        ///@todo else will work only for relative pathes
        ///I don't think any ctions required, "." in working dir is ok for us
        if (fileInfo.isFile() || fileInfo.isDir())
            file = fileInfo.fileName();
        else
            file = QString(".");
        qDebug() << "url is: " << url <<" url.path() is " << url.path()  << " file is: " << file;
        args << KShell::quoteArg(file);
    }

    *job << args;
    return true;
}


///TODO: imlement in HG and Bazaar!
void IDVCSexecutor::parseOutput(const QString& jobOutput,
                                QList<DVCScommit>& revisions) const 
{
    Q_UNUSED(jobOutput)
    Q_UNUSED(revisions)
}

DVCSjob* IDVCSexecutor::checkout(const QString &repository, const QString &branch) 
{
    Q_UNUSED(repository)
    Q_UNUSED(branch)
    return empty_cmd();
}

DVCSjob* IDVCSexecutor::log(const KUrl& url)
{
    Q_UNUSED(url)
            return empty_cmd();
}
