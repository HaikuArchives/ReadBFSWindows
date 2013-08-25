#include "Cache.h"
#include "AutoLocker.h"
#include "Inode.h"


// benaphores:
// http://www.irisa.fr/texmex/people/dufouil/navitex/doxy/beosthread_8c-source.html

static PagePool sPagePool;
static const uint32 kMaxAllocatedPages = 1024;
static volume_list sVolumeList;
static mutex sVolumeListLock;

static uint32
transaction_hash2(void *_transaction, const void *_id, uint32 range)
{
	cache_transaction *transaction = (cache_transaction *)_transaction;
	const int32 *id = (const int32 *)_id;

	if (transaction != NULL)
		return transaction->id % range;

	return (uint32)*id % range;
}

static int
transaction_compare(void *_transaction, const void *_id)
{
	cache_transaction *transaction = (cache_transaction *)_transaction;
	const int32 *id = (const int32 *)_id;

	return transaction->id - *id;
}

block_cache::block_cache(HANDLE _fd, off_t numBlocks, size_t blockSize,
	bool readOnly)
	:
	hash(NULL),
	fd(_fd),
	max_blocks(numBlocks),
	block_size(blockSize),
	allocated_block_count(0),
	next_transaction_id(1),
	last_transaction(NULL),
	transaction_hash(NULL),
	read_only(readOnly)
{
	printf("block_cache::block_cache: semaphore stuff not implemented\n");
	hash = hash_init(32, 0, &cached_block::Compare, &cached_block::Hash);
	if (hash == NULL)
		return;

	transaction_hash = hash_init(16, 0, &transaction_compare,
		&transaction_hash2);
	if (transaction_hash == NULL)
		return;
/* //TODO: implement this:
	if (benaphore_init(&lock, "block cache") < B_OK)
		return;*/
}

static void
add_to_iovec(iovec *vecs, int32 &index, int32 max, const void *address, size_t size)
{
	
	if (index > 0
		&& (addr_t)vecs[index - 1].iov_base + vecs[index - 1].iov_len == (addr_t)address) {
		// the iovec can be combined with the previous one
		vecs[index - 1].iov_len += size;
		return;
	}

	if (index == max)
		panic("no more space for iovecs!");

	// we need to start a new iovec
// TODO: how to fix next line????
	vecs[index].iov_base = /*const_cast<void *>*/(caddr_t)(address);
	vecs[index].iov_len = size;
	index++;
}

void
block_cache::Free(void *address)
{
	if (address == NULL)
		return;

	free(address);
}


void *
block_cache::Allocate()
{
	return malloc(block_size);
}


block_cache::~block_cache()
{
	printf("block_cache::block_cache destructor not implemented\n");
	//benaphore_destroy(&lock);

	hash_uninit(transaction_hash);
	hash_uninit(hash);
}
status_t
block_cache::InitCheck()
{
	printf("block_cache::InitCheck partially implemented\n");
	//if (lock.sem < B_OK)
	//	return lock.sem;

	if (hash == NULL || transaction_hash == NULL)
		return B_NO_MEMORY;

	return B_OK;
}

/*! Allocates a new block for \a blockNumber, ready for use */
cached_block *
block_cache::NewBlock(off_t blockNumber)
{
	cached_block *block = new/*(nothrow)*/ cached_block;
	if (block == NULL) {
		// TODO: this was FATAL instead of printf
		printf("could not allocate block!\n");
		return NULL;
	}

	// if we hit the limit of blocks to cache¸ try to free one or more
	if (allocated_block_count >= kMaxBlockCount) {
		RemoveUnusedBlocks(LONG_MAX,
			allocated_block_count - kMaxBlockCount + 1);
	}

	block->current_data = Allocate();
	if (!block->current_data) {
		//TODO:
		//FATAL(("could not allocate block data!\n"));
		printf("could not allocate block data!\n");
		delete block;
		return NULL;
	}

	block->block_number = blockNumber;
	block->ref_count = 0;
	block->accessed = 0;
	block->transaction_next = NULL;
	block->transaction = block->previous_transaction = NULL;
	block->original_data = NULL;
	block->parent_data = NULL;
	block->is_dirty = false;
	block->unused = false;
#ifdef DEBUG_CHANGED
	block->compare = NULL;
#endif

	allocated_block_count++;

	return block;
}

void
block_cache::FreeBlock(cached_block *block)
{
	Free(block->current_data);
	block->current_data = NULL;

	if (block->original_data != NULL || block->parent_data != NULL) {
		panic("block_cache::FreeBlock(): %p, %p\n", block->original_data,
			block->parent_data);
	}

#ifdef DEBUG_CHANGED
	Free(block->compare);
#endif

	delete block;
}

void
block_cache::RemoveUnusedBlocks(int32 maxAccessed, int32 count)
{
	//TODO:
	//TRACE(("block_cache: remove up to %ld unused blocks\n", count));
	printf("block_cache: remove up to %ld unused blocks\n", count);
	cached_block *block; 
	for (block_list::Iterator it = unused_blocks.GetIterator();
		 block = it.Next();) {

		if (maxAccessed < block->accessed)
			continue;
		//TODO:
		//TRACE(("  remove block %Ld, accessed %ld times\n",
		//	block->block_number, block->accessed));
		printf("  remove block %Ld, accessed %ld times\n",
			block->block_number, block->accessed);

		// this can only happen if no transactions are used
		//TODO: fix this:
		printf("Cache.cpp: write_cached_block not implemented\n");
		//if (block->is_dirty)
		//	write_cached_block(this, block, false);

		// remove block from lists
		it.Remove();
		hash_remove(hash, block);

		FreeBlock(block);

		if (--count <= 0)
			break;
	}
}




/* block cache */
/* extern */ void		block_cache_delete(void *_cache, bool allowWrites){
	printf("block_cache_delete not implemented\n");
};
/*void *		block_cache_create(HANDLE fd, off_t numBlocks, size_t blockSize, bool readOnly){
	printf("block_cache_create not implemented\n");
	return 0;
};*/
void *
block_cache_create(HANDLE fd, off_t numBlocks, size_t blockSize, bool readOnly)
{
	//block_cache *cache = new(nothrow) block_cache(fd, numBlocks, blockSize,readOnly);
	block_cache *cache = new block_cache(fd, numBlocks, blockSize,readOnly);
	if (cache == NULL)
		return NULL;

	if (cache->InitCheck() != B_OK) {
		delete cache;
		return NULL;
	}

	return cache;
}
status_t	block_cache_sync(void *_cache){
	printf("block_cache_sync not implemented\n");
	return 0;
};
status_t	block_cache_sync_etc(void *_cache, off_t blockNumber, size_t numBlocks){
	printf("block_cache_sync_etc not implemented\n");
	return 0;
};
/* extern */ status_t	block_cache_make_writable(void *_cache, off_t blockNumber, int32_t transaction){
	printf("block_cache_make_writable not implemented\n");
	return 0;
};
/* extern */ void *		block_cache_get_writable_etc(void *_cache,off_t blockNumber, off_t base,off_t length, int32_t transaction){
	printf("block_cache_get_writable_etc not implemented\n");
	return 0;
};
/* extern */ void *		block_cache_get_writable(void *_cache,off_t blockNumber, int32_t transaction){
	printf("block_cache_get_writable not implemented\n");
	return 0;
};
/* extern */ void *		block_cache_get_empty(void *_cache,off_t blockNumber, int32_t transaction){
	printf("block_cache_get_empty not implemented\n");
	return 0;
};
/*  const void * block_cache_get_etc(void *_cache, off_t blockNumber, off_t base, off_t length){
	printf("block_cache_get_etc not implemented\n");
	return 0;
};
const void * block_cache_get(void *_cache,	off_t blockNumber){
	printf("block_cache_get not implemented\n");
	return 0;
};*/

/*!
	Retrieves the block \a blockNumber from the hash table, if it's already
	there, or reads it from the disk.

	\param _allocated tells you wether or not a new block has been allocated
		to satisfy your request.
	\param readBlock if \c false, the block will not be read in case it was
		not already in the cache. The block you retrieve may contain random
		data.
*/
static cached_block *
get_cached_block(block_cache *cache, off_t blockNumber, bool *_allocated,/*off_t offset,*/
	bool readBlock = true)
{
//	printf("Cache.cpp:get_cached_block\n");
	if (blockNumber < 0 || blockNumber >= cache->max_blocks) {
		panic("get_cached_block: invalid block number %lld (max %lld)",
			blockNumber, cache->max_blocks - 1);
		return NULL;
	}

	cached_block *block = (cached_block *)hash_lookup(cache->hash,
		&blockNumber);
	*_allocated = false;

	if (block == NULL) {
		// read block into cache
		block = cache->NewBlock(blockNumber);
		if (block == NULL)
			return NULL;

		hash_insert(cache->hash, block);
		*_allocated = true;
	} else {
		// TODO: currently, the data is always mapped in
/*
		if (block->ref_count == 0 && block->current_data != NULL) {
			// see if the old block can be resurrected
			block->current_data = cache->allocator->Acquire(block->current_data);
		}

		if (block->current_data == NULL) {
			// there is no block yet, but we need one
			block->current_data = cache->allocator->Get();
			if (block->current_data == NULL)
				return NULL;

			*_allocated = true;
		}
*/
	}
	//printf("read_pos?  ");
	if (*_allocated && readBlock) {
		int32 blockSize = cache->block_size;
		//printf("yes\n");
//TODO: read_pos should read starting from volume->StartPos()
// how to get volume->StartPos() (=2664990720) here???????
		//extern off_t Myoffset=2664990720;
		//if (read_pos(cache->fd, 59392+blockNumber * blockSize, block->current_data,
		if (read_pos(cache->fd, /*2664990720+*/blockNumber * blockSize, block->current_data,
				blockSize) < blockSize) {
			//if (read_pos(cache->fd, 59392+blockNumber * blockSize, block->current_data,blockSize) < blockSize){
			hash_remove(cache->hash, block);
			cache->FreeBlock(block);
			//}
			//TODO:
			//FATAL(("could not read block %Ld\n", blockNumber));
			printf("could not read block %Ld\n", blockNumber);
			return NULL;
		}
	}//else printf("no\n");

	if (block->unused) {
		//TRACE(("remove block %Ld from unused\n", blockNumber));
		block->unused = false;
		cache->unused_blocks.Remove(block);
	}

	block->ref_count++;
	block->accessed++;

	return block;
}

const void *
block_cache_get_etc(void *_cache, off_t blockNumber, off_t base, off_t length/*, off_t offset*/)
{
	block_cache *cache = (block_cache *)_cache;
	// TODO: benaphore stuff...
	//BenaphoreLocker locker(&cache->lock);
	bool allocated;

	cached_block *block = get_cached_block(cache, blockNumber, &allocated/*, offset*/);
	if (block == NULL){
		printf("block == NULL\n");
		return NULL;
	}

#ifdef DEBUG_CHANGED
	if (block->compare == NULL)
		block->compare = cache->Allocate();
	if (block->compare != NULL)
		memcpy(block->compare, block->current_data, cache->block_size);
#endif
	return block->current_data;
}


const void *
block_cache_get(void *_cache, off_t blockNumber)
{
	return block_cache_get_etc(_cache, blockNumber, blockNumber, 1/*,offset*/);
}

/* extern */ status_t	block_cache_set_dirty(void *_cache, off_t blockNumber, bool isDirty,int32_t transaction){
	printf("block_cache_set_dirty not implemented\n");
	return 0;
};
/* void		block_cache_put(void *_cache,off_t blockNumber){
	printf("block_cache_put not implemented\n");
}; */
void
block_cache_put(void *_cache, off_t blockNumber)
{
	block_cache *cache = (block_cache *)_cache;
	//TODO: benaphore stuff
	//BenaphoreLocker locker(&cache->lock);

	put_cached_block(cache, blockNumber);
}

static void
put_cached_block(block_cache *cache, cached_block *block)
{
#ifdef DEBUG_CHANGED
	if (!block->is_dirty && block->compare != NULL
		&& memcmp(block->current_data, block->compare, cache->block_size)) {
		dprintf("new block:\n");
		dump_block((const char *)block->current_data, 256, "  ");
		dprintf("unchanged block:\n");
		dump_block((const char *)block->compare, 256, "  ");
		write_cached_block(cache, block);
		panic("block_cache: supposed to be clean block was changed!\n");

		cache->Free(block->compare);
		block->compare = NULL;
	}
#endif

	if (--block->ref_count == 0
		&& block->transaction == NULL
		&& block->previous_transaction == NULL) {
		// put this block in the list of unused blocks
		block->unused = true;
		cache->unused_blocks.Add(block);
	}

	if (cache->allocated_block_count > kMaxBlockCount) {
		cache->RemoveUnusedBlocks(LONG_MAX,
			cache->allocated_block_count - kMaxBlockCount);
	}
}


static void
put_cached_block(block_cache *cache, off_t blockNumber)
{
	if (blockNumber < 0 || blockNumber >= cache->max_blocks) {
		panic("put_cached_block: invalid block number %lld (max %lld)",
			blockNumber, cache->max_blocks - 1);
	}
	
	cached_block *block = (cached_block *)hash_lookup(cache->hash, &blockNumber);
	if (block != NULL)
		put_cached_block(cache, block);
}

/* file cache */
status_t vm_page_put_page(vm_page* page){ 
	printf("vm_page_put_page not implemented\n"); 
	return 0;
}

vm_page * vm_page_allocate_page(int state)
{
	AutoLocker<PagePool> poolLocker(sPagePool);

	// is a queued free page available?
	vm_page* page = sPagePool.freePages.RemoveHead();
	if (page) {
		page->ref_count++;
		return page;
	}

	// no free page

	// no page yet -- allocate a new one
// TODO: nothrow?
	page = new vm_page;
	if (!page || !page->data) {
		delete page;
		return NULL;
	}

	sPagePool.allocatedPages++;

	return page;
}
void vm_cache_remove_page(file_cache_ref *cacheRef, vm_page *page){ 
	printf("vm_cache_remove_page not implemented\n"); 
}
status_t vm_page_set_state(vm_page *page, int state){ 
	printf("vm_page_set_state not implemented\n"); 
	return 0;
}

void vm_cache_insert_page(file_cache_ref *cache, vm_page *page,off_t offset){ 
	//printf("vm_cache_insert_page\n"); 
	AutoLocker<PagePool> _(sPagePool);

	if (page->cache != NULL) {
		panic("vm_cache_insert_page(%p, %p): page already in cache %p\n",
			cache, page, page->cache);
		return;
	}

	page->cache = cache;
	page->offset = offset;

	// insert page into hash
	status_t error = hash_insert(sPagePool.pageHash, page);
	if (error != B_OK) {
		panic("vm_cache_insert_page(): Failed to insert page %p into hash!\n",
			page);
		page->cache = NULL;
		page->offset = 0;
		return;
	}

	// add page to cache page list
	cache->pages.Add(page);
}
void
vm_page_get_page(vm_page* page)
{
	if (page) {
		AutoLocker<PagePool> _(sPagePool);

		// increase ref count
		page->ref_count++;

		// if the page was unused before, remove it from the unused pages list
		if (page->ref_count == 1)
			sPagePool.unusedPages.Remove(page);
	}
}
vm_page *
vm_cache_lookup_page(file_cache_ref *cache, off_t offset)
{
	printf("vm_cache_lookup_page... %I64d\n",cache->virtual_size);
	//TODO: added this myself here, where should I call this?
	if (sPagePool.pageHash==NULL){
		printf("need to init file_cache (%p): ",sPagePool.pageHash);
		file_cache_init();
	}
	else printf("NO need to init file_cache (%p)\n",sPagePool.pageHash);
	if (cache==NULL)
		return NULL;

	AutoLocker<PagePool> _(sPagePool);
	page_hash_key key(cache->deviceFD, cache->nodeID, offset);
	printf("vm_cache_lookup_page: 0x%p, 0x%p\n",sPagePool.pageHash, &key);
	vm_page* page = (vm_page*)hash_lookup(sPagePool.pageHash, &key);
	printf("vm_cache_lookup_page 3\n");
	if (page)
		vm_page_get_page(page);
	printf("vm_cache_lookup_page 4: %p\n",page);
	return page;
}

void *
file_cache_create(mount_id mountID, vnode_id vnodeID, off_t size, HANDLE fd, Volume* v)
{
	printf("file_cache_create(mountID = %ld, vnodeID = %I64d, size = %I64d, fd = 0x%p)\n", mountID, vnodeID, size, fd);
	
	// TODO: nothrow in windows??
	file_cache_ref *ref = new file_cache_ref;
	if (ref == NULL)
		return NULL;

	ref->mountID = mountID;
	ref->nodeID = vnodeID;
	ref->nodeHandle = NULL;
	ref->deviceFD = fd;
	ref->virtual_size = size;
	ref->volume = v;

	// create lock
	char buffer[32];
	//snprintf(buffer, sizeof(buffer), "file cache %ld:%lld", mountID, vnodeID);
	//TODO: unsafe, should be snprintf
	sprintf(buffer, "file cache %ld:%lld", mountID, vnodeID);
	//TODO: enable locking:
	//status_t error = mutex_init(&ref->lock, buffer);
	status_t error=0;
	if (error != B_OK) {
		printf("file_cache_create(): Failed to init mutex: %s\n",
			strerror(error));
		delete ref;
		return NULL;
	}

	return ref;
}
void file_cache_delete(void *_cacheRef){
	printf("file_cache_delete not implemented\n");
};
status_t	file_cache_set_size(void *_cacheRef,off_t size){
	printf("file_cache_set_size not implemented\n");
	return 0;
};
status_t	file_cache_sync(void *_cache){
	printf("file_cache_sync not implemented\n");
	return 0;
};

status_t
vfs_read_pages(HANDLE fd, off_t pos, const iovec *vecs, size_t count,
	off_t *_numBytes, bool fsReenter)
{
	
	// check how much the iovecs allow us to read
	off_t toRead = 0;
	for (size_t i = 0; i < count; i++)
		toRead += vecs[i].iov_len;
	printf("vfs_read_pages: count: %Lu, toRead: %I64d, pos=%I64d\n",count,toRead,pos);
	iovec* newVecs = NULL;
	if (*_numBytes < toRead) {
		// We're supposed to read less than specified by the vecs. Since
		// readv_pos() doesn't support this, we need to clone the vecs.
		//newVecs = new(nothrow) iovec[count];  
		// TODO: (nothrow) ???
		newVecs = new iovec[count];
		if (!newVecs)
			return B_NO_MEMORY;

		size_t newCount = 0;
		for (size_t i = 0; i < count && toRead > 0; i++) {
			size_t vecLen = min_c(vecs[i].iov_len, toRead);
			newVecs[i].iov_base = vecs[i].iov_base;
			newVecs[i].iov_len = vecLen;
			toRead -= vecLen;
			newCount++;
		}

		vecs = newVecs;
		count = newCount;
	}

	ssize_t bytesRead = readv_pos(fd, pos, vecs, count);
	delete[] newVecs;
	if (bytesRead < 0)
		return bytesRead;

	*_numBytes = bytesRead;
	return B_OK;
}

status_t vfs_write_pages(HANDLE fd, off_t pos, const iovec *vecs, size_t count,
	off_t *_numBytes, bool fsReenter){
	printf("vfs_write_pages not implemented\n");
	return 0;
}


file_extent *
find_file_extent(file_cache_ref *ref, off_t offset, uint32 *_index)
{
	// TODO: do binary search

	for (uint32 index = 0; index < ref->map.count; index++) {
		file_extent *extent = ref->map[index];

		if (extent->offset <= offset
			&& extent->offset + extent->disk.length > offset) {
			if (_index)
				*_index = index;
			return extent;
		}
	}

	return NULL;
}

status_t get_file_map(file_cache_ref* ref, off_t offset, off_t size,
	file_io_vec *vecs, size_t *_count)
{
	printf("get_file_map: offset = %I64d, size = %I64d\n", offset, size);
	
	//TODO: convert the file_cache_ref into volume and inode info ?
	//Volume *volume = (Volume *)_fs;
	//Inode *inode = (Inode *)_node;
	Volume *volume = ref->volume;
	Inode *inode = new Inode(volume,ref->nodeID);

	int32 blockShift = volume->BlockShift();
	size_t index = 0, max = *_count;
	block_run run;
	off_t fileOffset;

	//FUNCTION_START(("offset = %I64d, size = %I64d\n", offset, size));

	while (true) {
		static int i,j, k, l,m,n;
		status_t status = inode->FindBlockRun(offset, run, fileOffset);
		printf("blockrun: (%d, %d, %d)\n", (int)run.allocation_group, run.start, run.length);
		printf("volume->ToOffset(run)=%I64d, offset=%I64d, fileOffset=%I64d\n",volume->ToOffset(run), offset, fileOffset);
		if (status != B_OK)
			return status;
		vecs[index].offset = volume->ToOffset(run) + offset - fileOffset;
		printf("vecs[%lu].offset=%I64d\n",index,volume->ToOffset(run) + offset - fileOffset);
		vecs[index].length = (run.Length() << blockShift) - offset + fileOffset;

		offset += vecs[index].length;
		// are we already done?
		if (size <= vecs[index].length
			|| offset >= inode->Size()) {
			if (offset > inode->Size()) {
				// make sure the extent ends with the last official file
				// block (without taking any preallocations into account)
				vecs[index].length = (inode->Size() - fileOffset + volume->BlockSize() - 1)
					& ~(volume->BlockSize() - 1);
			}
			*_count = index + 1;
			delete inode;
			return B_OK;
		}
		size -= vecs[index].length;
		index++;

		if (index >= max) {
			// we're out of file_io_vecs; let's bail out
			*_count = index;
			delete inode;
			return B_BUFFER_OVERFLOW;
		}
	}

	// can never get here
	return B_ERROR;
}

//status_t pages_io(file_cache_ref *ref, off_t offset, const iovec *vecs,
//	size_t count, size_t *_numBytes, bool doWrite){
//	printf("pages_io not implemented\n");
//	return 0;
//};
/*!
	Does the dirty work of translating the request into actual disk offsets
	and reads to or writes from the supplied iovecs as specified by \a doWrite.
*/
void dump_ref(file_cache_ref *ref){
/*
struct file_cache_ref {
	mutex						lock;
	mount_id					mountID; useless
	vnode_id					nodeID; interesting
	fs_vnode					nodeHandle; 
	//int							deviceFD;
	HANDLE						deviceFD;
	off_t						virtual_size;

	cache_page_list				pages;
	cache_modified_page_list	modifiedPages;

	file_map					map;
};*/
	printf("Dumping file_cache_ref:\n");
	printf("\tmount_id=%i\n",ref->mountID);
	printf("\tvnode_id=%I64d\n",ref->nodeID);
	printf("\tnodeHandle=%p\n",ref->nodeHandle);
	printf("\tdeviceFD=%p\n",ref->deviceFD);
	printf("\tvirtual_size=%I64d\n",ref->virtual_size);
}

status_t
pages_io(file_cache_ref *ref, off_t offset, const iovec *vecs, size_t count,
	off_t *_numBytes, bool doWrite)
{
	printf("pages_io: ref = %p, offset = %I64d, size = %I64d, vecCount = %lu, %s\n",
		ref, offset, *_numBytes, count, doWrite ? "write" : "read");
	dump_ref(ref);
	// translate the iovecs into direct device accesses
	file_io_vec fileVecs[MAX_FILE_IO_VECS];
	size_t fileVecCount = 32;//MAX_FILE_IO_VECS;
	off_t numBytes = *_numBytes;
	printf("\tfileVecCount = %Lu,%Lu\n",fileVecCount,numBytes );
	status_t status = get_file_map(ref, offset, numBytes, fileVecs,
		&fileVecCount);
	
	if (status < B_OK && status != B_BUFFER_OVERFLOW) {
		printf("get_file_map(offset = %I64d, numBytes = %Lu) failed: %s\n",
			offset, numBytes, strerror(status));
		return status;
	}

	bool bufferOverflow = status == B_BUFFER_OVERFLOW;

//#ifdef TRACE_FILE_CACHE
	printf("got %lu file vecs for %I64d:%I64d%s:\n", fileVecCount, offset,
		numBytes, bufferOverflow ? " (array too small)" : "");
	for (size_t i = 0; i < fileVecCount; i++) {
		printf("  [%lu] offset = %I64d, size = %I64d\n",
			i, fileVecs[i].offset, fileVecs[i].length);
	}
//#endif

	if (fileVecCount == 0) {
		// There are no file vecs at this offset, so we're obviously trying
		// to access the file outside of its bounds
		printf("pages_io: access outside of vnode %p at offset %Ld\n",
			/*ref->vnode*/ref->nodeID, offset);
		return B_BAD_VALUE;
	}

	uint32 fileVecIndex;
	off_t size;

	if (!doWrite) {
		// now directly read the data from the device
		// the first file_io_vec can be read directly

		size = fileVecs[0].length;
		if (size > numBytes)
			size = numBytes;

		status = vfs_read_pages(ref->deviceFD, fileVecs[0].offset, vecs,
			count, &size, false);
		if (status < B_OK)
			return status;

		// TODO: this is a work-around for buggy device drivers!
		//	When our own drivers honour the length, we can:
		//	a) also use this direct I/O for writes (otherwise, it would
		//	   overwrite precious data)
		//	b) panic if the term below is true (at least for writes)
		if (size > fileVecs[0].length) {
			//dprintf("warning: device driver %p doesn't respect total length in read_pages() call!\n", ref->device);
			size = fileVecs[0].length;
		}

		ASSERT(size <= fileVecs[0].length);

		// If the file portion was contiguous, we're already done now
		if (size == numBytes)
			return B_OK;

		// if we reached the end of the file, we can return as well
		if (size != fileVecs[0].length) {
			*_numBytes = size;
			return B_OK;
		}

		fileVecIndex = 1;
	} else {
		fileVecIndex = 0;
		size = 0;
	}

	// Too bad, let's process the rest of the file_io_vecs

	size_t totalSize = size;

	// first, find out where we have to continue in our iovecs
	uint32 i = 0;
	for (i=0; i < count; i++) {
		if (size < vecs[i].iov_len)
			break;

		size -= vecs[i].iov_len;
	}

	size_t vecOffset = size;
	size_t bytesLeft = numBytes - size;

	while (true) {
		for (; fileVecIndex < fileVecCount; fileVecIndex++) {
			file_io_vec &fileVec = fileVecs[fileVecIndex];
			off_t fileOffset = fileVec.offset;
			off_t fileLeft = min_c(fileVec.length, bytesLeft);

			printf("FILE VEC [%lu] length %Ld\n", fileVecIndex, fileLeft);

			// process the complete fileVec
			while (fileLeft > 0) {
				iovec tempVecs[MAX_TEMP_IO_VECS];
				uint32 tempCount = 0;

				// size tracks how much of what is left of the current fileVec
				// (fileLeft) has been assigned to tempVecs 
				size = 0;

				// assign what is left of the current fileVec to the tempVecs
				for (size = 0; size < fileLeft && i < count
						&& tempCount < MAX_TEMP_IO_VECS;) {
					// try to satisfy one iovec per iteration (or as much as
					// possible)

					// bytes left of the current iovec
					size_t vecLeft = vecs[i].iov_len - vecOffset;
					if (vecLeft == 0) {
						vecOffset = 0;
						i++;
						continue;
					}

					printf("fill vec %ld, offset = %lu, size = %lu\n",
						i, vecOffset, size);

					// actually available bytes
					size_t tempVecSize = min_c(vecLeft, fileLeft - size);

					tempVecs[tempCount].iov_base
						//= (void *)((addr_t)vecs[i].iov_base + vecOffset);
						//TODO: why all the casting if it's same type??
						= (vecs[i].iov_base + vecOffset);
					tempVecs[tempCount].iov_len = tempVecSize;
					tempCount++;

					size += tempVecSize;
					vecOffset += tempVecSize;
				}

				off_t bytes = size;
				if (doWrite) {
					status = vfs_write_pages(ref->deviceFD, fileOffset,
						tempVecs, tempCount, &bytes, false);
				} else {
					status = vfs_read_pages(ref->deviceFD, fileOffset,
						tempVecs, tempCount, &bytes, false);
				}
				if (status < B_OK)
					return status;

				totalSize += bytes;
				bytesLeft -= size;
				fileOffset += size;
				fileLeft -= size;
				//dprintf("-> file left = %Lu\n", fileLeft);

				if (size != bytes || i >= count) {
					// there are no more bytes or iovecs, let's bail out
					*_numBytes = totalSize;
					return B_OK;
				}
			}
		}

		if (bufferOverflow) {
			status = get_file_map(ref, offset + totalSize, bytesLeft, fileVecs,
				&fileVecCount);
			printf(" bla2\n");
			if (status < B_OK && status != B_BUFFER_OVERFLOW) {
				printf("get_file_map(offset = %Ld, numBytes = %lu) failed: %s\n",
					offset, numBytes, strerror(status));
				return status;
			}

			bufferOverflow = status == B_BUFFER_OVERFLOW;
			fileVecIndex = 0;

//#ifdef TRACE_FILE_CACHE
			printf("got %lu file vecs for %Ld:%lu%s:\n", fileVecCount,
				offset + totalSize, numBytes,
				bufferOverflow ? " (array too small)" : "");
			for (size_t i = 0; i < fileVecCount; i++) {
				printf("  [%lu] offset = %Ld, size = %Ld\n",
					i, fileVecs[i].offset, fileVecs[i].length);
			}
//#endif
		} else
			break;
	}

	*_numBytes = totalSize;
	printf("PAGES_IO END\n");
	return B_OK;
}


status_t	file_cache_read(void *_cacheRef, off_t offset,void *bufferBase, off_t *_size){
	
	file_cache_ref *ref = (file_cache_ref *)_cacheRef;

	printf("file_cache_read(ref = 0x%p, offset = %I64dd, buffer = %p, size = %I64u)\n",
		ref, offset, bufferBase, *_size);
	// bufferBase is the buffer where data needs to be copied to
	return cache_io(ref, offset, (addr_t)bufferBase, _size, false);
};
status_t	file_cache_write(void *_cacheRef,off_t offset, const void *buffer,size_t *_size){
	printf("file_cache_write not implemented\n");
	return 0;
};
// TODO: when to call this??
status_t
file_cache_init()
{
	printf("file_cache_init\n");
	status_t error = sPagePool.Init();
	if (error != B_OK)
		return error;
	//TODO: enable locking!!
	error = 0;//mutex_init(&sVolumeListLock, "volume list");
	if (error != B_OK) {
		panic("file_cache_init: Failed to init volume list lock\n");
		return error;
	}

	return B_OK;
}

status_t
write_to_cache(file_cache_ref *ref, off_t offset, size_t size, addr_t buffer, size_t bufferSize){
	printf("write_to_cache not implemented\n");
	return 0;
}

/*!
	This function is called by read_into_cache() (and from there only) - it
	can only handle a certain amount of bytes, and read_into_cache() makes
	sure that it matches that criterion.
*/
inline status_t
read_chunk_into_cache(file_cache_ref *ref, off_t offset, off_t numBytes,
	int32 pageOffset, addr_t buffer, off_t bufferSize)
{
	printf("read_chunk(offset = %I64d, size =%I64d, pageOffset = %ld, buffer = %p, bufferSize = %I64d\n",
		offset, numBytes, pageOffset, buffer, bufferSize);

	iovec vecs[MAX_IO_VECS];
	int32 vecCount = 0;

	vm_page *pages[MAX_IO_VECS];
	int32 pageIndex = 0;

	// allocate pages for the cache and mark them busy
	for (off_t pos = 0; pos < numBytes; pos += B_PAGE_SIZE) {
		vm_page *page = pages[pageIndex++] = vm_page_allocate_page(PAGE_STATE_FREE);
		if (page == NULL)
			panic("no more pages!");

		page->state = PAGE_STATE_BUSY;

		vm_cache_insert_page(ref, page, offset + pos);

		addr_t virtualAddress = page->Address();

		add_to_iovec(vecs, vecCount, MAX_IO_VECS, virtualAddress, B_PAGE_SIZE);
		// ToDo: check if the array is large enough!
	}

	// TODO: enable locking		mutex_unlock(&ref->lock);

	// read file into reserved pages
	status_t status = pages_io(ref, offset, vecs, vecCount, &numBytes, false);
	if (status < B_OK) {
		// reading failed, free allocated pages

		printf("file_cache: read pages failed: %s\n", strerror(status));

		// TODO: enable locking		mutex_lock(&ref->lock);

		for (int32 i = 0; i < pageIndex; i++) {
			vm_cache_remove_page(ref, pages[i]);
			vm_page_set_state(pages[i], PAGE_STATE_FREE);
			vm_page_put_page(pages[i]);
		}

		return status;
	}

	// copy the pages and unmap them again

	for (int32 i = 0; i < vecCount; i++) {
		addr_t base = (addr_t)vecs[i].iov_base;
		size_t size = vecs[i].iov_len;

		// copy to user buffer if necessary
		if (bufferSize != 0) {
			size_t bytes = min_c(bufferSize, size - pageOffset);

			memcpy((void *)buffer, (void *)(base + pageOffset), bytes);
			buffer += bytes;
			bufferSize -= bytes;
			pageOffset = 0;
		}
	}

	// TODO: enable locking		mutex_lock(&ref->lock);

	// make the pages accessible in the cache
	for (int32 i = pageIndex; i-- > 0;) {
		vm_page_set_state(pages[i], PAGE_STATE_ACTIVE);
		vm_page_put_page(pages[i]);
	}

	return B_OK;
}


/*!
	This function reads \a size bytes directly from the file into the cache.
	If \a bufferSize does not equal zero, \a bufferSize bytes from the data
	read in are also copied to the provided \a buffer.
	This function always allocates all pages; it is the responsibility of the
	calling function to only ask for yet uncached ranges.
	The cache_ref lock must be hold when calling this function.
*/
status_t
read_into_cache(file_cache_ref *ref, off_t offset, off_t size, addr_t buffer, off_t bufferSize)
{
	printf("read_from_cache: ref = %p, offset = %I64d, size = %I64d, buffer = %p, bufferSize = %Lu\n",
		ref, offset, size, (void *)buffer, bufferSize);

	// do we have to read in anything at all?
	if (size == 0)
		return B_OK;

	// make sure "offset" is page aligned - but also remember the page offset
	int32 pageOffset = offset & (B_PAGE_SIZE - 1);
	//#define PAGE_ALIGN(x) (((x) + (B_PAGE_SIZE - 1)) & ~( - 1))
	size = PAGE_ALIGN(size + pageOffset);
	offset -= pageOffset;
	printf("size=%I64d, offset=%I64d, %i\n",size,offset,~(-1));
	while (true) {
		off_t chunkSize = size;
		if (chunkSize > (MAX_IO_VECS * B_PAGE_SIZE))
			chunkSize = MAX_IO_VECS * B_PAGE_SIZE;
		printf("size=%I64d,chunkSize=%I64d\n",size,chunkSize);
		status_t status = read_chunk_into_cache(ref, offset, chunkSize, pageOffset,
								buffer, bufferSize);
		if (status != B_OK)
			return status;

		if ((size -= chunkSize) == 0)
			return B_OK;

		if (chunkSize >= bufferSize) {
			bufferSize = 0;
			buffer = NULL;
		} else {
			bufferSize -= chunkSize - pageOffset;
			buffer += chunkSize - pageOffset;
		}

		offset += chunkSize;
		pageOffset = 0;
	}

	return B_OK;
}
status_t
satisfy_cache_io(file_cache_ref *ref, off_t offset, addr_t buffer, addr_t lastBuffer,
	bool doWrite)
{
	off_t requestSize = buffer - lastBuffer;

	if (doWrite)
		return write_to_cache(ref, offset, requestSize, lastBuffer, requestSize);
printf("read_into_cache2\n");
	return read_into_cache(ref, offset, requestSize, lastBuffer, requestSize);
}


status_t
cache_io(void *_cacheRef, off_t offset, addr_t buffer, off_t *_size, bool doWrite)
{
	if (_cacheRef == NULL)
		panic("cache_io() called with NULL ref!\n");
	
	file_cache_ref *ref = (file_cache_ref *)_cacheRef;
	
	off_t fileSize = ref->virtual_size;

	printf("cache_io(ref = %p, offset = %I64d, buffer = %p, size = %I64d, %s,%I64d)\n",
		ref, offset, (void *)buffer, *_size, doWrite ? "write" : "read",fileSize);

	// out of bounds access?
	if (offset >= fileSize || offset < 0) {
		*_size = 0;
		return B_OK;
	}

	int32 pageOffset = offset & (B_PAGE_SIZE - 1);
	off_t size = *_size;
	offset -= pageOffset;
	printf("PAGE_OFFSET=%i,offset=%I64d,size=%I64d\n",pageOffset,offset,size);

	if (offset + pageOffset + size > fileSize) {
		printf("adapt\n");
		// adapt size to be within the file's offsets
		size = fileSize - pageOffset - offset;
		*_size = size;
		printf("adapt: size=%I64d\n",size); //don't come here during test
	}

	// "offset" and "lastOffset" are always aligned to B_PAGE_SIZE,
	// the "last*" variables always point to the end of the last
	// satisfied request part

	off_t bytesLeft = size, lastLeft = size;
	int32 lastPageOffset = pageOffset;
	addr_t lastBuffer = buffer;
	off_t lastOffset = offset;
printf("\t offsets: %I64d+%i=%I64d .%I64d\n",lastOffset, lastPageOffset,lastOffset + lastPageOffset,lastLeft);
	// TODO: enable locking		mutex_lock(&ref->lock);

	for (; bytesLeft > 0; offset += B_PAGE_SIZE) {
		// check if this page is already in memory
	restart:
		vm_page *page = vm_cache_lookup_page(ref, offset);
		PagePutter pagePutter(page);
printf("offsets__0: %I64d+%i=%I64d, bytesLeft=%I64d page=%p\n",lastOffset, lastPageOffset,lastOffset + lastPageOffset,bytesLeft,page);
		if (page != NULL) {
			// The page is busy - since we need to unlock the cache sometime
			// in the near future, we need to satisfy the request of the pages
			// we didn't get yet (to make sure no one else interferes in the
			// mean time).
			status_t status = B_OK;
printf("page1!=NULL\n");
			if (lastBuffer != buffer) {
				status = satisfy_cache_io(ref, lastOffset + lastPageOffset,
					buffer, lastBuffer, doWrite);
				if (status == B_OK) {
					lastBuffer = buffer;
					lastLeft = bytesLeft;
					lastOffset = offset;
					lastPageOffset = 0;
					pageOffset = 0;
				}
			}

			if (status != B_OK) {
				// TODO: enable locking		mutex_unlock(&ref->lock);
				return status;
			}

			if (page->state == PAGE_STATE_BUSY) {
				// TODO: enable locking		mutex_unlock(&ref->lock);
				// ToDo: don't wait forever!
				//snooze(20000);
				Sleep(20000);
				// TODO: enable locking		mutex_lock(&ref->lock);
				goto restart;
			}
		}

		//TODO: test: 
		off_t bytesInPage = min_c(off_t(B_PAGE_SIZE - pageOffset), bytesLeft);
		//off_t bytesInPage = 4096;
		addr_t virtualAddress;
		printf("bytesInPage = %I64d\n",bytesInPage);
		printf("lookup page from offset %I64d: %p, size = %I64d, pageOffset = %i\n", offset, page, bytesLeft, pageOffset);
		if (page != NULL) {
printf("page2!=NULL, %I64d\n",bytesLeft);
			virtualAddress = page->Address();

			// and copy the contents of the page already in memory
			if (doWrite) {
				memcpy((void *)(virtualAddress + pageOffset), (void *)buffer, bytesInPage);

				// make sure the page is in the modified list
				if (page->state != PAGE_STATE_MODIFIED)
					vm_page_set_state(page, PAGE_STATE_MODIFIED);
			} else{
				printf("memcpy: virtualAddress=%p, pageOffset=%i, bytesInPage=%I64d, bytesLeft=%I64d\n",virtualAddress,pageOffset, bytesInPage,bytesLeft);
				memcpy((void *)buffer, (void *)(virtualAddress + pageOffset), bytesInPage);
			}

			if (bytesLeft <= bytesInPage) {
				// we've read the last page, so we're done!
				// TODO: enable locking		mutex_unlock(&ref->lock);
				return B_OK;
			}
printf("offsets__1: %I64d+%i=%I64d\n",lastOffset, lastPageOffset,lastOffset + lastPageOffset);
			// prepare a potential gap request
			lastBuffer = buffer + bytesInPage;
			lastLeft = bytesLeft - bytesInPage;
			lastOffset = offset + B_PAGE_SIZE;
			lastPageOffset = 0;
			printf("offsets___2: %I64d+%i=%I64d\n",lastOffset, lastPageOffset,lastOffset + lastPageOffset);
		}

		if (bytesLeft <= bytesInPage)
			break;
		printf("__________\n");
		buffer += bytesInPage;
		bytesLeft -= bytesInPage;
		pageOffset = 0;
	}

	// fill the last remaining bytes of the request (either write or read)

	status_t status;
	if (doWrite)
		status = write_to_cache(ref, lastOffset + lastPageOffset, lastLeft, lastBuffer, lastLeft);
	else{
		printf("read_into_cache1: bytesLeft=%I64d, %I64d+%i=%I64d\n",bytesLeft,lastOffset, lastPageOffset,lastOffset + lastPageOffset);
		
		//if (bytesLeft >0){
		//	printf("read_into_cache3: bytesLeft=%I64d, %I64d+%i=%I64d\n",bytesLeft,lastOffset, lastPageOffset,lastOffset + lastPageOffset);
		//	status = read_into_cache(ref, lastOffset + lastPageOffset, bytesLeft, lastBuffer, bytesLeft);
		//}
		//else{ 
			printf("read_into_cache4: bytesLeft=%I64d, %I64d+%i=%I64d\n",bytesLeft,lastOffset, lastPageOffset,lastOffset + lastPageOffset);
			status = read_into_cache(ref, lastOffset + lastPageOffset, lastLeft, lastBuffer, lastLeft);
		//}
	}
	// TODO: enable locking		mutex_unlock(&ref->lock);
	return status;
}

/* static */
int
cached_block::Compare(void *_cacheEntry, const void *_block)
{
	cached_block *cacheEntry = (cached_block *)_cacheEntry;
	const off_t *block = (const off_t *)_block;

	return cacheEntry->block_number - *block;
}

/* static */
uint32
cached_block::Hash(void *_cacheEntry, const void *_block, uint32 range)
{
	cached_block *cacheEntry = (cached_block *)_cacheEntry;
	const off_t *block = (const off_t *)_block;

	if (cacheEntry != NULL)
		return cacheEntry->block_number % range;

	return (uint64)*block % range;
}

file_map::file_map()
{
	array = NULL;
	count = 0;
}


file_map::~file_map()
{
	Free();
}
file_extent *
file_map::operator[](uint32 index)
{
	return ExtentAt(index);
}


file_extent *
file_map::ExtentAt(uint32 index)
{
	if (index >= count)
		return NULL;

	if (count > CACHED_FILE_EXTENTS)
		return &array[index];

	return &direct[index];
}


status_t
file_map::Add(file_io_vec *vecs, size_t vecCount, off_t &lastOffset)
{
	printf("file_map::Add(vecCount = %ld)\n", vecCount);

	off_t offset = 0;

	if (vecCount <= CACHED_FILE_EXTENTS && count == 0) {
		// just use the reserved area in the file_cache_ref structure
	} else {
		// TODO: once we can invalidate only parts of the file map,
		//	we might need to copy the previously cached file extends
		//	from the direct range
		file_extent *newMap = (file_extent *)realloc(array,
			(count + vecCount) * sizeof(file_extent));
		if (newMap == NULL)
			return B_NO_MEMORY;

		array = newMap;

		if (count != 0) {
			file_extent *extent = ExtentAt(count - 1);
			offset = extent->offset + extent->disk.length;
		}
	}

	int32 start = count;
	count += vecCount;

	for (uint32 i = 0; i < vecCount; i++) {
		file_extent *extent = ExtentAt(start + i);

		extent->offset = offset;
		extent->disk = vecs[i];

		offset += extent->disk.length;
	}

//#ifdef TRACE_FILE_CACHE
	for (uint32 i = 0; i < count; i++) {
		file_extent *extent = ExtentAt(i);
		printf("file_map::Add: [%ld] extend offset %Ld, disk offset %Ld, length %Ld\n",
			i, extent->offset, extent->disk.offset, extent->disk.length);
	}
//#endif

	lastOffset = offset;
	return B_OK;
}


void
file_map::Free()
{
	if (count > CACHED_FILE_EXTENTS)
		free(array);

	array = NULL;
	count = 0;
}

int
vm_page::Compare(void *_cacheEntry, const void *_key)
{
	vm_page *page = (vm_page*)_cacheEntry;
	const page_hash_key *key = (const page_hash_key *)_key;

	// device FD
	unsigned long long pageFD = reinterpret_cast<unsigned long long>(page->cache->deviceFD);
	unsigned long long keyFD = reinterpret_cast<unsigned long long>(key->deviceFD);
	if (pageFD != keyFD)
		return  pageFD - keyFD;
		//return  page->cache->deviceFD - key->deviceFD;

	// node ID
	if (page->cache->nodeID < key->nodeID)
		return -1;
	if (page->cache->nodeID > key->nodeID)
		return 1;

	// offset
	if (page->offset < key->offset)
		return -1;
	if (page->offset > key->offset)
		return 1;

	return 0;
}

uint32
vm_page::Hash(void *_cacheEntry, const void *_key, uint32 range)
{
	vm_page *page = (vm_page*)_cacheEntry;
	const page_hash_key *key = (const page_hash_key *)_key;

	HANDLE fd = (page ? page->cache->deviceFD : key->deviceFD);
	vnode_id id = (page ? page->cache->nodeID : key->nodeID);
	off_t offset = (page ? page->offset : key->offset);

	//TODO: how the hell are we gonna fix this??  fixed now?
	//unsigned long value = (void*)fd;
	unsigned long long value = reinterpret_cast<unsigned long long>(fd);
	value = value * 17 + id;
	value = value * 17 + offset;

	printf("vm_page::Hash unchecked, _cacheEntry=0x%p, page=0x%p, key=0x%p, range=%u, id=%I64d, offset=%I64d\nend result: %Lu\n",
		_cacheEntry,page,key,range,id,offset,value%range);
	return value % range;
	
	//return 0;
}