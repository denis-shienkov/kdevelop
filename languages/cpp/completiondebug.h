/***************************************************************************
   copyright            : (C) 2006 by David Nolden
   email                : david.nolden.kdevelop@art-master.de
***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef __COMPLETIONDEBUG_H__
#define __COMPLETIONDEBUG_H__

//#define VERBOSE
#define VERBOSEMAJOR
//#define DEPTHBACKTRACE
#define NOFULLVERBOSE

#include <qstringlist.h>
#include <kdebug.h>

namespace CompletionDebug {
template <class StreamType>
  class KDDebugState {
  private:
  StreamType m_stream;
  QStringList m_prefixStack;
	int m_counter;
	int m_depth;
	bool m_enabled;
	  
  public:
  typedef StreamType KStreamType;
  KDDebugState();
 
  KDDebugState( StreamType stream ) : m_stream( stream ), m_counter(0), m_depth(0), m_enabled(true) {
  }
 
  void push( const QString & txt ) {
      m_prefixStack.push_back( txt );
	  pushDepth();
  }
 
  void pop() {
      m_prefixStack.pop_back();
	  pushDepth();
  };

  inline void pushDepth() {
  	m_depth++;
  }
	
  inline void popDepth() {
  	m_depth--;
  }
 
  StreamType& dbg() {
    if( !m_enabled ) kndbgstream();
    m_stream << "(" << m_counter << ")";
    for( QStringList::iterator it = m_prefixStack.begin(); it != m_prefixStack.end() ; ++it )
      m_stream << *it;

	  m_counter++;
	  return m_stream;
  }

	void setState( bool enabled ) {
		m_enabled = enabled;
	}

	  bool state() {
		  return m_enabled;
	  }
	  
#ifndef NDEBUG
  void outputPrefix( kdbgstream& target ) {
    target << "(" << m_counter << ")";
    for( QStringList::iterator it = m_prefixStack.begin(); it != m_prefixStack.end() ; ++it )
      target << *it;
    
    m_counter++;
  }
#endif
    
  void clearCounter() {
      m_counter = 0;
  }
	  
  int depth() {
    return m_prefixStack.size();
  }
};
#ifndef NDEBUG 
template<>
  KDDebugState<kdbgstream>::KDDebugState();
#endif
template<>
  KDDebugState<kndbgstream>::KDDebugState();
#if defined(VERBOSE) && !defined(NDEBUG) && !defined(NOFULLVERBOSE)
 typedef KDDebugState<kdbgstream> DBGStreamType;
#else
 typedef KDDebugState<kndbgstream> DBGStreamType;
#endif
 ///Class to help indent the debug-output correctly
 extern DBGStreamType dbgState;
 extern const int completionMaxDepth;
 class LogDebug {
 private:
   DBGStreamType& m_state;
   int m_max;
 public:
 LogDebug( const QString& prefix = "#", int max = completionMaxDepth, DBGStreamType& st = dbgState ) : m_state( st ), m_max( max ) {
     m_state.push( prefix );
   };
   ~LogDebug() {
     m_state.pop();
   }
 
   DBGStreamType::KStreamType& dbg() {
     return m_state.dbg();
   }
 
   int depth() {
     return m_state.depth();
   }
 
   operator bool() {
     bool r = depth() < m_max;
  
	if( !r ) {
      dbg() << "recursion is too deep";
#ifdef DEPTHBACKTRACE
	   kdDebug( 9007 ) << kdBacktrace();
#endif
	}
  
     return r;
   }
 };


///does not care about the logging, but still counts the depth
  class DepthDebug {
    DBGStreamType& m_state;
	int m_max;
  public:
  	DepthDebug( const QString& prefix = "#", int max = completionMaxDepth, DBGStreamType& st = dbgState ) : m_state( st ), m_max( max ) {
      Q_UNUSED( prefix );
	  	dbgState.pushDepth();
    };
	  ~DepthDebug() {
		dbgState.popDepth();
	  }
    
    kndbgstream dbg() {
      return kndDebug();
    }
  
    int depth() {
      return m_state.depth();
    }
    
    operator bool() {
      bool r = depth() < m_max;
      
	if( !r ) {
        kdDebug( 9007 ) << "recursion is too deep";
#ifdef DEPTHBACKTRACE
		kdDebug( 9007 ) << kdBacktrace();
#endif
	}
      
      return r;
    }
  };

#ifndef VERBOSEMAJOR
#define ifVerboseMajor(x) /**/
#else
#define ifVerboseMajor(x) x
#endif


#ifdef VERBOSE
 typedef LogDebug Debug;
 DBGStreamType::KStreamType& dbg();
#define ifVerbose(x) x
#else
 DBGStreamType::KStreamType& dbg();
 typedef DepthDebug Debug;
#define ifVerbose(x) /**/
#endif
#ifndef NDEBUG
kdbgstream dbgMajor();
#else
kndbgstream dbgMajor();
#endif
}
#endif 
// kate: indent-mode csands; tab-width 4;
