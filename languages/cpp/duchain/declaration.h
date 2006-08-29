/* This file is part of KDevelop
    Copyright (C) 2006 Hamish Rodda <rodda@kde.org>

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

#ifndef DECLARATION_H
#define DECLARATION_H

#include <QList>

#include "identifier.h"
#include "kdevdocumentrangeobject.h"
#include "cppnamespace.h"
#include "typesystem.h"

class AbstractType;
class DUContext;
class Use;
class Definition;

/**
 * Represents a single declaration in a definition-use chain.
 */
class Declaration : public KDevDocumentRangeObject
{
public:
  enum Scope {
    GlobalScope,
    NamespaceScope,
    ClassScope,
    FunctionScope,
    LocalScope
  };

  Declaration(KTextEditor::Range* range, Scope scope);
  virtual ~Declaration();

  bool isDefinition() const;
  void setDeclarationIsDefinition(bool dd);

  Definition* definition() const;
  void setDefinition(Definition* definition);

  DUContext* context() const;
  void setContext(DUContext* context);

  Scope scope() const;

  template <class T>
  KSharedPtr<T> type() const { return KSharedPtr<T>::dynamicCast(abstractType()); }

  template <class T>
  void setType(KSharedPtr<T> type) { setAbstractType(AbstractType::Ptr::staticCast(type)); }

  AbstractType::Ptr abstractType() const;
  void setAbstractType(AbstractType::Ptr type);

  void setIdentifier(const Identifier& identifier);
  const Identifier& identifier() const;

  QualifiedIdentifier qualifiedIdentifier() const;

  const QList<Use*>& uses() const;
  void addUse(Use* range);
  void removeUse(Use* range);

  bool operator==(const Declaration& other) const;

  virtual QString toString() const;

private:
  DUContext* m_context;
  Scope m_scope;
  AbstractType::Ptr m_type;
  Identifier m_identifier;

  QList<Use*> m_uses;
};

#endif // DECLARATION_H

// kate: indent-width 2;
