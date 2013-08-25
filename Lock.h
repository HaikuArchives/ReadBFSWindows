/* Lock - simple semaphores, read/write lock implementation
 * Roughly based on a Be sample code written by Nathan Schrenk.
 *
 * Copyright 2001-2006, Axel Dörfler, axeld@pinc-software.de.
 * This file may be used under the terms of the MIT License.
 */
#ifndef LOCK_H
#define LOCK_H

#include "system_dependencies.h"
#define DEBUG 1
#include "Debug.h"
#include "Utility.h"
#include "Debug.h"
//for semaphore functions
#include "System.h"
// WTF, why doesn't he see these defs???
#if !defined(ASSERT) && !defined(DEBUGGER)
	#define ASSERT(x) { if (!(x)) DEBUGGER(("skyfs: assert failed: " #x "\n")); }
	#define DEBUGGER(x) printf x
#endif


typedef struct recursive_lock {
    sem_id      sem;
    thread_id   holder;
    int         recursion;
} recursive_lock;

typedef struct mutex {
    sem_id      sem;
    thread_id   holder;
} mutex;

typedef struct benaphore {
    sem_id  sem;
    int32   count;
} benaphore;

// Note: this is currently a trivial r/w lock implementation
//  it will be replaced with something better later - this
//  or a similar API will be made publically available at this point.
typedef struct rw_lock {
    sem_id      sem;
    int32       count;
    benaphore   writeLock;
} rw_lock;


// Configure here if and when real benaphores should be used
//#define USE_BENAPHORE  // we're using windows :)
	// if defined, benaphores are used for the Semaphore/RecursiveLock classes
//#	define FAST_LOCK
	// the ReadWriteLock class uses a second Semaphore to
	// speed up locking - only makes sense if USE_BENAPHORE
	// is defined, too.
#ifdef FAST_LOCK
#	error implement recursive write locking first
#endif

class Semaphore {
	public:
		Semaphore(const char *name)
			:
#ifdef USE_BENAPHORE
			fSemaphore(create_sem(0, name)),
			fCount(1)
#else
			fSemaphore(create_sem(1, name))
#endif
		{
		}

		~Semaphore()
		{
			delete_sem(fSemaphore);
		}

		status_t InitCheck()
		{
			if (fSemaphore < B_OK)
				return fSemaphore;
			
			return B_OK;
		}

		status_t Lock()
		{
#ifdef USE_BENAPHORE
			if (atomic_add(&fCount, -1) <= 0)
#endif
				return acquire_sem(fSemaphore);
#ifdef USE_BENAPHORE
			return B_OK;
#endif
		}
	
		status_t Unlock()
		{
#ifdef USE_BENAPHORE
			if (atomic_add(&fCount, 1) < 0)
#endif
				return release_sem(fSemaphore);
#ifdef USE_BENAPHORE
			return B_OK;
#endif
		}

	private:
		sem_id	fSemaphore;
#ifdef USE_BENAPHORE
		vint32	fCount;
#endif
};

// a convenience class to lock a Semaphore object

class Locker {
	public:
		Locker(Semaphore &lock)
			: fLock(lock)
		{
			fStatus = lock.Lock();
			if(fStatus != B_OK) printf("skyfs: Locker::Locker: Assertion failed: fStatus == B_OK\n");
		}

		~Locker()
		{
			if (fStatus == B_OK)
				fLock.Unlock();
		}

		status_t Status() const
		{
			return fStatus;
		}

	private:
		Semaphore	&fLock;
		status_t	fStatus;
};


//**** Recursive Lock

class RecursiveLock {
	public:
		RecursiveLock(const char *name)
			:
#ifdef USE_BENAPHORE
			fSemaphore(create_sem(0, name)),
			fCount(1),
#else
			fSemaphore(create_sem(1, name)),
#endif
			fOwner(INVALID_HANDLE_VALUE)
		{
		}

		status_t LockWithTimeout(bigtime_t timeout)
		{
			//TODO: verify
			//thread_id thread = find_thread(NULL);
			HANDLE thread = GetCurrentThread();
			if (thread == fOwner) {
				fOwnerCount++;
				return B_OK;
			}

			status_t status;
#ifdef USE_BENAPHORE
			if (atomic_add(&fCount, -1) > 0)
				status = B_OK;
			else
#endif
				status = acquire_sem_etc(fSemaphore, 1, B_RELATIVE_TIMEOUT, timeout);

			if (status == B_OK) {
				fOwner = thread;
				fOwnerCount = 1;
			}

			return status;
		}

		status_t Lock()
		{
			return LockWithTimeout(B_INFINITE_TIMEOUT);
		}

		status_t Unlock()
		{
			// TODO: verify
			//thread_id thread = find_thread(NULL); 
			HANDLE thread = GetCurrentThread();
			if (thread != fOwner) {
				panic("RecursiveLock unlocked by %d, owned by %d\n", (int)thread, (int)fOwner);
			}

			if (--fOwnerCount == 0) {
				fOwner = INVALID_HANDLE_VALUE;
#ifdef USE_BENAPHORE
				if (atomic_add(&fCount, 1) < 0)
#endif
					return release_sem(fSemaphore);
			}

			return B_OK;
		}

		HANDLE Owner() const { return fOwner; }
		int32 OwnerCount() const { return fOwnerCount; }

	private:
		sem_id	fSemaphore;
#ifdef USE_BENAPHORE
		vint32	fCount;
#endif
		//thread_id	fOwner;
		HANDLE	fOwner;
		int32		fOwnerCount;
};

// a convenience class to lock an RecursiveLock object

class RecursiveLocker {
	public:
		RecursiveLocker(RecursiveLock &lock)
			: fLock(lock)
		{
			fStatus = lock.Lock();
			ASSERT(fStatus == B_OK);
		}

		~RecursiveLocker()
		{
			if (fStatus == B_OK)
				fLock.Unlock();
		}

		status_t Status() const
		{
			return fStatus;
		}

	private:
		RecursiveLock	&fLock;
		status_t		fStatus;
};


//**** Many Reader/Single Writer Lock

// This is a "fast" implementation of a single writer/many reader
// locking scheme. It's fast because it uses the benaphore idea
// to do lazy semaphore locking - in most cases it will only have
// to do some simple integer arithmetic.
// The second semaphore (fWriteLock) is needed to prevent the situation
// that a second writer can acquire the lock when there are still readers
// holding it.

#define MAX_READERS 100000

// Note: this code will break if you actually have 100000 readers
// at once. With the current thread/... limits in BeOS you can't
// touch that value, but it might be possible in the future.
// Also, you can only have about 20000 concurrent writers until
// the semaphore count exceeds the int32 bounds

// Timeouts:
// It may be a good idea to have timeouts for the WriteLocked class,
// in case something went wrong - we'll see if this is necessary,
// but it would be a somewhat poor work-around for a deadlock...
// But the only real problem with timeouts could be for things like
// "chkbfs" - because such a tool may need to lock for some more time


// define if you want to have fast locks as the foundation for the
// ReadWriteLock class - the benefit is that acquire_sem() doesn't
// have to be called when there is no one waiting.
// The disadvantage is the use of 2 real semaphores which is quite
// expensive regarding that BeOS only allows for a total of 64k
// semaphores (since every open BFS inode has such a lock).

#ifdef FAST_LOCK
class ReadWriteLock {
	public:
		ReadWriteLock(const char *name)
			:
			fWriteLock(name)
		{
			Initialize(name);
		}

		ReadWriteLock()
			:
			fWriteLock("bfs r/w w-lock")
		{
		}

		~ReadWriteLock()
		{
			delete_sem(fSemaphore);
		}

		status_t Initialize(const char *name = "bfs r/w lock")
		{
			fSemaphore = create_sem(0, name);
			fCount = MAX_READERS;
			return fSemaphore;
		}

		status_t InitCheck()
		{
			if (fSemaphore < B_OK)
				return fSemaphore;
			
			return B_OK;
		}

		status_t Lock()
		{
			if (atomic_add(&fCount, -1) <= 0)
				return acquire_sem(fSemaphore);
			
			return B_OK;
		}
		
		void Unlock()
		{
			if (atomic_add(&fCount, 1) < 0)
				release_sem(fSemaphore);
		}
		
		status_t LockWrite()
		{
			if (fWriteLock.Lock() < B_OK)
				return B_ERROR;

			int32 readers = atomic_add(&fCount, -MAX_READERS);
			status_t status = B_OK;

			if (readers < MAX_READERS) {
				// Acquire sem for all readers currently not using a semaphore.
				// But if we are not the only write lock in the queue, just get
				// the one for us
				status = acquire_sem_etc(fSemaphore, readers <= 0 ? 1 : MAX_READERS - readers, 0, 0);
			}
			fWriteLock.Unlock();

			return status;
		}
		
		void UnlockWrite()
		{
			int32 readers = atomic_add(&fCount, MAX_READERS);
			if (readers < 0) {
				// release sem for all readers only when we were the only writer
				release_sem_etc(fSemaphore, readers <= -MAX_READERS ? 1 : -readers, 0);
			}
		}

	private:
		friend class ReadLocked;
		friend class WriteLocked;

		sem_id		fSemaphore;
		vint32		fCount;
		Semaphore	fWriteLock;
};
#else	// FAST_LOCK
class ReadWriteLock {
	public:
		ReadWriteLock(const char *name)
		{
			Initialize(name);
		}

		ReadWriteLock()
		{
		}

		~ReadWriteLock()
		{
			delete_sem(fSemaphore);
		}

		status_t Initialize(const char *name = "bfs r/w lock")
		{
			fSemaphore = create_sem(MAX_READERS, name);
			return fSemaphore;
		}

		status_t InitCheck()
		{
			if (fSemaphore < B_OK)
				return fSemaphore;
			
			return B_OK;
		}

		status_t Lock()
		{
			// TODO: verify
			//thread_id currentThread = find_thread(NULL);
			HANDLE currentThread = GetCurrentThread();
			if (currentThread == fOwner) {
				fOwnerCount++;
				return B_OK;
			}
			return acquire_sem(fSemaphore);
		}
		
		void Unlock()
		{
			//TODO: verify
			//thread_id currentThread = find_thread(NULL);
			HANDLE currentThread = GetCurrentThread();
			if (fOwner == currentThread && --fOwnerCount > 0)
				return;

			release_sem(fSemaphore);
		}
		
		status_t LockWrite()
		{
			//TODO: verify
			//thread_id currentThread = find_thread(NULL);
			HANDLE currentThread = GetCurrentThread();
			if (currentThread == fOwner) {
				fOwnerCount++;
				return B_OK;
			}
			status_t status = acquire_sem_etc(fSemaphore, MAX_READERS, 0, 0);
			if (status >= B_OK) {
				fOwner = currentThread;
				fOwnerCount = 1;
			}
			return status;
		}
		
		void UnlockWrite()
		{
			if (--fOwnerCount == 0) {
				fOwner = INVALID_HANDLE_VALUE; //used to be -1
				release_sem_etc(fSemaphore, MAX_READERS, 0);
			}
		}

	private:
		friend class ReadLocked;
		friend class WriteLocked;

		sem_id		fSemaphore;
		//thread_id	fOwner;
		HANDLE fOwner;
		int32		fOwnerCount;
};
#endif	// FAST_LOCK


class ReadLocked {
	public:
		ReadLocked(ReadWriteLock &lock)
			:
			fLock(lock)
		{
			fStatus = lock.Lock();
		}
		
		~ReadLocked()
		{
			if (fStatus == B_OK)
				fLock.Unlock();
		}
	
		status_t IsLocked()
		{
			return fStatus;
		}

	private:
		ReadWriteLock	&fLock;
		status_t		fStatus;
};


class WriteLocked {
	public:
		WriteLocked(ReadWriteLock &lock)
			:
			fLock(&lock)
		{
			fStatus = lock.LockWrite();
		}

		WriteLocked(ReadWriteLock *lock)
			:
			fLock(lock)
		{
			fStatus = lock != NULL ? lock->LockWrite() : B_ERROR;
		}

		~WriteLocked()
		{
			if (fStatus == B_OK)
				fLock->UnlockWrite();
		}

		status_t IsLocked()
		{
			return fStatus;
		}

	private:
		ReadWriteLock	*fLock;
		status_t		fStatus;
};


// A simple locking structure that doesn't use a semaphore - it's useful
// if you have to protect critical parts with a short runtime.
// It also allows to nest several locks for the same thread.

class SimpleLock {
	public:
		SimpleLock()
			:
			//fHolder(-1),
			fHolder(INVALID_HANDLE_VALUE),
			fCount(0)
		{
		}

		status_t Lock(bigtime_t time = 500)
		{
			printf("SimpleLock::Lock 1\n");
			//TODO: verify
			//int32 thisThread = find_thread(NULL);
			HANDLE thisThread = GetCurrentThread();
			printf("SimpleLock::Lock 2\n");
			//int32 current;
			HANDLE current;
			int a=0;
			while (1) {
				printf("SimpleLock::Lock 2 - %i\n",a++);
				// TODO: changed back to old code
				//if (fHolder == -1) {
				if (fHolder == INVALID_HANDLE_VALUE) {
					current = fHolder;
					fHolder = thisThread;
				}
				/*current = atomic_test_and_set(&fHolder, thisThread, -1);
				if (current == -1)
					break;
				if (current == thisThread)
					break;*/
					
				//snooze(time);
				//TODO: good replacement?
				Sleep(time);
			}

			// ToDo: the lock cannot fail currently! We may want
			// to change this
			atomic_add(&fCount, 1);
			return B_OK;
		}

		void Unlock()
		{
			if (atomic_add(&fCount, -1) == 1)
				//atomic_set(&fHolder, -1);
				fHolder = INVALID_HANDLE_VALUE;
		}

		bool IsLocked() const
		{
			//return fHolder == find_thread(NULL);
			return fHolder == GetCurrentThread();
		}

	private:
		//vint32	fHolder;
		HANDLE	fHolder;
		vint32	fCount;
};

// A convenience class to lock the SimpleLock, note the
// different timing compared to the direct call

class SimpleLocker {
	public:
		SimpleLocker(SimpleLock &lock,bigtime_t time = 1000)
			: fLock(lock)
		{
			lock.Lock(time);
		}

		~SimpleLocker()
		{
			fLock.Unlock();
		}

	private:
		SimpleLock	&fLock;
};

#endif	/* LOCK_H */
