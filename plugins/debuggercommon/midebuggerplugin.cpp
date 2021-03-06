/*
 * Common code for MI debugger support
 *
 * Copyright 1999-2001 John Birch <jbb@kdevelop.org>
 * Copyright 2001 by Bernd Gehrmann <bernd@kdevelop.org>
 * Copyright 2006 Vladimir Prus <ghost@cs.msu.su>
 * Copyright 2007 Hamish Rodda <rodda@kde.org>
 * Copyright 2016  Aetf <aetf@unlimitedcodeworks.xyz>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License or (at your option) version 3 or any later version
 * accepted by the membership of KDE e.V. (or its successor approved
 * by the membership of KDE e.V.), which shall act as a proxy
 * defined in Section 14 of version 3 of the license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "midebuggerplugin.h"

#include "midebugjobs.h"
#include "dialogs/processselection.h"

#include <interfaces/context.h>
#include <interfaces/contextmenuextension.h>
#include <interfaces/icore.h>
#include <interfaces/idebugcontroller.h>
#include <interfaces/iruncontroller.h>
#include <interfaces/iuicontroller.h>
#include <language/interfaces/editorcontext.h>
#include <sublime/message.h>
#include <isession.h>

#include <KActionCollection>
#include <KLocalizedString>
#include <KMessageBox>
#include <KParts/MainWindow>
#include <KStringHandler>

#include <QAction>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QPointer>
#include <QTimer>

using namespace KDevelop;
using namespace KDevMI;

class KDevMI::DBusProxy : public QObject
{
    Q_OBJECT

public:
    DBusProxy(const QString& service, const QString& name, QObject* parent)
        : QObject(parent),
          m_dbusInterface(service, QStringLiteral("/debugger")),
          m_name(name), m_valid(true)
    {}

    ~DBusProxy() override
    {
        if (m_valid) {
            m_dbusInterface.call(QStringLiteral("debuggerClosed"), m_name);
        }
    }

    QDBusInterface* interface()
    {
        return &m_dbusInterface;
    }

    void Invalidate()
    {
        m_valid = false;
    }

public Q_SLOTS:
    void debuggerAccepted(const QString& name)
    {
        if (name == m_name) {
            emit debugProcess(this);
        }
    }

    void debuggingFinished()
    {
        m_dbusInterface.call(QStringLiteral("debuggingFinished"), m_name);
    }

Q_SIGNALS:
    void debugProcess(DBusProxy*);

private:
    QDBusInterface m_dbusInterface;
    QString m_name;
    bool m_valid;
};

MIDebuggerPlugin::MIDebuggerPlugin(const QString &componentName, const QString& displayName, QObject *parent)
    : KDevelop::IPlugin(componentName, parent), m_displayName(displayName)
{
    core()->debugController()->initializeUi();

    setupActions();
    setupDBus();
}

void MIDebuggerPlugin::setupActions()
{
    KActionCollection* ac = actionCollection();

    auto * action = new QAction(this);
    action->setIcon(QIcon::fromTheme(QStringLiteral("core")));
    action->setText(i18nc("@action", "Examine Core File with %1", m_displayName));
    action->setWhatsThis(i18nc("@info:whatsthis",
                              "<b>Examine core file</b>"
                              "<p>This loads a core file, which is typically created "
                              "after the application has crashed, e.g. with a "
                              "segmentation fault. The core file contains an "
                              "image of the program memory at the time it crashed, "
                              "allowing you to do a post-mortem analysis.</p>"));
    connect(action, &QAction::triggered, this, &MIDebuggerPlugin::slotExamineCore);
    ac->addAction(QStringLiteral("debug_core"), action);

#if HAVE_KSYSGUARD
    action = new QAction(this);
    action->setIcon(QIcon::fromTheme(QStringLiteral("connect_creating")));
    action->setText(i18nc("@action", "Attach to Process with %1", m_displayName));
    action->setWhatsThis(i18nc("@info:whatsthis",
                              "<b>Attach to process</b>"
                              "<p>Attaches the debugger to a running process.</p>"));
    connect(action, &QAction::triggered, this, &MIDebuggerPlugin::slotAttachProcess);
    ac->addAction(QStringLiteral("debug_attach"), action);
#endif
}

void MIDebuggerPlugin::setupDBus()
{
    QDBusConnectionInterface* dbusInterface = QDBusConnection::sessionBus().interface();
    const auto& registeredServiceNames = dbusInterface->registeredServiceNames().value();
    for (const auto& service : registeredServiceNames) {
        slotDBusOwnerChanged(service, QString(), QStringLiteral("n"));
    }

    connect(dbusInterface, &QDBusConnectionInterface::serviceOwnerChanged,
            this, &MIDebuggerPlugin::slotDBusOwnerChanged);
}

void MIDebuggerPlugin::unload()
{
    unloadToolViews();
}

MIDebuggerPlugin::~MIDebuggerPlugin()
{
}

void MIDebuggerPlugin::slotDBusOwnerChanged(const QString& service, const QString& oldOwner, const QString& newOwner)
{
    if (oldOwner.isEmpty() && service.startsWith(QLatin1String("org.kde.drkonqi"))) {
        if (m_drkonqis.contains(service)) {
            return;
        }
        // New registration
        const QString name = i18n("KDevelop (%1) - %2", m_displayName, core()->activeSession()->name());
        auto drkonqiProxy = new DBusProxy(service, name, this);
        m_drkonqis.insert(service, drkonqiProxy);
        connect(drkonqiProxy->interface(), SIGNAL(acceptDebuggingApplication(QString)),
                drkonqiProxy, SLOT(debuggerAccepted(QString)));
        connect(drkonqiProxy, &DBusProxy::debugProcess,
                this, &MIDebuggerPlugin::slotDebugExternalProcess);

        drkonqiProxy->interface()->call(QStringLiteral("registerDebuggingApplication"), name, QCoreApplication::applicationPid());
    } else if (newOwner.isEmpty() && service.startsWith(QLatin1String("org.kde.drkonqi"))) {
        // Deregistration
        const auto proxyIt = m_drkonqis.find(service);
        if (proxyIt != m_drkonqis.end()) {
            auto proxy = *proxyIt;
            m_drkonqis.erase(proxyIt);
            proxy->Invalidate();
            delete proxy;
        }
    }
}

void MIDebuggerPlugin::slotDebugExternalProcess(DBusProxy* proxy)
{
    QDBusReply<int> reply = proxy->interface()->call(QStringLiteral("pid"));
    if (reply.isValid()) {
        connect(attachProcess(reply.value()), &KJob::result,
                proxy, &DBusProxy::debuggingFinished);
    }

    core()->uiController()->activeMainWindow()->raise();
}

ContextMenuExtension MIDebuggerPlugin::contextMenuExtension(Context* context, QWidget* parent)
{
    ContextMenuExtension menuExt = IPlugin::contextMenuExtension(context, parent);

    if (context->type() != KDevelop::Context::EditorContext)
        return menuExt;

    auto *econtext = dynamic_cast<EditorContext*>(context);
    if (!econtext)
        return menuExt;

    QString contextIdent = econtext->currentWord();

    if (!contextIdent.isEmpty())
    {
        QString squeezed = KStringHandler::csqueeze(contextIdent, 30);

        auto* action = new QAction(parent);
        action->setText(i18nc("@action:inmenu", "Evaluate: %1", squeezed));
        action->setWhatsThis(i18nc("@info:whatsthis",
                                  "<b>Evaluate expression</b>"
                                  "<p>Shows the value of the expression under the cursor.</p>"));
        connect(action, &QAction::triggered, this, [this, contextIdent](){
            emit addWatchVariable(contextIdent);
        });
        menuExt.addAction(ContextMenuExtension::DebugGroup, action);

        action = new QAction(parent);
        action->setText(i18nc("@action:inmenu", "Watch: %1", squeezed));
        action->setWhatsThis(i18nc("@info:whatsthis",
                                  "<b>Watch expression</b>"
                                  "<p>Adds the expression under the cursor to the Variables/Watch list.</p>"));
        connect(action, &QAction::triggered, this, [this, contextIdent](){
            emit evaluateExpression(contextIdent);
        });
        menuExt.addAction(ContextMenuExtension::DebugGroup, action);
    }

    return menuExt;
}

void MIDebuggerPlugin::slotExamineCore()
{
    showStatusMessage(i18n("Choose a core file to examine..."), 1000);

    if (core()->debugController()->currentSession() != nullptr) {
        KMessageBox::ButtonCode answer = KMessageBox::warningYesNo(
            core()->uiController()->activeMainWindow(),
            i18n("A program is already being debugged. Do you want to abort the "
                 "currently running debug session and continue?"));
        if (answer == KMessageBox::No)
            return;
    }
    auto *job = new MIExamineCoreJob(this, core()->runController());
    core()->runController()->registerJob(job);
    // job->start() is called in registerJob
}

#if HAVE_KSYSGUARD
void MIDebuggerPlugin::slotAttachProcess()
{
    showStatusMessage(i18n("Choose a process to attach to..."), 1000);

    if (core()->debugController()->currentSession() != nullptr) {
        KMessageBox::ButtonCode answer = KMessageBox::warningYesNo(
            core()->uiController()->activeMainWindow(),
            i18n("A program is already being debugged. Do you want to abort the "
                 "currently running debug session and continue?"));
        if (answer == KMessageBox::No)
            return;
    }

    QPointer<ProcessSelectionDialog> dlg = new ProcessSelectionDialog(core()->uiController()->activeMainWindow());
    if (!dlg->exec() || !dlg->pidSelected()) {
        delete dlg;
        return;
    }

    // TODO: move check into process selection dialog
    int pid = dlg->pidSelected();
    delete dlg;
    if (QApplication::applicationPid() == pid) {
        const QString messageText =
            i18n("Not attaching to process %1: cannot attach the debugger to itself.", pid);
        auto* message = new Sublime::Message(messageText, Sublime::Message::Error);
        ICore::self()->uiController()->postMessage(message);
    }
    else
        attachProcess(pid);
}
#endif

MIAttachProcessJob* MIDebuggerPlugin::attachProcess(int pid)
{
    auto *job = new MIAttachProcessJob(this, pid, core()->runController());
    core()->runController()->registerJob(job);
    // job->start() is called in registerJob

    return job;
}

QString MIDebuggerPlugin::statusName() const
{
    return i18n("Debugger");
}

void MIDebuggerPlugin::showStatusMessage(const QString& msg, int timeout)
{
    emit showMessage(this, msg, timeout);
}

#include "midebuggerplugin.moc"
