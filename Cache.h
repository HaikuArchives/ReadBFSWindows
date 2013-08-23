#ifndef CACHE_H
#define CACHE_H

#include "System.h"
#include "DoublyLinkedList.h"
#include "Hash.h"
#include "Lock.h"


#define CACHED_FILE_EXTENTS	2
// TODO: no idea if this value is correct in this context: The B_PAGE_SIZE constant gives the size, in bytes, of a page of RAM.
#define B_PAGE_SIZE 4096
#define MAX_IO_VECS         64  // 256 kB
#define PAGE_ALIGN(x) (((x) + (B_PAGE_SIZE - 1)) & ~(B_PAGE_SIZE - 1))

#define MAX_FILE_IO_VECS 32 
#define MAX_TEMP_IO_VECS 8
#define B_BUFFER_OVERFLOW    EINVAL


struct cache_transaction;
struct cached_block;
struct hash_table;
struct block_cache;
typedef DoublyLinkedListLink<cached_block> block_link;

static const int32 kMaxBlockCount = 1024;
enum {
	PAGE_STATE_ACTIVE = 0,
	PAGE_STATE_BUSY,
	PAGE_STATE_MODIFIED,
	PAGE_STATE_FREE
};
struct cached_block {
	cached_block	*next;			// next in hash
	cached_block	*transaction_next;
	block_link		link;
	off_t			block_number;
	void			*current_data;
	void			*original_data;
	void			*parent_data;
#ifdef DEBUG_CHANGED
	void			*compare;
#endif
	int32			ref_count;
	int32			accessed;
	bool			busy : 1;
	bool			is_writing : 1;
	bool			is_dirty : 1;
	bool			unused : 1;
	bool			unmapped : 1;
	cache_transaction *transaction;
	cache_transaction *previous_transaction;

	static int Compare(void *_cacheEntry, const void *_block);
	static uint32 Hash(void *_cacheEntry, const void *_block, uint32 range);
};

typedef DoublyLinkedList<cached_block,
	DoublyLinkedListMemberGetLink<cached_block,
		&cached_block::link> > block_list;

struct volume_list_entry;
class Volume;
typedef DoublyLinkedListLink<volume_list_entry> volume_list_link;

struct volume_list_entry {
	volume_list_link	link;
	Volume*	volume;
};
typedef DoublyLinkedList<volume_list_entry,
	DoublyLinkedListMemberGetLink<volume_list_entry,
		&volume_list_entry::link> > volume_list;

struct cache_transaction {
	cache_transaction();

	cache_transaction *next;
	int32			id;
	int32			num_blocks;
	int32			sub_num_blocks;
	cached_block	*first_block;
	block_list		blocks;
	//transaction_notification_hook notification_hook; //TODO: what's this?
	void			*notification_data;
	bool			open;
	bool			has_sub_transaction;
};

struct block_cache {
	hash_table	*hash;
	//benaphore	lock; //TODO: what with this? I'll make it an int for now
	int			lock;
	HANDLE		fd;
	off_t		max_blocks;
	size_t		block_size;
	int32		allocated_block_count;
	int32		next_transaction_id;
	cache_transaction *last_transaction;
	hash_table	*transaction_hash;

	block_list	unmapped_blocks;
	block_list	unused_blocks;

	bool		read_only;

	block_cache(HANDLE fd, off_t numBlocks, size_t blockSize, bool readOnly);
	~block_cache();

	status_t InitCheck();

	void RemoveUnusedBlocks(int32 maxAccessed = LONG_MAX, int32 count = LONG_MAX);
	void FreeBlock(cached_block *block);
	cached_block *NewBlock(off_t blockNumber);
	void Free(void *address);
	void *Allocate();

	static void LowMemoryHandler(void *data, int32 level);
};
struct file_io_vec {
	off_t	offset;
	off_t	length;
};
struct file_extent {
	off_t			offset;
	file_io_vec		disk;
};

struct file_map {
	file_map();
	~file_map();

	file_extent *operator[](uint32 index);
	file_extent *ExtentAt(uint32 index);
	status_t Add(file_io_vec *vecs, size_t vecCount, off_t &lastOffset);
	void Free();

	union {
		file_extent	direct[CACHED_FILE_EXTENTS];
		file_extent	*array;
	};
	size_t			count;
};

typedef void*	fs_vnode;
struct vm_page;
struct file_cache_ref;

//not implemented:
status_t write_to_cache(file_cache_ref *ref, off_t offset, size_t size, addr_t buffer, size_t bufferSize);
status_t vm_page_put_page(vm_page* page);
vm_page *vm_page_allocate_page(int state);
void vm_cache_remove_page(file_cache_ref *cacheRef, vm_page *page);
void vm_cache_insert_page(file_cache_ref *cacheRef, vm_page *page,
	off_t offset);
vm_page *vm_cache_lookup_page(file_cache_ref *cacheRef, off_t page);
status_t vm_page_set_state(vm_page *page, int state);
void vm_page_get_page(vm_page* page);


typedef DoublyLinkedListLink<vm_page> page_link;
// vm page
struct vm_page {
	vm_page*			next;
	page_link			link_cache;
	page_link			link_cache_modified;
	page_link			link_pool;
	file_cache_ref*		cache;
	off_t				offset;
	void*				data;
	uint8				state;
	uint32				ref_count;

	vm_page()
		: next(NULL),
		  cache(NULL),
		  offset(0),
		  data(malloc(B_PAGE_SIZE)),
		  state(PAGE_STATE_FREE),
		  ref_count(1)
	{
	}

	~vm_page()
	{
		free(data);
	}

	addr_t				Address() const	{ return (addr_t)data; }

	static int Compare(void *_cacheEntry, const void *_key);
	static uint32 Hash(void *_cacheEntry, const void *_key, uint32 range);
};

typedef DoublyLinkedList<vm_page,
	DoublyLinkedListMemberGetLink<vm_page,
		&vm_page::link_pool> > pool_page_list;

struct page_hash_key {
	page_hash_key() {}
	page_hash_key(HANDLE fd, vnode_id id, off_t offset)
		: deviceFD(fd),
		  nodeID(id),
		  offset(offset)
	{
	}

	HANDLE			deviceFD;
	vnode_id		nodeID;
	off_t			offset;
};
struct PagePool {

	PagePool()
		: pageHash(NULL),
		  unusedPages(),
		  freePages(),
		  allocatedPages(0)
	{
	}

	~PagePool()
	{
	}

	status_t Init()
	{
		status_t error = 0; //TODO: enable locking
		//status_t error = recursive_lock_init(&lock, "page pool");
		if (error != B_OK) {
			panic("PagePool: Failed to init lock\n");
			return error;
		}

		pageHash = hash_init(256, 0, &vm_page::Compare, &vm_page::Hash);
		if (pageHash == NULL) {
			panic("PagePool: Failed to create page hash\n");
			return B_NO_MEMORY;
		}

		return B_OK;
	}

	bool Lock() { //TODO: enable locking
		return true;
		//return (recursive_lock_lock(&lock) == B_OK); 
	}
	void Unlock() { //TODO: enable locking
		//recursive_lock_unlock(&lock); 
	}

	recursive_lock	lock;
	hash_table*		pageHash;
	pool_page_list	unusedPages;
	pool_page_list	freePages;
	uint32			allocatedPages;
};
struct PagePutter {
	PagePutter(vm_page* page)
		: fPage(page)
	{
	}
	
	~PagePutter()
	{
		if (fPage)
			vm_page_put_page(fPage);
	}

private:
	vm_page*	fPage;
};


typedef DoublyLinkedList<vm_page,
	DoublyLinkedListMemberGetLink<vm_page,
		&vm_page::link_cache_modified> > cache_modified_page_list;

typedef DoublyLinkedList<vm_page,
	DoublyLinkedListMemberGetLink<vm_page,
		&vm_page::link_cache> > cache_page_list;

struct file_cache_ref {
	mutex						lock;
	mount_id					mountID;
	vnode_id					nodeID;
	fs_vnode					nodeHandle;
	//int							deviceFD;
	HANDLE						deviceFD;
	off_t						virtual_size;

	cache_page_list				pages;
	cache_modified_page_list	modifiedPages;

	file_map					map;
	Volume *					volume;
};
void dump_ref(file_cache_ref *ref);

// Block cache stuff
void		block_cache_delete(void *_cache, bool allowWrites);
void *		block_cache_create(HANDLE fd, off_t numBlocks, size_t blockSize, bool readOnly);
status_t	block_cache_sync(void *_cache);
status_t	block_cache_sync_etc(void *_cache, off_t blockNumber, size_t numBlocks);
status_t	block_cache_make_writable(void *_cache, off_t blockNumber, int32_t transaction);
void *		block_cache_get_writable_etc(void *_cache,off_t blockNumber, off_t base,off_t length, int32_t transaction);
void *		block_cache_get_writable(void *_cache,off_t blockNumber, int32_t transaction);
void *		block_cache_get_empty(void *_cache,off_t blockNumber, int32_t transaction);
const void * block_cache_get_etc(void *_cache, off_t blockNumber, off_t base, off_t length/*, off_t offset*/);
const void * block_cache_get(void *_cache,	off_t blockNumber/*, off_t offset*/);
status_t	block_cache_set_dirty(void *_cache, off_t blockNumber, bool isDirty,int32_t transaction);
void		block_cache_put(void *_cache,off_t blockNumber);

// File cache stuff

void *		file_cache_create(mount_id mountID,vnode_id vnodeID, off_t size, HANDLE fd, Volume * volume);
void		file_cache_delete(void *_cacheRef);
status_t	file_cache_set_size(void *_cacheRef,off_t size);
status_t	file_cache_sync(void *_cache);
status_t	file_cache_read(void *_cacheRef, off_t offset,void *bufferBase, off_t *_size);
status_t	file_cache_write(void *_cacheRef,off_t offset, const void *buffer,size_t *_size);
//status_t	file_cache_invalidate_file_map(void *_cacheRef,off_t offset, off_t size);
//status_t	file_cache_read_pages(void *_cacheRef,off_t offset, const iovec *vecs,size_t count, size_t *_numBytes);
//status_t	file_cache_write_pages(void *_cacheRef,off_t offset, const iovec *vecs,size_t count, size_t *_numBytes);
status_t file_cache_init();


status_t vfs_read_pages(HANDLE fd, off_t pos, const iovec *vecs, size_t count,
	off_t *_numBytes, bool fsReenter);
status_t vfs_write_pages(HANDLE fd, off_t pos, const iovec *vecs, size_t count,
	off_t *_numBytes, bool fsReenter);
status_t get_file_map(file_cache_ref *ref, off_t offset, off_t size,
	file_io_vec *vecs, size_t *_count);
file_extent *
find_file_extent(file_cache_ref *ref, off_t offset, uint32 *_index);

// cache stuff
status_t pages_io(file_cache_ref *ref, off_t offset, const iovec *vecs,
	size_t count, off_t *_numBytes, bool doWrite);
inline status_t
read_chunk_into_cache(file_cache_ref *ref, off_t offset, off_t numBytes,
	int32 pageOffset, addr_t buffer, off_t bufferSize);
status_t read_into_cache(file_cache_ref *ref, off_t offset, off_t size, addr_t buffer, off_t bufferSize);
status_t satisfy_cache_io(file_cache_ref *ref, off_t offset, addr_t buffer, addr_t lastBuffer,bool doWrite);
status_t cache_io(void *_cacheRef, off_t offset, addr_t buffer, off_t *_size, bool doWrite);


static cached_block * get_cached_block(block_cache *cache, 
									   off_t blockNumber, 
									   bool *_allocated,
									   bool readBlock);
static void put_cached_block(block_cache *cache, off_t blockNumber);
static void put_cached_block(block_cache *cache, cached_block *block);


#endif // CACHE_H
