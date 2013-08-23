/*
 * Copyright 2007, Ingo Weinhold, bonefish@cs.tu-berlin.de.
 * Distributed under the terms of the MIT License.
 */

#ifndef _SYSTEM_DEPENDENCIES_H
#define _SYSTEM_DEPENDENCIES_H
#define _CRT_SECURE_NO_DEPRECATE 1
//#define DEBUG 1

#include <ctype.h>
#include <errno.h>
//#include <null.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
//#include <unistd.h>

//#include <ByteOrder.h>

#ifndef _BOOT_MODE
//#include <fs_attr.h>
//#include <fs_cache.h>
//#include <fs_index.h>
//#include <fs_info.h>
//#include <fs_interface.h>
//#include <fs_query.h>
//#include <fs_volume.h>
//#include <Drivers.h>
//#include <KernelExport.h>
//#include <NodeMonitor.h>
#include "SupportDefs.h"
//#include <TypeConstants.h>
#endif	// _BOOT_MODE

#include "DoublyLinkedList.h"
//#include <util/kernel_cpp.h>
#include "Stack.h"
#include "Cache.h"

//#endif	// !BFS_SHELL


#endif	// _SYSTEM_DEPENDENCIES_H
