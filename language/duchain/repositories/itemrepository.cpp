/*
   Copyright 2008 David Nolden <david.nolden.kdevelop@art-master.de>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "itemrepository.h"

#include <QDataStream>

#include <kstandarddirs.h>
#include <kcomponentdata.h>
#include <klockfile.h>
#include <kmessagebox.h>
#include <klocale.h>

#include <interfaces/icore.h>

#include "../duchain.h"

namespace KDevelop {

AbstractItemRepository::~AbstractItemRepository() {
}

ItemRepositoryRegistry::ItemRepositoryRegistry(QString openPath, KLockFile::Ptr lock) : m_cleared(false) {
  if(!openPath.isEmpty())
    open(openPath, m_cleared, lock);
}

QAtomicInt& ItemRepositoryRegistry::getCustomCounter(const QString& identity, int initialValue) {
  if(!m_customCounters.contains(identity))
    m_customCounters.insert(identity, new QAtomicInt(initialValue));
  return *m_customCounters[identity];
}

bool processExists(int pid) {
  ///@todo Find a cross-platform way of doing this!
  QFileInfo f(QString("/proc/%1").arg(pid));
  return f.exists();
}

///The global item-repository registry that is used by default
ItemRepositoryRegistry& allocateGlobalItemRepositoryRegistry() {
  
  KLockFile::Ptr lock;
  QString repoPath;
   KComponentData component("item repositories temp", QByteArray(), KComponentData::SkipMainComponentRegistration);
//   if(ICore::self()) {
///@todo Use the kde directory again, once we know how to get it in this early stage
//     QString baseDir = KStandardDirs::locateLocal("data", "kdevduchain");
    QString baseDir = QDir::homePath() + "/.kdevduchain";
    KStandardDirs::makeDir(baseDir);
    //Since each instance of kdevelop needs an own directory, iterate until we find a not-yet-used one
    for(int a = 0; a < 100; ++a) {
      QString specificDir = baseDir + QString("/%1").arg(a);
      KStandardDirs::makeDir(specificDir);
       lock = new KLockFile(specificDir + "/lock", component);
       KLockFile::LockResult result = lock->lock(KLockFile::NoBlockFlag | KLockFile::ForceFlag);

       bool useDir = false;
       
       if(result == KLockFile::LockFail) {
         int pid;
         QString hostname, appname;
         if(lock->getLockInfo(pid, hostname, appname)) {
           if(!processExists(pid)) {
             kDebug() << "The process holding" << specificDir << "does not exists any more. Re-using the directory.";
             QFile::remove(specificDir + "/lock");
             useDir = true;
             if(lock->lock(KLockFile::NoBlockFlag | KLockFile::ForceFlag) != KLockFile::LockOK) {
               kWarning() << "Failed to re-establish the lock in" << specificDir;
               continue;
             }
           }
         }
       }else{
         useDir = true;
       }
       if(useDir) {
          repoPath = specificDir;
          if(result == KLockFile::LockStale)
            kWarning() << "stale lock detected:" << specificDir + "/lock";
          break;
       }
    }
    
    if(repoPath.isEmpty()) {
      kWarning() << "could not create a directory for the duchain data";
    }else{
      kDebug() << "picked duchain directory" << repoPath;
    }
//   }
  
  static ItemRepositoryRegistry global(repoPath, lock);
  return global;
}

///The global item-repository registry that is used by default
ItemRepositoryRegistry& globalItemRepositoryRegistry() {
  
  static ItemRepositoryRegistry& global(allocateGlobalItemRepositoryRegistry());
  return global;
}

void ItemRepositoryRegistry::registerRepository(AbstractItemRepository* repository) {
  m_repositories << repository;
  if(!m_path.isEmpty())
    repository->open(m_path, m_cleared);
}

QString ItemRepositoryRegistry::path() const {
return m_path;
}

void ItemRepositoryRegistry::lockForWriting() {
  //Create is_writing
  QFile f(m_path + "/is_writing");
  f.open(QIODevice::WriteOnly);
  f.close();
}

void ItemRepositoryRegistry::unlockForWriting() {
  //Delete is_writing
  QFile::remove(m_path + "/is_writing");
}

void ItemRepositoryRegistry::unRegisterRepository(AbstractItemRepository* repository) {
  Q_ASSERT(m_repositories.contains(repository));
  repository->close();
  m_repositories.removeAll(repository);
}

bool ItemRepositoryRegistry::open(const QString& path, bool clear, KLockFile::Ptr lock) {
  if(m_path == path && m_cleared == clear)
    return true;
  
  QFileInfo wasWriting(m_path + "/is_writing");
  if(wasWriting.exists()) {
    clear = true;
  }
  
  m_path = path;
  m_cleared = clear;
  if(clear) {
      kWarning() << "The data-repository at %1 has to be cleared. Either the disk format has changed, or KDevelop crashed while writing the repository" << m_path;
//     KMessageBox::information( 0, i18n("The data-repository at %1 has to be cleared. Either the disk format has changed, or KDevelop crashed while writing the repository.", m_path ) );
  }
  
  foreach(AbstractItemRepository* repository, m_repositories) {
    if(!repository->open(path, clear)) {
      Q_ASSERT(!clear); //We have a problem if opening a repository fails although it should be cleared
      close();
      open(path, true, m_lock);
    }
  }
  
  QFile f(path + "/Counters");
  if(f.open(QIODevice::ReadOnly)) {
    QDataStream stream(&f);
    
    while(!stream.atEnd()) {
      //Read in all custom counter values
      QString counterName;
      stream >> counterName;
      int counterValue;
      stream >> counterValue;
      if(m_customCounters.contains(counterName))
        *m_customCounters[counterName] = counterValue;
      else
        getCustomCounter(counterName, 0) = counterValue;
    }
  }else{
    kDebug() << "Could not open counter file";
  }
  
  m_lock = lock;
  return true;
}

void ItemRepositoryRegistry::store() {
  foreach(AbstractItemRepository* repository, m_repositories)
    repository->store();

  //Store all custom counter values
  QFile f(m_path + "/Counters");
  if(f.open(QIODevice::WriteOnly)) {
    f.resize(0);
    QDataStream stream(&f);
    for(QMap<QString, QAtomicInt*>::const_iterator it = m_customCounters.begin(); it != m_customCounters.end(); ++it) {
      stream << it.key();
      stream << it.value()->fetchAndAddRelaxed(0);
    }
  }else{
    kWarning() << "Could not open counter file for writing";
  }
}

void ItemRepositoryRegistry::close() {
    store();
    
  foreach(AbstractItemRepository* repository, m_repositories)
    repository->close();
  
  m_path = QString();
  m_cleared = false;
}

ItemRepositoryRegistry::~ItemRepositoryRegistry() {
  close();
  foreach(QAtomicInt* counter, m_customCounters.values())
    delete counter;
}

}
