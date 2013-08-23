/* Debug - debug stuff
 *
 * Copyright 2001-2006, Axel Dörfler, axeld@pinc-software.de.
 * This file may be used under the terms of the MIT License.
 */
#ifndef DEBUG_H
#define DEBUG_H


#include "system_dependencies.h"

#ifdef USER
#	define __out printf
#else
#	define __out printf
#endif

// Which debugger should be used when?
// The DEBUGGER() macro actually has no effect if DEBUG is not defined,
// use the DIE() macro if you really want to die.
#ifdef DEBUG
#	ifdef USER
#		define DEBUGGER(x) debugger x
#	else
#		define DEBUGGER(x) printf x //kernel_debugger x
#	endif
#else
#	define DEBUGGER(x) ;
#endif

#ifdef USER
#	define DIE(x) debugger x
#else
#	define DIE(x) printf x//kernel_debugger x
#endif

// Short overview over the debug output macros:
//	PRINT()
//		is for general messages that very unlikely should appear in a release build
//	FATAL()
//		this is for fatal messages, when something has really gone wrong
//	INFORM()
//		general information, as disk size, etc.
//	REPORT_ERROR(status_t)
//		prints out error information
//	RETURN_ERROR(status_t)
//		calls REPORT_ERROR() and return the value
//	D()
//		the statements in D() are only included if DEBUG is defined

#ifdef DEBUG
	#define PRINT(x) { __out("skyfs: "); __out x; }
	#define REPORT_ERROR(status) \
		__out("skyfs: %s:%d: %s\n", __FUNCTION__, __LINE__, strerror(status));
	#define RETURN_ERROR(err) { status_t _status = err; if (_status < B_OK) REPORT_ERROR(_status); return _status;}
	#define FATAL(x) { __out("skyfs: "); __out x; }
	#define INFORM(x) { __out("skyfs: "); __out x; }
	#define FUNCTION() __out("skyfs: %s()\n",__FUNCTION__);
	#define FUNCTION_START(x) { __out("skyfs: %s() ",__FUNCTION__); __out x; }
	//#define FUNCTION() ;
//	#define FUNCTION_START(x) ;
	#define D(x) {x;};
	#define ASSERT(x) { if (!(x)) DEBUGGER(("skyfs: assert failed: " #x "\n")); }
#else
	#define PRINT(x) ;
	#define REPORT_ERROR(status) \
		__out("skyfs: %s:%d: %s\n", __FUNCTION__, __LINE__, strerror(status));
	#define RETURN_ERROR(err) { status_t _status = err; if (_status < B_OK) REPORT_ERROR(_status); return _status;}
//	#define FATAL(x) { __out("skyfs: "); __out x; sync(); panic("BFS!\n"); }
	#define FATAL(x) { __out("skyfs: "); __out x; }
	#define INFORM(x) { __out("skyfs: "); __out x; }
	#define FUNCTION() ;
	#define FUNCTION_START(x) ;
	#define D(x) ;
	#define ASSERT(x) ;
#endif

#ifdef DEBUG
	struct block_run;
	struct bplustree_header;
	struct bplustree_node;
	struct data_stream;
	struct bfs_inode;
	struct disk_super_block;
	class Inode;
	class Volume;
	
	// some structure dump functions
	void dump_block_run(const char *prefix, block_run &run);
	void dump_super_block(const disk_super_block *superBlock);
	void dump_data_stream(const data_stream *stream);
	void dump_inode(const bfs_inode *inode);
	void dump_bplustree_header(const bplustree_header *header);
	void dump_bplustree_node(const bplustree_node *node,
					const bplustree_header *header = NULL, Volume *volume = NULL);
	void dump_block(const char *buffer, int size);

	void remove_debugger_commands();
	void add_debugger_commands();
#endif

#endif	/* DEBUG_H */
