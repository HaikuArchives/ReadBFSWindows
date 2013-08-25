#ifndef SYSTEM_H
#define SYSTEM_H

#include "SupportDefs.h"

#include <stdio.h>
//thread stuff?
#include <conio.h>

#include "native.h"
//#include <windows.h>

#pragma warning( disable: 4018 )
#pragma warning( disable: 4101 )
#pragma warning( disable: 4103 )
#pragma warning( disable: 4200 )
#pragma warning( disable: 4244 )
#pragma warning( disable: 4267 )
#pragma warning( disable: 4311 )
#pragma warning( disable: 4355 )
#pragma warning( disable: 4390 )
#pragma warning( disable: 4800 )
#pragma warning( disable: 4005 )
#pragma warning( disable: 4103 )
#pragma warning( disable: 4200 )
#pragma warning( disable: 4309 )
#pragma warning( disable: 4311 )



int read_pos(HANDLE fDevice, int64_ sStart, void* data, unsigned int size);
int read_pos2(HANDLE fDevice, int64_ sStart, void* data, unsigned int size);
int write_pos(HANDLE fDevice, int64_ sStart, void* data, unsigned int size);
status_t acquire_sem(sem_id sem);
status_t acquire_sem_etc(sem_id sem,uint32 count,uint32 flags,bigtime_t timeout);
sem_id create_sem(uint32 thread_count, const char *name);
status_t delete_sem(sem_id sem);
status_t release_sem(sem_id sem);
status_t release_sem_etc(sem_id sem, int32 count, uint32 flags);



status_t resume_thread(thread_id id);
ssize_t	readv_pos(HANDLE fd, off_t pos, const struct iovec *vec, size_t count);
gid_t getegid(void);
uid_t geteuid(void);
int32 	atomic_add (vint32 *value, int32 addValue);
int32 	atomic_set (vint32 *value, int32 newValue);
status_t get_vnode(int fs, vnode_id id, void **_vnode);
status_t publish_vnode (unsigned int mountID, off_t vnodeID, void* privateNode);
status_t new_vnode (unsigned int mountID, off_t vnodeID, void* privateNode);
status_t remove_vnode (unsigned int mountID, off_t vnodeID);
status_t unremove_vnode (unsigned int mountID, off_t vnodeID);
status_t put_vnode (unsigned int mountID, off_t vnodeID);
status_t cache_next_block_in_transaction(void* a, int,unsigned int* b, off_t * c, int d, int e);
void notify_query_entry_created(int a, int b, unsigned int c,off_t d, const char* e, off_t f);
void notify_query_entry_removed(int a, int b, unsigned int c,off_t d, const char* e, off_t f);
ssize_t	writev_pos(HANDLE fd, off_t pos, const struct iovec *vec,size_t count);
void cache_abort_sub_transaction(void* d, int id);
void cache_abort_transaction(void* d, int id);
int32 cache_blocks_in_transaction(void* d, int id);
typedef void (*tFoo)(int f,void*ff);
void cache_end_transaction(void* d, int id, tFoo f, void*);
int cache_start_sub_transaction(void* d, int id);
int cache_start_transaction(void* d);

#endif // SYSTEM_H