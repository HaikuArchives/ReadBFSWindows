#include "System.h"
#include <stdio.h>


int read_pos(HANDLE fDevice, int64_ sStart, void* data, unsigned int size){
	char * q = (char *)calloc(size,sizeof(char));
	DWORD dwBytes;
	LARGE_INTEGER sbStart;
	sbStart.QuadPart = (LONGLONG)sStart;
	//printf("sStart=%I64d, sizeof(data)=%i\n",sStart,size);
	DWORD dwCur = SetFilePointer(fDevice,sbStart.LowPart,&sbStart.HighPart,FILE_BEGIN);
	//int nRet = ReadFile(fDevice,q,size,&dwBytes,NULL);
	int nRet;
	NTSTATUS Status;
	IO_STATUS_BLOCK Iosb;
	//printf("_____________________________NtReadFile_____________________________\n");
	Status = NtReadFile(fDevice,	// file Handle
			0,		// event Handle
			NULL,	// APC entry point
			NULL,	// APC context
			&Iosb,	// IOSB address
			q,//Message,// ptr to data buffer
			size,	// length
			//0,		// byte offset
			&sbStart,		// byte offset
			NULL);	// key
	nRet = 1;
	if(!NT_SUCCESS(Status) )
	{
		printf("NtReadFile request failed 0x%0x\n", Status);
		//exit(0);
		nRet = 0;
	}

    //
    // Check the returned status from the WRITE.  
    //
    if(!NT_SUCCESS(Iosb.Status) )
	{
		printf("READ failed with status = 0x%0x\n",Iosb.Status);
		//exit(0);
		nRet = 0;
	}
	
	if(!nRet){
		printf("read_pos: error %i,%i\n",nRet,GetLastError());
		free (q);
		return nRet;
	}
	else {
		//printf("!!!!!!!!!!!!!!!!!  read_pos: success !!!!!!!!!!!!!!!!!\n");
		//printf("dwBytes=%d\n",dwBytes);
		if (/*size==1024*/false){
			for (unsigned int j=0;j<size;j++){
				if ((char)q[j]!=0) 
					printf("%.2x",(char)q[j]);
				else printf("00");
			}
		//	printf("\nend_read_pos\n");
		}
		memcpy(data,q,size);
	}
	free (q);
	return size;
}
int read_pos2(HANDLE fDevice, int64_ sStart, void* data, unsigned int size){
	char * q = (char *)calloc(size,sizeof(char));
	DWORD dwBytes;
	LARGE_INTEGER sbStart;
	sbStart.QuadPart = (LONGLONG)sStart;
	//printf("sStart=%I64d, sizeof(data)=%i\n",sStart,size);
	DWORD dwCur = SetFilePointer(fDevice,sbStart.LowPart,&sbStart.HighPart,FILE_BEGIN);
	//int nRet = ReadFile(fDevice,q,size,&dwBytes,NULL);
	int nRet;
	NTSTATUS Status;
	IO_STATUS_BLOCK Iosb;
	//printf("_____________________________NtReadFile_____________________________\n");
	Status = NtReadFile(fDevice,	// file Handle
			0,		// event Handle
			NULL,	// APC entry point
			NULL,	// APC context
			&Iosb,	// IOSB address
			q,//Message,// ptr to data buffer
			size,	// length
			//0,		// byte offset
			&sbStart,		// byte offset
			NULL);	// key
	nRet = 1;
	if(!NT_SUCCESS(Status) )
	{
		printf("NtReadFile request failed 0x%0x\n", Status);
		//exit(0);
		nRet = 0;
	}

    //
    // Check the returned status from the WRITE.  
    //
    if(!NT_SUCCESS(Iosb.Status) )
	{
		printf("READ failed with status = 0x%0x\n",Iosb.Status);
		//exit(0);
		nRet = 0;
	}
	
	if(!nRet){
		printf("read_pos: error %i,%i\n",nRet,GetLastError());
		free (q);
		return nRet;
	}
	else {
		//printf("!!!!!!!!!!!!!!!!!  read_pos: success !!!!!!!!!!!!!!!!!\n");
		//printf("dwBytes=%d\n",dwBytes);
		if (/*size==1024*/true){
			for (unsigned int j=0;j<size;j++){
				if ((char)q[j]!=0) 
					printf("%.2x",(char)q[j]);
				else printf("00");
			}
		//	printf("\nend_read_pos\n");
		}
		memcpy(data,q,size);
	}
	free (q);
	return size;
}

ssize_t	readv_pos(HANDLE fDevice, off_t pos, const struct iovec *vec,
				  size_t count){
	printf("readv_pos: pos=%I64d, count=%i\n",pos,count);
// assumption: read count iovecs into vec[] ?
	
	DWORD dwBytes;
	LARGE_INTEGER sbStart,byteOffset;
	size_t offset=0;
	sbStart.QuadPart = (LONGLONG)pos;
	byteOffset.QuadPart = (LONGLONG)pos;
	//printf("sStart=%I64d, sizeof(data)=%i\n",sStart,size);
	//DWORD dwCur = SetFilePointer(fDevice,sbStart.LowPart,&sbStart.HighPart,FILE_BEGIN);
	//int nRet = ReadFile(fDevice,q,size,&dwBytes,NULL);
	int nRet=1;
	
	//printf("_____________________________NtReadFile_____________________________\n");
	for (int i=0; i<count;i++){
		NTSTATUS Status;
		IO_STATUS_BLOCK Iosb;
		printf("reading iovec[%i] (length=%Lu)...",i,vec[i].iov_len);
		byteOffset.QuadPart = sbStart.QuadPart + offset;
		Status = NtReadFile(fDevice,	// file Handle
			0,		// event Handle
			NULL,	// APC entry point
			NULL,	// APC context
			&Iosb,	// IOSB address
			vec[i].iov_base,//Message,// ptr to data buffer
			vec[i].iov_len,	// length
			//0,		// byte offset
			//&sbStart+offset,		// byte offset
			&byteOffset,
			NULL);	// key
		printf(" done reading from offset: %I64d\n",byteOffset.QuadPart);
		offset+=vec[i].iov_len;
		if(!NT_SUCCESS(Status) )
		{
			printf("NtReadFileV request failed 0x%0x\n", Status);
// getting this one for some reason...
//#define STATUS_ACCESS_VIOLATION          ((NTSTATUS)0xC0000005L)    // winnt
			//exit(0);
			nRet = 0;
		}

		//
		// Check the returned status from the WRITE.  
		//
		if(!NT_SUCCESS(Iosb.Status) )
		{
			printf("READV failed with status = 0x%0x\n",Iosb.Status);
			//exit(0);
			nRet = 0;
		}
		
		if(!nRet){
			printf("readv_pos: error %i,%i\n",nRet,GetLastError());
			return nRet;
		}
		else {
			//printf("!!!!!!!!!!!!!!!!!  read_pos: success !!!!!!!!!!!!!!!!!\n");
			//printf("dwBytes=%d\n",dwBytes);
			if (/*size==1024*/true){
				for (unsigned int j=0;j<vec[i].iov_len;j++){
					//if ((char)vec[i].iov_base[j]!=0) 
						//printf("%.2x",(char)vec[i].iov_base[j]);
					//	printf("%c",(char)vec[i].iov_base[j]);
					//else printf("00");
				}
				//printf("################################################################\n");
			//	printf("\nend_read_pos\n");
			}
			//memcpy(vec[i],q,size);
		}
	}
	//TODO: this might be incorrect
	return offset;
}

int write_pos(HANDLE fDevice, int64_ sStart, void* data, unsigned int size){
	printf("write_pos not implemented\n");
	return 0;
}

status_t acquire_sem(sem_id sem){
	//printf("acquire_sem not implemented\n");
	return 0;
}
status_t acquire_sem_etc(sem_id sem,uint32 count,uint32 flags,bigtime_t timeout){
	printf("acquire_sem_etc not implemented\n");
	return 0;
}
sem_id create_sem(uint32 thread_count, const char *name){
	//printf("create_sem not implemented\n");
	return 0;
}
status_t delete_sem(sem_id sem){
	printf("delete_sem not implemented\n");
	return 0;
}
status_t release_sem(sem_id sem){
	//printf("release_sem not implemented\n");
	return 0;
}
status_t release_sem_etc(sem_id sem, int32 count, uint32 flags){
	printf("release_sem_etc not implemented\n");
	return 0;
}




gid_t getegid(void){
	//printf("getegid not implemented\n");
	return 0;
}
uid_t geteuid(void){
	//printf("geteuid not implemented\n");
	return 0;
}


// TODO: correct?
int32 	atomic_add (vint32 *value, int32 addValue){
	*value+=addValue;
	return *value;
}
int32 	atomic_set (vint32 *value, int32 newValue){
	*value=newValue;
	return *value;
}

//  http://factory.haiku-os.org/documentation/Haiku_Book_doxygen/html/fs__interface_8h.html#02a4d3e3e95cdb05bf3a3da6dc2ac80a
status_t get_vnode(int fs, vnode_id id, void **_vnode){
	//Creates the private data handle to be associated with the node referred to by id. 
	printf("get_vnode not implemented\n");
	return B_ERROR;
}
status_t publish_vnode (unsigned int mountID, off_t vnodeID, void* privateNode){
	printf("publish_vnode not implemented, continuing...\n");
	return 0;
}

/*publish_vnode(mount_id nsid, vnode_id vnid,
	fs_vnode data)
{
	// get the request port and the file system
	RequestPort* port;
	FileSystem* fileSystem;
	status_t error = get_port_and_fs(&port, &fileSystem);
	if (error != B_OK)
		return error;

	// prepare the request
	RequestAllocator allocator(port->GetPort());
	PublishVNodeRequest* request;
	error = AllocateRequest(allocator, &request);
	if (error != B_OK)
		return error;

	request->nsid = nsid;
	request->vnid = vnid;
	request->node = data;

	// send the request
	UserlandRequestHandler handler(fileSystem, PUBLISH_VNODE_REPLY);
	PublishVNodeReply* reply;
	error = port->SendRequest(&allocator, &handler, (Request**)&reply);
	if (error != B_OK)
		return error;
	RequestReleaser requestReleaser(port, reply);

	// process the reply
	if (reply->error != B_OK)
		return reply->error;
	return error;
}*/

status_t new_vnode (unsigned int mountID, off_t vnodeID, void* privateNode) {
	printf("new_vnode not implemented\n");
	return -1;
}
status_t remove_vnode (unsigned int mountID, off_t vnodeID){
	printf("remove_vnode not implemented\n");
	return -1;
}
status_t unremove_vnode (unsigned int mountID, off_t vnodeID){
	printf("unremove_vnode not implemented\n");
	return -1;
}
status_t put_vnode (unsigned int mountID, off_t vnodeID){
	printf("put_vnode not implemented\n");
	return -1;
}

 	
status_t resume_thread(thread_id id){ 
	printf("resume_thread not implemented\n");
	return 0;
}


status_t cache_next_block_in_transaction(void* a, int,unsigned int* b, off_t * c, int d, int e){
	printf("cache_next_block_in_transaction not implemented\n");
	return 0;
}
void notify_query_entry_created(int a, int b, unsigned int c,off_t d, const char* e, off_t f){ 
	printf("notify_query_entry_created not implemented\n");
}

void notify_query_entry_removed(int a, int b, unsigned int c,off_t d, const char* e, off_t f){ 
	printf("notify_query_entry_removed not implemented\n");
}
ssize_t	writev_pos(HANDLE fd, off_t pos, const struct iovec *vec,size_t count){
	printf("writev_pos not implemented\n");
	return 0;
}

void cache_abort_sub_transaction(void* d, int id){
	printf("cache_abort_sub_transaction not implemented\n");
}
void cache_abort_transaction(void* d, int id){
	printf("cache_abort_transaction not implemented\n");
}
int32 cache_blocks_in_transaction(void* d, int id){
	printf("cache_blocks_in_transaction not implemented\n");
	return 0;
}

void cache_end_transaction(void* d, int id, tFoo f, void*){
	printf("cache_end_transaction not implemented\n");
}
int cache_start_sub_transaction(void* d, int id){
	printf("cache_start_sub_transaction not implemented\n");
	return 0;
}
int cache_start_transaction(void* d){
	printf("cache_start_transaction not implemented\n");
	return 0;
}


/*
check haiku for:
thread_id, thread_func, thread_state, thread_info, thread_entry, kill_thread, suspend_thread and other threadstuff in fssh_api_wrapper
*/
//thread_id spawn_kernel_thread(thread_func t,char* threadName, int flags, void * parent);
//status_t resume_thread(thread_id id);
//thread_id find_thread(int);
/*typedef struct {
      thread_id thread;
      team_id team;
      char name[B_OS_NAME_LENGTH];
      thread_state state;
      sem_id sem;
      int32 priority;
      bigtime_t user_time;
      bigtime_t kernel_time;
      void *stack_base;
      void *stack_end;
} thread_info;*/
//typedef int32 (*thread_func)(void *data);
//thread_id spawn_kernel_thread(thread_entry func, const char *name,long priority, void *data);



