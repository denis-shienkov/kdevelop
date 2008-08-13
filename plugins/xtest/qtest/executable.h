/*
* KDevelop xUnit integration
* Copyright 2008 Manuel Breugelmans <mbr.nxi@gmail.com>
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

#ifndef VERITAS_QTEST_QTESTEXE_H
#define VERITAS_QTEST_QTESTEXE_H

#include <KUrl>

namespace QTest
{

/*! Wraps a QTest executable. Currently only used to retrieve
test commands (aka functions) for a given testcase. */
class Executable
{
public:
    Executable();
    virtual ~Executable();

    /*! Initialize the filesystem location of this QTest
        @note mandatory to set this. */
    virtual void setLocation(const KUrl& url);
    virtual KUrl location() const;

    /*! Fetch the test functions aka testcommand names.
        Executes this with the -functions flag.
        Return the resulting lines as a list.
        @note blocking! */
    virtual QStringList fetchFunctions();

    /*! Deduce a test name from exe name */
    virtual QString name() const;

private:
    KUrl m_location;
};

}

#endif // VERITAS_QTEST_QTESTEXE_H
