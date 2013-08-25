/* Author: Peter Speybrouck - peter.speybrouck@gmail.com
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file. As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return. Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */


#ifndef SUPPORTDEFS_H
#define SUPPORTDEFS_H
#define _CRT_SECURE_NO_DEPRECATE 1
//#define _X86_ 1
//#include <winioctl.h>
// add as dependancy (linker/input)
// C:\WINDDK\3790.1830\lib\wxp\i386\ntdll.lib
#include <windows.h>
extern "C"{
	//#include <ntddk.h>  //for NTCreateFile
	#include "myntddk.h"  //for NTCreateFile
}
//access to the debugfile
#include <fstream>
extern std::ofstream debug;

//#include <sys/stat.h>
#include <string.h>

//#define B_OK 0
#define port_id     int
#define ssize_t		signed long

//#define size_t		unsigned long
//#define size_t		long long
//#define size_t		unsigned int
//typedef long long	size_t;

#define int32		int 
#define int16		short
typedef unsigned int uint32;
#define status_t	int
#define vint32      int
#define sem_id		int
#define uint8		unsigned char 
#define uint16		unsigned short
#define thread_id   int
#define uint		unsigned int
#define bigtime_t   unsigned long long
#define int64_		long long
//#ifdef off_t
#undef off_t
//typedef long long off_t;
#define off_t long long
//#endif
#define uint64		unsigned long long
#define mode_t		unsigned int
#define int8		char
#define type_code   unsigned int
#define int32_t		int 
#define mount_id	unsigned int
//#define vnode_id	unsigned short
//typedef unsigned short vnode_id;
#define vnode_id	off_t
#define uid_t		int32
#define gid_t		int32
#define ino_t		unsigned short
#define dev_t		unsigned int

#define	NAME_MAX			255	/* max bytes in a file name */
#define B_FILE_NAME_LENGTH	NAME_MAX
#define B_OS_NAME_LENGTH	32
#define O_ACCMODE			0003
#define O_RWMASK			O_ACCMODE
#define	O_RDONLY			0		/* +1 == FREAD */
#define	O_WRONLY			1		/* +1 == FWRITE */
#define	O_RDWR				2		/* +1 == FREAD|FWRITE */
#define	S_IRWXO				0000007	/* rwx, other */
#define S_IXUSR				0000100

#define F_OK				0
#define X_OK				1
#define W_OK				2
#define R_OK				4

#define B_INFINITE_TIMEOUT	(9223372036854775807LL)
#define B_RELATIVE_TIMEOUT	8

// error codes
#define LONG_MAX 2147483647L
#pragma warning (disable: 4005)
#define LONG_MIN (-LONG_MAX - 1L) 
#pragma warning (default: 4005)
#define B_GENERAL_ERROR_BASE		LONG_MIN
#define B_OS_ERROR_BASE				B_GENERAL_ERROR_BASE + 0x1000
#define B_APP_ERROR_BASE			B_GENERAL_ERROR_BASE + 0x2000
#define B_INTERFACE_ERROR_BASE		B_GENERAL_ERROR_BASE + 0x3000
#define B_MEDIA_ERROR_BASE			B_GENERAL_ERROR_BASE + 0x4000 /* - 0x41ff */
#define B_TRANSLATION_ERROR_BASE	B_GENERAL_ERROR_BASE + 0x4800 /* - 0x48ff */
#define B_MIDI_ERROR_BASE			B_GENERAL_ERROR_BASE + 0x5000
#define B_STORAGE_ERROR_BASE		B_GENERAL_ERROR_BASE + 0x6000
#define B_POSIX_ERROR_BASE			B_GENERAL_ERROR_BASE + 0x7000
#define B_MAIL_ERROR_BASE			B_GENERAL_ERROR_BASE + 0x8000
#define B_PRINT_ERROR_BASE			B_GENERAL_ERROR_BASE + 0x9000
#define B_DEVICE_ERROR_BASE			B_GENERAL_ERROR_BASE + 0xa000

enum {
	B_NO_MEMORY = B_GENERAL_ERROR_BASE,
	B_IO_ERROR,
	B_PERMISSION_DENIED,
	B_BAD_INDEX,
	B_BAD_TYPE,
	B_BAD_VALUE,
	B_MISMATCHED_VALUES,
	B_NAME_NOT_FOUND,
	B_NAME_IN_USE,
	B_TIMED_OUT,
    B_INTERRUPTED,
	B_WOULD_BLOCK,
    B_CANCELED,
	B_NO_INIT,
	B_BUSY,
	B_NOT_ALLOWED,
	B_BAD_DATA,
	B_DONT_DO_THAT,
	B_ERROR = -1,
	B_OK = 0,
	B_NO_ERROR = 0
};
// inode types
// TODO: this is probably not right but also not needed for now but this needs to be verified:
/* extended file types */
#define S_ATTR_DIR			01000000000	/* attribute directory */
#define S_ATTR				02000000000	/* attribute */
#define S_INDEX_DIR			04000000000	/* index (or index directory) */
#define S_STR_INDEX			00100000000	/* string index */
#define S_INT_INDEX			00200000000	/* int32 index */
#define S_UINT_INDEX		00400000000	/* uint32 index */
#define S_LONG_LONG_INDEX	00010000000	/* int64 index */
#define S_ULONG_LONG_INDEX	00020000000	/* uint64 index */
#define S_FLOAT_INDEX		00040000000	/* float index */
#define S_DOUBLE_INDEX		00001000000	/* double index */
#define S_ALLOW_DUPS		00002000000	/* allow duplicate entries (currently unused) */

/* link types */
#define	S_LINK_SELF_HEALING	00001000000	/* link will be updated if you move its target */
#define S_LINK_AUTO_DELETE	00002000000	/* link will be deleted if you delete its target */

#pragma warning( disable: 4005 )
/* standard file types */
#define S_IFMT				00000170000 /* type of file */
#define	S_IFLNK				00000120000 /* symbolic link */
#define S_IFREG 			00000100000 /* regular */
#define S_IFBLK 			00000060000 /* block special */
#define S_IFDIR 			00000040000 /* directory */
#define S_IFCHR 			00000020000 /* character special */
#define S_IFIFO 			00000010000 /* fifo */
#pragma warning( default: 4005 )

//#define S_IFREG				00000100000 /* regular */
//#define	S_IFLNK				00000120000 /* symbolic link */
//#define S_IFDIR 			00000040000 /* directory */
//#define _IFMT				0170000  // TODO: do I need this? added it for S_ISLNK
//#define S_STR_INDEX			'CSTR'
//#define S_INT_INDEX			'LONG'
//#define S_UINT_INDEX		'ULNG'
//#define S_LONG_LONG_INDEX	'LLNG'
//#define S_ULONG_LONG_INDEX	'ULLG'
//#define S_FLOAT_INDEX		'FLOT'
//#define S_DOUBLE_INDEX		'DBLE'
//#define S_ATTR_DIR			01000000000
//#define S_ATTR				02000000000
//#define S_INDEX_DIR			04000000000
//#define S_ALLOW_DUPS		00002000000

#define S_ISLNK(m)	((((m)) & S_IFMT) == S_IFLNK)
#define S_ISDIR(m)	((((m)) & S_IFDIR)== S_IFDIR)

#define O_TRUNC 0x0002
#define O_EXCL  0x0004
#define B_GET_GEOMETRY	7   //some ioctl
//from TypeConstants.h
enum  	{
  B_ANY_TYPE = 'ANYT',
  B_ATOM_TYPE = 'ATOM',
  B_ATOMREF_TYPE = 'ATMR',
  B_BOOL_TYPE = 'BOOL',
  B_CHAR_TYPE = 'CHAR',
  B_COLOR_8_BIT_TYPE = 'CLRB',
  B_DOUBLE_TYPE = 'DBLE',
  B_FLOAT_TYPE = 'FLOT',
  B_GRAYSCALE_8_BIT_TYPE = 'GRYB',
  B_INT16_TYPE = 'SHRT',
  B_INT32_TYPE = 'LONG',
  B_INT64_TYPE = 'LLNG',
  B_INT8_TYPE = 'BYTE',
  B_LARGE_ICON_TYPE = 'ICON',
  B_MEDIA_PARAMETER_GROUP_TYPE = 'BMCG',
  B_MEDIA_PARAMETER_TYPE = 'BMCT',
  B_MEDIA_PARAMETER_WEB_TYPE = 'BMCW',
  B_MESSAGE_TYPE = 'MSGG',
  B_MESSENGER_TYPE = 'MSNG',
  B_MIME_TYPE = 'MIME',
  B_MINI_ICON_TYPE = 'MICN',
  B_MONOCHROME_1_BIT_TYPE = 'MNOB',
  B_OBJECT_TYPE = 'OPTR',
  B_OFF_T_TYPE = 'OFFT',
  B_PATTERN_TYPE = 'PATN',
  B_POINTER_TYPE = 'PNTR',
  B_POINT_TYPE = 'BPNT',
  B_PROPERTY_INFO_TYPE = 'SCTD',
  B_RAW_TYPE = 'RAWT',
  B_RECT_TYPE = 'RECT',
  B_REF_TYPE = 'RREF',
  B_RGB_32_BIT_TYPE = 'RGBB',
  B_RGB_COLOR_TYPE = 'RGBC',
  B_SIZE_T_TYPE = 'SIZT',
  B_SSIZE_T_TYPE = 'SSZT',
  B_STRING_TYPE = 'CSTR',
  B_TIME_TYPE = 'TIME',
  B_UINT16_TYPE = 'USHT',
  B_UINT32_TYPE = 'ULNG',
  B_UINT64_TYPE = 'ULLG',
  B_UINT8_TYPE = 'UBYT',
  B_VECTOR_ICON_TYPE = 'VICN',
  B_ASCII_TYPE = 'TEXT',
  S_MIME_TYPE = 'MIMS'
};


/*----- Storage Kit/File System Errors ------------------------*/
enum {
	B_FILE_ERROR = B_STORAGE_ERROR_BASE,
	B_FILE_NOT_FOUND,	/* discouraged; use B_ENTRY_NOT_FOUND in new code*/
	B_FILE_EXISTS,
	B_ENTRY_NOT_FOUND,
	B_NAME_TOO_LONG,
	B_NOT_A_DIRECTORY,
	B_DIRECTORY_NOT_EMPTY,
	B_DEVICE_FULL,
	B_READ_ONLY_DEVICE,
	B_IS_A_DIRECTORY,
	B_NO_MORE_FDS,
	B_CROSS_DEVICE_LINK,
	B_LINK_LIMIT,
	B_BUSTED_PIPE,
	B_UNSUPPORTED,
	B_PARTITION_TOO_SMALL
};

//dirent stuff


typedef struct dirent {
	dev_t			d_dev;		/* device */
	dev_t			d_pdev;		/* parent device (only for queries) */
	unsigned long long			d_ino;		/* inode number */
	unsigned long long			d_pino;		/* parent inode (only for queries) */
	unsigned short		d_reclen;	/* length of this record, not the name */
	char				d_name[256];	/* name of the entry (null byte terminated) */
	unsigned int  type;
	unsigned int  size;
	time_t        access;
	time_t        modify;
	time_t        create;
} dirent;

#define B_READ_ONLY_DEVICE	EINVAL //22
#define B_DEVICE_FULL 		ENOSPC //28
#define B_ENTRY_NOT_FOUND	ENOENT //2

//from fs_info.h
struct fs_info {
    dev_t   dev;                                /* fs dev_t */
    ino_t   root;                               /* root ino_t */
    uint32  flags;                              /* file system flags */
    off_t   block_size;                         /* fundamental block size */
    off_t   io_size;                            /* optimal io size */
    off_t   total_blocks;                       /* total number of blocks */
    off_t   free_blocks;                        /* number of free blocks */
    off_t   total_nodes;                        /* total number of nodes */
    off_t   free_nodes;                         /* number of free nodes */
    char    device_name[128];                   /* device holding fs */
    char    volume_name[B_FILE_NAME_LENGTH];    /* volume name */
    char    fsh_name[B_OS_NAME_LENGTH];         /* name of fs handler */
};
struct attr_stat {
        dev_t     st_dev;
        ino_t     st_ino;
        unsigned short st_mode;
        short      st_nlink;
        short      st_uid;
        short      st_gid;
        dev_t     st_rdev;
        off_t     st_size;
        time_t st_atime;
        time_t st_mtime;
        time_t st_ctime;
		uint32 st_type;
};
/*struct stat {
	dev_t st_dev;
	ino_t st_ino;
	mode_t st_mode;
	nlink_t st_nlink;
	uid_t st_uid;
	gid_t st_gid;
	dev_t st_rdev;
	off_t st_size;
	time_t st_atime;
	time_t st_mtime;
	time_t st_ctime;
	blksize_t st_blksize;
	blkcnt_t st_blocks;
	mode_t st_attr;
}; */
#if 0
struct stat {
	dev_t			st_dev;		/* "device" that this file resides on */
	ino_t			st_ino;		/* this file's inode #, unique per device */
	mode_t			st_mode;		/* mode bits (rwx for user, group, etc) */
	nlink_t			st_nlink;		/* number of hard links to this file */
	uid_t			st_uid;		/* user id of the owner of this file */
	gid_t			st_gid;		/* group id of the owner of this file */
	off_t			st_size;		/* size in bytes of this file */
	dev_t			st_rdev;		/* device type (not used) */
	size_t			st_blksize;	/* preferred block size for i/o */
	time_t			st_atime;		/* last access time */
	time_t			st_mtime;		/* last modification time */
	time_t			st_ctime;		/* last change time, not creation time */
	time_t			st_crtime;		/* creation time */

	// Haiku extensions:
	// TODO: we might also define special types for files and TTYs
	// TODO: we should find another solution for this, as BStatable::GetStat()
	//		can only retrieve the R5 stat structure
	unsigned int		st_type;		/* attribute/index type */
};
#endif //0
typedef char* caddr_t;
typedef struct iovec{
	caddr_t    iov_base;  /* base address of the data storage area */
                           /* represented by the iovec structure */
    size_t        iov_len;   /* size of the data storage area in bytes */
	//off_t        iov_len;   /* size of the data storage area in bytes */
} iovec;
#ifndef addr_t
typedef char* addr_t;
#endif
#define B_ATTR_NAME_LENGTH (B_FILE_NAME_LENGTH-1)
#define B_LIVE_QUERY   0x00000001
#define B_QUERY_NON_INDEXED 0x00000010   // TODO: just a lucky guess since no definition found
#define B_MOUNT_READ_ONLY 0x0001

typedef struct device_geometry{
	uint32 	bytes_per_sector;
	uint32 	sectors_per_track;
	uint32 	cylinder_count;
	uint32 	head_count;
	unsigned char 	device_type;
	bool 	removable;
	bool 	read_only;
	bool 	write_once;
}device_geometry;

#define panic printf
#define 	min_c(a, b)   ((a)>(b)?(b):(a))

#endif // SUPPORTDEFS_H