/* Copyright 2008 Aleix Pol <aleixpol@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "applychangeswidget.h"
#include "komparesupport.h"
#include <ktexteditor/document.h>
// #include <ktexteditor/editor.h>
// #include <ktexteditor/editorchooser.h>
#include <ktexteditor/view.h>

#include <shell/partcontroller.h>

#include <kparts/part.h>

#include <KTabWidget>
#include <KMimeType>
#include <KMimeTypeTrader>
#include <QLayout>
#include <QSplitter>
#include <QLabel>
#include <QStandardItemModel>
#include <QTreeView>
#include <QDebug>
#include <KPushButton>
#include "coderepresentation.h"

namespace KDevelop
{
    
struct ApplyChangesWidgetPrivate
{
    ApplyChangesWidgetPrivate(ApplyChangesWidget * p)
        : parent(p), m_index(0) {}
    
    void addItem(QStandardItemModel* mit, KTextEditor::Document *document, const KTextEditor::Range &range, const QString& type);
    void jump( const QModelIndex & idx);
    void createEditPart(const KDevelop::IndexedString& url, const QString& info);

    
    ApplyChangesWidget * const parent;
    unsigned int m_index;
    QList<KParts::ReadWritePart*> m_editParts;
    QList<QStandardItemModel*> m_changes;
    QList<QPair<IndexedString, IndexedString> > m_files;
    KTabWidget * m_documentTabs;
    
    KompareWidgets m_kompare;
};

ApplyChangesWidget::ApplyChangesWidget(QWidget* parent)
    : KDialog(parent), d(new ApplyChangesWidgetPrivate(this))
{
    setSizeGripEnabled(true);
    setInitialSize(QSize(800, 400));
    
    d->m_documentTabs = new KTabWidget(this);
    
    KDialog::setButtons(KDialog::Ok | KDialog::Cancel | KDialog::User1);
    KPushButton * switchButton(KDialog::button(KDialog::User1));
    switchButton->setText("Edit Document");
    switchButton->setEnabled(d->m_kompare.enabled);
    
    connect(switchButton, SIGNAL(released()),
            this, SLOT(switchEditView()));
    connect(d->m_documentTabs, SIGNAL(currentChanged(int)),
            this, SLOT(indexChanged(int)));
    
    setMainWidget(d->m_documentTabs);
}

ApplyChangesWidget::~ApplyChangesWidget()
{
    delete d;
}

KTextEditor::Document* ApplyChangesWidget::document() const
{
    return qobject_cast<KTextEditor::Document*>(d->m_editParts[d->m_index]);
}

void ApplyChangesWidget::addDocuments(const IndexedString & original, const IndexedString & modified, const QString & info)
{
    
    QWidget * w = new QWidget;
    d->m_documentTabs->addTab(w, original.index() ? original.str() : modified.str());
    
    if(d->m_kompare.createWidget(original, modified, w) == -1)
        d->createEditPart(modified, info);
    
#ifndef NDEBUG
    //Duplicated originals should not exist
    typedef QPair<IndexedString, IndexedString> StringPair;
    foreach( StringPair files, d->m_files)
        Q_ASSERT(files.first != original);
#endif
    d->m_files.insert(d->m_index, qMakePair(original, modified));
}

bool ApplyChangesWidget::applyAllChanges()
{
    /// @todo implement safeguard in case a file saving fails
    
    bool ret = true;
    for(unsigned int i = 0; i < static_cast<unsigned int>(d->m_files.size()); ++i )
        if(!d->m_editParts[i]->saveAs(d->m_files[i].first.toUrl()))
            ret = false;
        
    return ret;
}

}

Q_DECLARE_METATYPE(KTextEditor::Range)

namespace KDevelop
{

void ApplyChangesWidgetPrivate::addItem(QStandardItemModel* mit, KTextEditor::Document *document, const KTextEditor::Range &range, const QString& type)
{
    bool isFirst=mit->rowCount()==0;
    QStringList edition=document->textLines(range);
    if(edition.first().isEmpty())
        edition.takeFirst();
    QStandardItem* it= new QStandardItem(edition.join("\n"));
    QStandardItem* action= new QStandardItem(type);
    
    it->setData(qVariantFromValue(range));
    it->setEditable(false);
    action->setEditable(false);
    mit->appendRow(QList<QStandardItem*>() << it << action);
    if(isFirst)
        jump(it->index());
}

void ApplyChangesWidget::jump( const QModelIndex & idx)
{
    d->jump(idx);
}

void ApplyChangesWidgetPrivate::jump( const QModelIndex & idx)
{
    Q_ASSERT( static_cast<int>(m_index) == m_documentTabs->currentIndex());
    
    QStandardItem *it=m_changes[m_index]->itemFromIndex(idx);
    KTextEditor::View* view=qobject_cast<KTextEditor::View*>(m_editParts[m_index]->widget());
    KTextEditor::Range r=it->data().value<KTextEditor::Range>();
    view->setSelection(r);
    view->setCursorPosition(r.start());
}

void ApplyChangesWidgetPrivate::createEditPart(const IndexedString & file, const QString & info)
{
    QWidget * widget = m_documentTabs->currentWidget();
    Q_ASSERT(widget);
    
    QVBoxLayout *m=new QVBoxLayout(widget);
    QSplitter *v=new QSplitter(widget);
    
    KUrl url = file.toUrl();
    
    KMimeType::Ptr mimetype = KMimeType::findByUrl( url, 0, true );
    
    m_editParts.insert(m_index, KMimeTypeTrader::self()->createPartInstanceFromQuery<KParts::ReadWritePart>(mimetype->name(), widget, widget));
    
    //Open the best code representation, even if it is artificial
    CodeRepresentation::Ptr repr = createCodeRepresentation(file);
    
    //KateDocument does not accept streaming of data for some reason
    //Q_ASSERT(m_editParts[m_index]->openStream(mimetype->name(), url));
    //Q_ASSERT(m_editParts[m_index]->writeStream(repr->text().toLocal8Bit()));
    //Q_ASSERT(m_editParts[m_index]->closeStream());
    m_editParts[m_index]->openUrl(url);
    
    m_changes.insert(m_index, new QStandardItemModel(widget));
    m_changes[m_index]->setHorizontalHeaderLabels(QStringList(i18n("Text")) << i18n("Action"));
    
    QTreeView *changesView=new QTreeView(widget);
    changesView->setRootIsDecorated(false);
    changesView->setModel(m_changes[m_index]);
    v->addWidget(m_editParts[m_index]->widget());
    v->addWidget(changesView);
    v->setSizes(QList<int>() << 400 << 100);
    
    QLabel* l=new QLabel(info, widget);
    l->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed));
    m->addWidget(l);
    m->addWidget(v);
    
    QObject::connect(m_editParts[m_index], SIGNAL(textChanged(KTextEditor::Document*, KTextEditor::Range, KTextEditor::Range)),
            parent, SLOT(change (KTextEditor::Document*, KTextEditor::Range, KTextEditor::Range)));
    QObject::connect(m_editParts[m_index], SIGNAL(textInserted(KTextEditor::Document*, KTextEditor::Range)),
            parent, SLOT(insertion (KTextEditor::Document*, KTextEditor::Range)));
    QObject::connect(m_editParts[m_index], SIGNAL(textRemoved(KTextEditor::Document*, KTextEditor::Range)),
            parent, SLOT(removal (KTextEditor::Document*, KTextEditor::Range)));
    QObject::connect(changesView, SIGNAL(activated(QModelIndex)),
            parent, SLOT(jump(QModelIndex)));
}

void ApplyChangesWidget::change (KTextEditor::Document *document, const KTextEditor::Range &,
                const KTextEditor::Range &newRange)
{
    d->addItem(d->m_changes[d->m_index], document, newRange, i18n("Change"));
}

void ApplyChangesWidget::insertion(KTextEditor::Document *document, const KTextEditor::Range &range)
{
    d->addItem(d->m_changes[d->m_index], document, range, i18n("Insert"));
}

void ApplyChangesWidget::removal(KTextEditor::Document *document, const KTextEditor::Range &range)
{
    d->addItem(d->m_changes[d->m_index], document, range, i18n("Remove"));
}

void ApplyChangesWidget::indexChanged(int newIndex)
{
    Q_ASSERT(newIndex != -1);
    d->m_index = newIndex;
}

}

#include "applychangeswidget.moc"
