#define _CRT_SECURE_NO_DEPRECATE 1

#include "SkyfsInterface.h"
#include "Inode.h"
#include "Volume.h"
#include "BPlusTree.h"

/**	Opens a directory ready to be traversed.
 *	skyfs_open_dir() is also used by bfs_open_index_dir().
 */


//static 
status_t skyfs_open_dir(void *_ns, void *_node, void **_cookie)
{
	//FUNCTION();
	
	if (_ns == NULL || _node == NULL || _cookie == NULL)
		RETURN_ERROR(B_BAD_VALUE);
	
	Inode *inode = (Inode *)_node;
	
	
	status_t status = inode->CheckPermissions(R_OK);
	
	if (status < B_OK)
		RETURN_ERROR(status);
	
	// we don't ask here for directories only, because the bfs_open_index_dir()
	// function utilizes us (so we must be able to open indices as well)
	if (!inode->IsContainer()){
		printf("inode->IsContainer()=%i\n",inode->IsContainer());
		RETURN_ERROR(B_BAD_VALUE);
	}
	BPlusTree *tree;
	if (inode->GetTree(&tree) != B_OK)
		RETURN_ERROR(B_BAD_VALUE);
	
	//dump_bplustree_header(tree->Header());
	TreeIterator *iterator = new TreeIterator(tree);
	
	if (iterator == NULL)
		RETURN_ERROR(B_NO_MEMORY);
	
	*_cookie = iterator;
	return B_OK;
}

//static 
status_t
skyfs_read_dir(void *_ns, void *_node, void *_cookie, struct dirent *Dirent, 
	size_t bufferSize, uint32 *_num)
{
	//FUNCTION();
	TreeIterator *iterator = (TreeIterator *)_cookie;
	if (iterator == NULL)
		RETURN_ERROR(B_BAD_VALUE);
	//iterator->Dump();
	uint16 length;
	vnode_id id;
	status_t status = iterator->GetNextEntry(Dirent->d_name, &length, bufferSize, &id);
	
	if (status == B_ENTRY_NOT_FOUND) { // should be status=2
		*_num = 0;
		return B_OK;
	} else if (status != B_OK)
		RETURN_ERROR(status);
	
	
	Volume *volume = (Volume *)_ns;
	
	Dirent->d_dev = volume->ID();
	Dirent->d_ino = id;

	Dirent->d_reclen = sizeof(struct dirent) + length;
	
	*_num = 1;
	
	return B_OK;
}

status_t
skyfs_rewind_dir(void * ns, void * node, void *_cookie)
{
	FUNCTION();
	TreeIterator *iterator = (TreeIterator *)_cookie;

	if (iterator == NULL)
		RETURN_ERROR(B_BAD_VALUE);
	
	return iterator->Rewind();
}


/**	Opens the file with the specified mode.
 */

//static 
status_t
skyfs_open(void *_fs, void *_node, int openMode, void **_cookie)
{
	FUNCTION();

	Volume *volume = (Volume *)_fs;
	Inode *inode = (Inode *)_node;
#ifdef DEBUG
	dump_inode(&(inode->Node()));
#endif
	// opening a directory read-only is allowed, although you can't read
	// any data from it.
	if (inode->IsDirectory() && openMode & O_RWMASK) {
		openMode = openMode & ~O_RWMASK;
		// ToDo: for compatibility reasons, we don't return an error here...
		// e.g. "copyattr" tries to do that
		//return B_IS_A_DIRECTORY;
	}

	status_t status = inode->CheckPermissions(openModeToAccess(openMode)
		| (openMode & O_TRUNC ? W_OK : 0));
	if (status < B_OK)
		RETURN_ERROR(status);

	// we could actually use the cookie to keep track of:
	//	- the last block_run
	//	- the location in the data_stream (indirect, double indirect,
	//	  position in block_run array)
	//
	// This could greatly speed up continuous reads of big files, especially
	// in the indirect block section.

	file_cookie *cookie = (file_cookie *)malloc(sizeof(file_cookie));
	if (cookie == NULL)
		RETURN_ERROR(B_NO_MEMORY); 

	// initialize the cookie
	cookie->open_mode = openMode;
		// needed by e.g. bfs_write() for O_APPEND
	cookie->last_size = inode->Size();
	
	//SYSTEMTIME st;
	FILETIME ft;
	//GetSystemTime(&st);              // gets current time
    //SystemTimeToFileTime(&st, &ft);  // converts to file time format
	//cookie->last_notification = system_time();
	GetSystemTimeAsFileTime(&ft);
	LARGE_INTEGER li;
	li.LowPart=ft.dwLowDateTime;
	li.HighPart=ft.dwHighDateTime;
	printf("file last_notification=%I64d\n",li.QuadPart);
	cookie->last_notification = li.QuadPart;

	// Should we truncate the file?
	if (openMode & O_TRUNC) {
		WriteLocked locked(inode->Lock());
		Transaction transaction(volume, inode->BlockNumber());

		status_t status = inode->SetFileSize(transaction, 0);
		if (status >= B_OK)
			status = inode->WriteBack(transaction);

		if (status < B_OK) {
			// bfs_free_cookie() is only called if this function is successful
			free(cookie);
			return status;
		}

		transaction.Done();
	}

	*_cookie = cookie;
	return B_OK;
}

/**	Read a file specified by node, using information in cookie
 *	and at offset specified by pos. read len bytes into buffer buf.
 */

//static 
status_t
skyfs_read(void *_ns, void *_node, void *_cookie, off_t pos, void *buffer, off_t *_length)
{
	// not used: _ns, _cookie
	//FUNCTION();
	Inode *inode = (Inode *)_node;

	if (!inode->HasUserAccessableStream()) {
		*_length = 0;
		RETURN_ERROR(B_BAD_VALUE);
	}

	ReadLocked locked(inode->Lock());
	return inode->ReadAt(pos, (uint8 *)buffer, _length);
}

status_t
skyfs_close_dir(void * /*ns*/, void * /*node*/, void * /*_cookie*/)
{
	FUNCTION();
	// Do whatever you need to to close a directory, but DON'T free the cookie!
	return B_OK;
}

/**	Do whatever is necessary to close a file, EXCEPT for freeing
 *	the cookie!
 */

status_t
skyfs_close(void *_ns, void *_node, void *_cookie)
{
	FUNCTION();
	if (_ns == NULL || _node == NULL || _cookie == NULL)
		return B_BAD_VALUE;

	return B_OK;
}

/**	Reads in the node from disk and creates an inode object from it.
 */

status_t skyfs_read_vnode(void *_ns, vnode_id id, void **_node, bool reenter)
{
	//FUNCTION_START(("vnode_id = %Ld\n", id));
	Volume *volume = (Volume *)_ns;

	// first inode may be after the log area, we don't go through
	// the hassle and try to load an earlier block from disk
	if (id < volume->ToBlock(volume->Log()) + volume->Log().Length()
		|| id > volume->NumBlocks()) {
			printf("error: inode at %Ld requested!\n", id);
		return B_ERROR;
	}

	CachedBlock cached(volume, id);
	bfs_inode *node = (bfs_inode *)cached.Block();
	if (node == NULL) {
		FATAL(("could not read inode: %Ld\n", id));
		return B_IO_ERROR;
	}

	status_t status = node->InitCheck(volume);
	if (status < B_OK) {
		FATAL(("inode at %Ld is corrupt!\n", id));
		return status;
	}

	Inode *inode = new Inode(volume, id);
	if (inode == NULL)
		return B_NO_MEMORY;

	status = inode->InitCheck(false);
	if (status < B_OK)
		delete inode;

	if (status == B_OK)
		*_node = inode;

	return status;
}

/**	the walk function just "walks" through a directory looking for the
 *	specified file. It calls get_vnode() on its vnode-id to init it
 *	for the kernel.
 */

status_t skyfs_lookup(void *_ns, void *_directory, const char *file, vnode_id *_vnodeID, int *_type)
{
	FUNCTION_START(("file = %s\n", file));
	if (_ns == NULL || _directory == NULL || file == NULL || _vnodeID == NULL)
		return B_BAD_VALUE;

	Volume *volume = (Volume *)_ns;
	Inode *directory = (Inode *)_directory;

	// check access permissions
	status_t status = directory->CheckPermissions(X_OK);
	if (status < B_OK)
		RETURN_ERROR(status);

	BPlusTree *tree;
	if (directory->GetTree(&tree) != B_OK)
		RETURN_ERROR(B_BAD_VALUE);

	if ((status = tree->Find((uint8 *)file, (uint16)strlen(file), _vnodeID)) < B_OK) {
		PRINT(("skyfs_walk() could not find %I64d:\"%s\": %s\n", directory->BlockNumber(), file, strerror(status)));
		return status;
	}

	RecursiveLocker locker(volume->Lock());
		// we have to hold the volume lock in order to not
		// interfere with new_vnode() here

	Inode *inode;
	if ((status = get_vnode(volume->ID(), *_vnodeID, (void **)&inode)) != B_OK) {
		REPORT_ERROR(status);
		return B_ENTRY_NOT_FOUND;
	}

	*_type = inode->Mode();

	return B_OK;
}

status_t skyfs_get_vnode_name(void* _fs, fs_vnode _node, char *buffer, size_t bufferSize)
{
	Inode *inode = (Inode *)_node;

	return inode->GetName(buffer, bufferSize);
}

//	Attribute functions


status_t skyfs_open_attr_dir(void *_ns, void *_node, void **_cookie)
{
	Inode *inode = (Inode *)_node;

	//FUNCTION();

	AttributeIterator *iterator = new AttributeIterator(inode);
	if (iterator == NULL)
		RETURN_ERROR(B_NO_MEMORY);

	*_cookie = iterator;
	return B_OK;
}

status_t skyfs_close_attr_dir(void *ns, void *node, void *cookie)
{
	//FUNCTION();
	return B_OK;
}

status_t skyfs_rewind_attr_dir(void *_ns, void *_node, void *_cookie)
{
	//FUNCTION();
	
	AttributeIterator *iterator = (AttributeIterator *)_cookie;
	if (iterator == NULL)
		RETURN_ERROR(B_BAD_VALUE);

	RETURN_ERROR(iterator->Rewind());
}

status_t skyfs_read_attr_dir(void *_ns, void *node, void *_cookie, struct dirent *dirent,
	size_t bufferSize, uint32 *_num)
{
	//FUNCTION();
	AttributeIterator *iterator = (AttributeIterator *)_cookie;

	if (iterator == NULL)
		RETURN_ERROR(B_BAD_VALUE);

	uint32 type;
	size_t length;
	status_t status = iterator->GetNext(dirent->d_name, &length, &type, &dirent->d_ino);
	if (status == B_ENTRY_NOT_FOUND) {
		*_num = 0;
		return B_OK;
	} else if (status != B_OK) {
		RETURN_ERROR(status);
	}

	Volume *volume = (Volume *)_ns;

	dirent->d_dev = volume->ID();
	dirent->d_reclen = sizeof(struct dirent) + length;

	*_num = 1;
	return B_OK;
}

status_t skyfs_open_attr(void* _fs, fs_vnode _node, const char *name, int openMode, void *_cookie)
{
	//FUNCTION();

	Inode *inode = (Inode *)_node;
	Attribute attribute(inode);

	return attribute.Open(name, openMode, (attr_cookie **)_cookie);
}

status_t skyfs_close_attr(void* _fs, fs_vnode _file, attr_cookie* cookie)
{
	return B_OK;
}

status_t skyfs_read_attr(void* _fs, fs_vnode _file, attr_cookie* _cookie, off_t pos,
	void *buffer, off_t *_length)
{
	//FUNCTION();

	attr_cookie *cookie = _cookie;
	Inode *inode = (Inode *)_file;

	Attribute attribute(inode, cookie);

	return attribute.Read(cookie, pos, (uint8 *)buffer, _length);
}

status_t skyfs_read_attr_stat(void* _fs, fs_vnode _file, attr_cookie* _cookie, struct attr_stat *stat)
{
	//FUNCTION();

	attr_cookie *cookie = (attr_cookie *)_cookie;
	Inode *inode = (Inode *)_file;

	Attribute attribute(inode, cookie);

	return attribute.Stat(*stat);
}

