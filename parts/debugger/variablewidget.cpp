// **************************************************************************
//    begin                : Sun Aug 8 1999
//    copyright            : (C) 1999 by John Birch
//    email                : jbb@kdevelop.org
// **************************************************************************

// **************************************************************************
// *                                                                        *
// *   This program is free software; you can redistribute it and/or modify *
// *   it under the terms of the GNU General Public License as published by *
// *   the Free Software Foundation; either version 2 of the License, or    *
// *   (at your option) any later version.                                  *
// *                                                                        *
// **************************************************************************

#include "variablewidget.h"
#include "gdbparser.h"

#include <kdebug.h>
#include <kpopupmenu.h>
#include <klineedit.h>
#include <kdeversion.h>

#include <qheader.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qpushbutton.h>
#include <qregexp.h>
#include <qcursor.h>
#include <klocale.h>

#include <qpoint.h>
#include <qclipboard.h>
#include <kapplication.h>

// **************************************************************************
// **************************************************************************
// **************************************************************************

namespace GDBDebugger
{

VariableWidget::VariableWidget(QWidget *parent, const char *name)
    : QWidget(parent, name)
{
    varTree_ = new VariableTree(this);
    QLabel *label = new QLabel(i18n("Expression to watch:"), this);

    watchVarEditor_ = new KHistoryCombo(this, "var-to-watch editor");

//    watchVarEntry_ = new KLineEdit(this);

    QPushButton *addButton = new QPushButton(i18n("Add"), this);

    QBoxLayout *watchEntry = new QHBoxLayout();
    watchEntry->addWidget(label);
//    watchEntry->addWidget(watchVarEntry_);
    watchEntry->addWidget(watchVarEditor_);
    watchEntry->setStretchFactor(watchVarEditor_, 1);
    watchEntry->addWidget(addButton);

    QVBoxLayout *topLayout = new QVBoxLayout(this, 2);
    topLayout->addWidget(varTree_, 10);
    topLayout->addLayout(watchEntry);

    connect( addButton, SIGNAL(clicked()), SLOT(slotAddWatchVariable()) );
    connect( watchVarEditor_, SIGNAL(returnPressed()), SLOT(slotAddWatchVariable()) );
//    connect( watchVarEntry_, SIGNAL(returnPressed()), SLOT(slotAddWatchVariable()) );
}

// **************************************************************************

void VariableWidget::clear()
{
//  varTree_->clear();
  QListViewItemIterator it(varTree_);
  while (it.current())
  {
    if (!dynamic_cast<WatchRoot*>(varTree_->findRoot(it.current())))
    {
      QListViewItem *item = it.current();
      delete item;
    } else
    {
      ++it;
    }
  }
}

// **************************************************************************

void VariableWidget::setEnabled(bool bEnabled)
{
    QWidget::setEnabled(bEnabled);
    if (bEnabled && parentWidget()) {
        varTree_->setColumnWidth(0, parentWidget()->width()/2);
  }
}
// **************************************************************************

void VariableWidget::slotAddWatchVariable()
{
//    QString watchVar(watchVarEntry_->text());
    QString watchVar(watchVarEditor_->currentText());
    if (!watchVar.isEmpty())
    {
        slotAddWatchVariable(watchVar);
    }
}

// **************************************************************************

void VariableWidget::slotAddWatchVariable(const QString &ident)
{
    if (!ident.isEmpty())
    {
        watchVarEditor_->addToHistory(ident);
        varTree_->slotAddWatchVariable(ident);
        watchVarEditor_->clearEdit();
    }
}

// **************************************************************************
// **************************************************************************
// **************************************************************************

VariableTree::VariableTree(VariableWidget *parent, const char *name)
    : KListView(parent, name),
      QToolTip( viewport() ),
      activeFlag_(0),
      currentThread_(-1)
{
    setRootIsDecorated(true);
    setAllColumnsShowFocus(true);
    setColumnWidthMode(0, Manual);
    setSorting(-1);
    QListView::setSelectionMode(QListView::Single);

    addColumn(i18n("Variable"));
    addColumn(i18n("Value"));
    addColumn(i18n("Type"));

    connect( this, SIGNAL(contextMenu(KListView*, QListViewItem*, const QPoint&)),
             SLOT(slotContextMenu(KListView*, QListViewItem*)) );

/*
    work in progress - disabled for now

    // jw
    connect( this, SIGNAL(doubleClicked(QListViewItem *item, const QPoint &pos, int c)),
             SLOT(slotDoubleClicked(QListViewItem *item, const QPoint &pos, int c)) );
*/
}

// **************************************************************************

VariableTree::~VariableTree()
{
}

// **************************************************************************

void VariableTree::slotContextMenu(KListView *, QListViewItem *item)
{
    if (!item)
        return;

    setSelected(item, true);    // Need to select this item.

    if (item->parent())
    {
        KPopupMenu popup(item->text(VarNameCol), this);
        int idRemoveWatch = -2;
        if (dynamic_cast<WatchRoot*>(findRoot(item)))
            idRemoveWatch = popup.insertItem( i18n("Remove Watch Variable") );

        int idToggleWatch = popup.insertItem( i18n("Toggle Watchpoint") );
        int	idCopyToClipboard = popup.insertItem( i18n("Copy to clipboard") );
        int res = popup.exec(QCursor::pos());

        if (res == idRemoveWatch)
            delete item;
        else if (res == idCopyToClipboard)
        {
            QClipboard *qb = KApplication::clipboard();
            QString text = "{ \"" + item->text( 0 ) + "\", " + // name
                            "\"" + item->text( 2 ) + "\", " + // type
                            "\"" + item->text( 1 ) + "\" }";  // value

#if KDE_VERSION > 305
            qb->setText( text, QClipboard::Clipboard );
#else
            qb->setText( text );
#endif
        }
        else if (res == idToggleWatch)
        {
            if (VarItem *item = dynamic_cast<VarItem*>(currentItem()))
                emit toggleWatchpoint(item->fullName());
        }
    }
}

// **************************************************************************

void VariableTree::slotAddWatchVariable(const QString &watchVar)
{
    kdDebug(9012) << "Add watch variable: " << watchVar << endl;
    VarItem *varItem = new VarItem(findWatch(), watchVar, typeUnknown);
    emit expandItem(varItem);
}

// **************************************************************************

// jw
void VariableTree::slotDoubleClicked(QListViewItem *item, const QPoint &pos, int c)
{
    kdDebug(9012) << " ### VariableTree::slotDoubleClicked 1" << endl;

    if (item)
    {
        kdDebug(9012) << " ### VariableTree::slotDoubleClicked 2" << endl;
        TrimmableItem *titem = dynamic_cast<TrimmableItem*>(item);
        if (titem)
        {
            kdDebug(9012) << " ### VariableTree::slotDoubleClicked 2" << endl;
            titem->handleDoubleClicked(pos, c);
        }
    }
}

// **************************************************************************

void VariableTree::setLocalViewState(bool localsOn, int frameNo, int threadNo)
{
    // When they want to _close_ a frame then we need to check the state of
    // all other frames to determine whether we still need the locals.
    if (!localsOn) {
        QListViewItem *sibling = firstChild();
        while (sibling) {
            VarFrameRoot *frame = dynamic_cast<VarFrameRoot*> (sibling);
            if (frame && frame->isOpen()) {
                localsOn = true;
                break;
            }

            sibling = sibling->nextSibling();
        }
    }

    emit setLocalViewState(localsOn);
    emit selectFrame(frameNo, threadNo);
}


// **************************************************************************

QListViewItem *VariableTree::findRoot(QListViewItem *item) const
{
    while (item->parent())
        item = item->parent();

    return item;
}

// **************************************************************************

VarFrameRoot *VariableTree::findFrame(int frameNo, int threadNo) const
{
    QListViewItem *sibling = firstChild();

    // frames only exist on th top level so we only need to
    // check the siblings
    while (sibling) {
        VarFrameRoot *frame = dynamic_cast<VarFrameRoot*> (sibling);
        if (frame && frame->matchDetails(frameNo, threadNo))
            return frame;

        sibling = sibling->nextSibling();
    }

    return 0;
}

// **************************************************************************

WatchRoot *VariableTree::findWatch()
{
    QListViewItem *sibling = firstChild();

    while (sibling) {
        if (WatchRoot *watch = dynamic_cast<WatchRoot*> (sibling))
            return watch;

        sibling = sibling->nextSibling();
    }

    return new WatchRoot(this);
}

// **************************************************************************

void VariableTree::trim()
{
    QListViewItem *child = firstChild();

    while (child) {
        QListViewItem *nextChild = child->nextSibling();

        // don't trim the watch root
        if (!(dynamic_cast<WatchRoot*> (child))) {
            if (TrimmableItem *item = dynamic_cast<TrimmableItem*> (child)) {
                if (item->isActive())
                    item->trim();
                else
                    delete item;
            }
        }
        child = nextChild;
    }
}

// **************************************************************************

void VariableTree::trimExcessFrames()
{
    viewport()->setUpdatesEnabled(false);
    QListViewItem *child = firstChild();

    while (child) {
        QListViewItem *nextChild = child->nextSibling();
        if (VarFrameRoot *frame = dynamic_cast<VarFrameRoot*> (child)) {
            if (!frame->matchDetails(0, currentThread_))
                delete frame;
        }
        child = nextChild;
    }
    viewport()->setUpdatesEnabled(true);
    repaint();
}

// **************************************************************************

QListViewItem *VariableTree::lastChild() const
{
    QListViewItem *child = firstChild();
    if (child)
        while (QListViewItem *nextChild = child->nextSibling())
            child = nextChild;

    return child;
}

// **************************************************************************

void VariableTree::maybeTip(const QPoint &p)
{
    kdDebug(9012) << "ToolTip::maybeTip()" << endl;

    VarItem * item = dynamic_cast<VarItem*>( itemAt( p ) );
    if ( item )
    {
        QRect r = itemRect( item );
        if ( r.isValid() )
            tip( r, item->tipText() );
    }
}

// **************************************************************************
// **************************************************************************
// **************************************************************************

TrimmableItem::TrimmableItem(VariableTree *parent)
    : KListViewItem (parent, parent->lastChild()),
      activeFlag_(0)
{
    setActive();
}

// **************************************************************************

TrimmableItem::TrimmableItem(TrimmableItem *parent)
    : KListViewItem (parent, parent->lastChild()),
      activeFlag_(0),
      waitingForData_(false)
{
    setActive();
}

// **************************************************************************

TrimmableItem::~TrimmableItem()
{
}

// **************************************************************************

void TrimmableItem::paintCell(QPainter *p, const QColorGroup &cg,
                              int column, int width, int align)
{
    if ( !p )
        return;
    // make toplevel item (watch and frame items) names bold
    if (column == 0 && !parent())
    {
        QFont f = p->font();
        f.setBold(true);
        p->setFont(f);
    }
    QListViewItem::paintCell( p, cg, column, width, align );
}

// **************************************************************************

int TrimmableItem::rootActiveFlag() const
{
    return ((VariableTree*)listView())->activeFlag();
}

// **************************************************************************

bool TrimmableItem::isTrimmable() const
{
    return !waitingForData_;
}

// **************************************************************************

QListViewItem *TrimmableItem::lastChild() const
{
    QListViewItem *child = firstChild();
    if (child)
        while (QListViewItem *nextChild = child->nextSibling())
            child = nextChild;

    return child;
}

// **************************************************************************

TrimmableItem *TrimmableItem::findMatch(const QString &match, DataType type) const
{
    QListViewItem *child = firstChild();

    // Check the siblings on this branch
    while (child) {
        if (child->text(VarNameCol) == match) {
            if (TrimmableItem *item = dynamic_cast<TrimmableItem*> (child))
                if (item->getDataType() == type)
                    return item;
        }

        child = child->nextSibling();
    }

    return 0;
}

// **************************************************************************

void TrimmableItem::trim()
{
    QListViewItem *child = firstChild();

    while (child) {
        QListViewItem *nextChild = child->nextSibling();
        if (TrimmableItem *item = dynamic_cast<TrimmableItem*>(child)) {
            // Never trim a branch if we are waiting on data to arrive.
            if (isTrimmable()) {
                if (item->isActive())
                    item->trim();      // recurse
                else
                    delete item;
            }
        }
        child = nextChild;
    }
}

// **************************************************************************

DataType TrimmableItem::getDataType() const
{
    return typeUnknown;
}

// **************************************************************************

void TrimmableItem::setCache(const QCString&)
{
    Q_ASSERT(false);
}

// **************************************************************************

QCString TrimmableItem::getCache()
{
    Q_ASSERT(false);
    return QCString();
}

// **************************************************************************

void TrimmableItem::updateValue(char* /* buf */)
{
    waitingForData_ = false;
}

// **************************************************************************

QString TrimmableItem::key (int, bool) const
{
    return QString::null;
}

// **************************************************************************
// **************************************************************************
// **************************************************************************

VarItem::VarItem(TrimmableItem *parent, const QString &varName, DataType dataType)
    : TrimmableItem (parent),
      cache_(QCString()),
      dataType_(dataType),
      highlight_(false)
{
    setText(VarNameCol, varName);

/*
    setRenameEnabled(VarTypeCol, true);
    setRenameEnabled(VarNameCol, true);
*/

    kdDebug(9012) << " ### VarItem::VarItem *CONSTR*" << endl;
    emit ((VariableTree*)listView())->varItemConstructed(this);
}

// **************************************************************************

VarItem::~VarItem()
{
}

// **************************************************************************

QString VarItem::varPath() const
{
    QString vPath("");
    const VarItem *item = this;

    // This stops at the root item (FrameRoot or WatchRoot)
    while ((item = dynamic_cast<const VarItem*> (item->parent()))) {
        if (item->getDataType() != typeArray) {
            if ((item->text(VarNameCol))[0] != '<') {
                QString itemName = item->text(VarNameCol);
                if (vPath.isEmpty())
                    vPath = itemName.replace(QRegExp("^static "), "");
                else
                    vPath = itemName.replace(QRegExp("^static "), "") + "." + vPath;
            }
        }
    }

    return vPath;
}

// **************************************************************************

QString VarItem::fullName() const
{
    QString itemName = getName();
    QString vPath = varPath();
    if (itemName[0] == '<')
        return vPath;

    if (vPath.isEmpty())
        return itemName.replace(QRegExp("^static "), "");

    return varPath() + "." + itemName.replace(QRegExp("^static "), "");
}

// **************************************************************************

void VarItem::setText(int column, const QString &data)
{
    if (!isActive() && isOpen() && dataType_ == typePointer) {
        waitingForData();
        ((VariableTree*)listView())->expandItem(this);
    }

    setActive();
    if (column == ValueCol) {
        QString oldValue(text(column));
        if (!oldValue.isEmpty())                   // Don't highlight new items
            highlight_ = (oldValue != QString(data));
    }

    QListViewItem::setText(column, data);
    repaint();
}

// **************************************************************************

void VarItem::updateValue(char *buf)
{
    TrimmableItem::updateValue(buf);

    // Hack due to my bad QString implementation - this just tidies up the display
    if ((strncmp(buf, "There is no member named len.", 29) == 0) ||
        (strncmp(buf, "There is no member or method named len.", 39) == 0))
        return;

    if (*buf == '$') {
        if (char *end = strchr(buf, '='))
            buf = end+2;
    }

    if (dataType_ == typeUnknown) {
        dataType_ = GDBParser::getGDBParser()->determineType(buf);
        if (dataType_ == typeArray)
            buf++;

        // Try fixing a format string here by overriding the dataType calculated
        // from this data
        QString varName = getName();
        if (dataType_ == typePointer && varName[0] == '/')
            dataType_ = typeValue;
    }

    GDBParser::getGDBParser()->parseData(this, buf, true, false);
    setActive();
}

// **************************************************************************

void VarItem::updateType(char *buf)
{
    kdDebug(9012) << " ### VarItem::updateType " << buf << endl;

    QString str(buf);
    int eq = str.find('=');
    if (eq < 0)
        return;
    str.replace(QRegExp("[\n\r]"),"");
    str = str.mid(eq + 1, 0xffff).stripWhiteSpace();

    originalValueType_ = str.latin1();

    setText(VarTypeCol, str);
}

// **************************************************************************

void VarItem::handleDoubleClicked(const QPoint &/*pos*/, int c)
{
    kdDebug(9012) << " ### VarItem::handleDoubleClicked 1" << endl;
    if (c == VarTypeCol || c == ValueCol)
    {
        kdDebug(9012) << " ### VarItem::handleDoubleClicked 2" << endl;
        static_cast<KListView*>(listView())->rename(this, c);
    }
}

// **************************************************************************

void VarItem::setCache(const QCString &value)
{
    cache_ = value;
    setExpandable(true);
    checkForRequests();
    if (isOpen())
        setOpen(true);
    setActive();
}

// **************************************************************************

void VarItem::setOpen(bool open)
{
    if (open) {
        if (cache_) {
            QCString value = cache_;
            cache_ = QCString();
            GDBParser::getGDBParser()->parseData(this, value.data(), false, false);
            trim();
        } else {
            if (dataType_ == typePointer || dataType_ == typeReference) {
                waitingForData();
                emit ((VariableTree*)listView())->expandItem(this);
            }
        }
    }

    QListViewItem::setOpen(open);
}

// **************************************************************************

QCString VarItem::getCache()
{
    return cache_;
}

// **************************************************************************

void VarItem::checkForRequests()
{
    // This shouldn't be needed to keep it from blowing up, but sometimes is.
    // On the other hand, if it's empty, there is no reason to go on...
    if ( cache_.isEmpty() ) return;

    /// @todo - hardcoded for now - these should get read from config

    // Signature for a QT1.44 QString
    if (strncmp(cache_, "<QArrayT<char>> = {<QGArray> = {shd = ", 38) == 0) {
        waitingForData();
        emit ((VariableTree*)listView())->expandUserItem(this,
                                                         fullName().latin1()+QCString(".shd.data"));
    }

    // Signature for a QT1.44 QDir
    if (strncmp(cache_, "dPath = {<QArrayT<char>> = {<QGArray> = {shd", 44) == 0) {
        waitingForData();
        emit ((VariableTree*)listView())->expandUserItem(this,
                                                         fullName().latin1()+QCString(".dPath.shd.data"));
    }

    // Signature for a QT2.x QT3.x QString
    /// @todo - This handling is not that good - but it works sufficiently well
    // at the moment to leave it here, and it won't cause bad things to happen.
    if (strncmp(cache_, "d = 0x", 6) == 0) {     // Eeeek - too small
        waitingForData();
        emit ((VariableTree*)listView())->expandUserItem(this,
                                                         // QCString().sprintf("(($len=($data=%s.d).len)?$data.unicode.rw@($len>100?200:$len*2):\"\")",
                                                         QCString().sprintf("(($len=($data=%s.d).len)?*((char*)&$data.unicode[0])@($len>100?200:$len*2):\"\")",
                                                                            fullName().latin1()));
    }

    // Signature for a QT2.0.x QT2.1 QCString
    if (strncmp(cache_, "<QArray<char>> = {<QGArray> = {shd = ", 37) == 0) {
        waitingForData();
        emit ((VariableTree*)listView())->expandUserItem(this,
                                                         fullName().latin1()+QCString(".shd.data"));
    }

    // Signature for a QT2.0.x QT2.1 QDir
    if (strncmp(cache_, "dPath = {d = 0x", 15) == 0) {
        waitingForData();
        ((VariableTree*)listView())->expandUserItem(this,
                                                    // QCString().sprintf("(($len=($data=%s.dPath.d).len)?$data.unicode.rw@($len>100?200:$len*2):\"\")",
                                                    QCString().sprintf("(($len=($data=%s.dPath.d).len)?*((char*)&$data.unicode[0])@($len>100?200:$len*2):\"\")",
                                                                       fullName().latin1()));
  }
}

// **************************************************************************

DataType VarItem::getDataType() const
{
    return dataType_;
}

// **************************************************************************

// Overridden to highlight the changed items
void VarItem::paintCell(QPainter *p, const QColorGroup &cg,
                        int column, int width, int align)
{
    if ( !p )
        return;

    if (column == ValueCol && highlight_) {
        QColorGroup hl_cg( cg.foreground(), cg.background(), cg.light(),
                           cg.dark(), cg.mid(), red, cg.base());
        QListViewItem::paintCell( p, hl_cg, column, width, align );
    } else
        QListViewItem::paintCell( p, cg, column, width, align );
}

// **************************************************************************

QString VarItem::tipText() const
{
    const unsigned int maxTooltipSize = 70;
    // FIXME: Column #1 is "Value": perhaps some kind of const somewhere is better ...
    QString tip = text( 1 );

    if (tip.length() < maxTooltipSize )
	    return tip;
    else
	    return tip.mid( 0, maxTooltipSize - 1 ) + " [...]";
}

// **************************************************************************
// **************************************************************************
// **************************************************************************

VarFrameRoot::VarFrameRoot(VariableTree *parent, int frameNo, int threadNo)
    : TrimmableItem (parent),
      needLocals_(true),
      frameNo_(frameNo),
      threadNo_(threadNo),
      params_(QCString()),
      locals_(QCString())
{
    setExpandable(true);
}

// **************************************************************************

VarFrameRoot::~VarFrameRoot()
{
}

// **************************************************************************

void VarFrameRoot::setParams(char *params)
{
    setActive();
    params_ = params;
}

// **************************************************************************

void VarFrameRoot::setLocals(char *locals)
{
    setActive();

    // "No symbol table info available" or "No locals."
    bool noLocals = (locals &&  (strncmp(locals, "No ", 3) == 0));
    setExpandable(!params_.isEmpty() || !noLocals);

    if (noLocals) {
        locals_ = "";
        if (locals)
            if (char *end = strchr(locals, '\n'))
                *end = 0;      // clobber the new line
    } else
        locals_ = locals;

    if (!isExpandable() && noLocals)
        setText( ValueCol, locals );

    needLocals_ = false;
    if (isOpen())
        setOpen(true);
}

// **************************************************************************

// Override setOpen so that we can decide what to do when we do change
// state. This
void VarFrameRoot::setOpen(bool open)
{
    bool localStateChange = (isOpen() != open);
    QListViewItem::setOpen(open);

    if (localStateChange)
        ((VariableTree*)listView())->setLocalViewState(open, frameNo_, threadNo_);

    if (!open)
        return;

    GDBParser::getGDBParser()->parseData(this, params_.data(), false, true);
    GDBParser::getGDBParser()->parseData(this, locals_.data(), false, false);

    locals_ = QCString();
    params_ = QCString();
}

// **************************************************************************

bool VarFrameRoot::matchDetails(int frameNo, int threadNo)
{
    return frameNo == frameNo_ && threadNo == threadNo_;
}

// **************************************************************************
// **************************************************************************
// **************************************************************************
// **************************************************************************

WatchRoot::WatchRoot(VariableTree *parent)
    : TrimmableItem(parent)
{
    setText(0, i18n("Watch"));
    setOpen(true);
}

// **************************************************************************

WatchRoot::~WatchRoot()
{
}

// **************************************************************************

void WatchRoot::requestWatchVars()
{
    for (QListViewItem *child = firstChild(); child; child = child->nextSibling())
        if (VarItem *varItem = dynamic_cast<VarItem*>(child))
            emit ((VariableTree*)listView())->expandItem(varItem);
}

// **************************************************************************
// **************************************************************************
// **************************************************************************

}

#include "variablewidget.moc"
