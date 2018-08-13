/*
 * Copyright 2001-2007, Axel Dörfler, axeld@pinc-software.de.
 * This file may be used under the terms of the MIT License.
 */

//! super block, mounting, etc.

#define _CRT_SECURE_NO_DEPRECATE 1

#include "Debug.h"
#include "Volume.h"
#include "Journal.h"
#include "Inode.h"
#include "Query.h"
//read_pos and write_pos
#include "System.h"
#include "Cache.h"

#include <string.h>

//extern ofstream debug;
static const int32 kDesiredAllocationGroups = 56;
	// This is the number of allocation groups that will be tried
	// to be given for newly initialized disks.
	// That's only relevant for smaller disks, though, since any
	// of today's disk sizes already reach the maximum length
	// of an allocation group (65536 blocks).
	// It seems to create appropriate numbers for smaller disks
	// with this setting, though (i.e. you can create a 400 MB
	// file on a 1 GB disk without the need for double indirect
	// blocks).


class DeviceOpener {
	public:
		DeviceOpener(const char *device, int partitionNr, int mode);
		DeviceOpener(int diskNr, int partitionNr, int mode);
		~DeviceOpener();

		HANDLE Open(int diskNr, int partitionNr, int mode);
		HANDLE Open(const char *device, int partitionNr, int mode);
		void *InitCache(off_t numBlocks, uint32 blockSize);
		void RemoveCache(bool allowWrites);

		void Keep();

		HANDLE Device() const { return fDevice; }
		int Mode() const { return fMode; }

		status_t GetSize(off_t *_size, uint32 *_blockSize = NULL);

		DRIVEPACKET PartitionInfo(){ return partitionInfo; }

	private:
		HANDLE		fDevice;
		int		fMode;
		void	*fBlockCache;
		PARTITION			diskInfo;
		DRIVEPACKET			partitionInfo;
};


DeviceOpener::DeviceOpener(const char *device, int partitionNr, int mode)
	:
	fBlockCache(NULL)
{
	Open(device, partitionNr,mode);
}

DeviceOpener::DeviceOpener(int diskNr, int partitionNr, int mode)
	:
	fBlockCache(NULL)
{
	Open(diskNr, partitionNr,mode);
}

DeviceOpener::~DeviceOpener()
{
	//TODO: verify changes
	//if (fDevice >= B_OK) {
	//	RemoveCache(false);
	//	close(fDevice);
	//}
	if (fDevice != INVALID_HANDLE_VALUE) {
		RemoveCache(false);
		//CloseHandle(fDevice);
		NtClose(fDevice);
		/*Status = NtClose(FileHandle);*/
	}
}


HANDLE
DeviceOpener::Open(const char *device, int partitionNr, int mode)
{
	printf("opening device: %s, skyfs partition %i\n",device, partitionNr);
	debug<<"DeviceOpener::Open: opening device: "<<device<<", partition "<<partitionNr<<std::endl;
//	DISK_GEOMETRY *pdg;
	//todo: replace open with CreateFile!
	//fDevice = open(device, mode);
	//TODO: ignore mode for now
	BOOL bResult; // results flag
	DWORD junk; // discard results
	
	fDevice = CreateFile(device, // drive to open
		GENERIC_READ, // we'd like read access
		FILE_SHARE_READ | FILE_SHARE_WRITE, // share mode
		NULL, // default security attributes
		OPEN_EXISTING, // disposition
		0, // file attributes
		NULL); // don't copy any file's attributes
	
	
	
	//TODO: removed this, seems double checking 
	//if (fDevice < 0)
	//if (fDevice == INVALID_HANDLE_VALUE)
		//fDevice = errno;  

	int i,nRet,count=-1;
	DWORD dwBytes;
	PARTITION *PartitionTbl;
	DRIVEPACKET stDrive;

	unsigned char szSector[512];
	WORD wDrive =0;

	char szTmpStr[64];
	
	DWORD dwMainPrevRelSector =0;
	DWORD dwPrevRelSector =0;
	if (fDevice== INVALID_HANDLE_VALUE){
		printf("DeviceOpene::Open: CreateFile failed\n");
		debug<<"DeviceOpene::Open: CreateFile failed\n";
		return INVALID_HANDLE_VALUE;
	}else {
		printf("DeviceOpener::Open: CreateFile success: 0x%p\n",fDevice);
		debug<<"DeviceOpene::Open: CreateFile success\n";
	}

	nRet = ReadFile(fDevice,szSector,512,&dwBytes,0);
	/*
	Status = NtReadFile(FileHandle,	// file Handle
			0,		// event Handle
			NULL,	// APC entry point
			NULL,	// APC context
			&Iosb,	// IOSB address
			q,//Message,// ptr to data buffer
			1024,	// length
			0,		// byte offset
			NULL);	// key
	*/
	if(!nRet){
		printf("DeviceOpene::Open: ReadFile error: %i\n",GetLastError());
		return INVALID_HANDLE_VALUE;
	}

	dwPrevRelSector =0;
	dwMainPrevRelSector =0;

	PartitionTbl = (PARTITION*)(szSector+0x1BE); //0x1be=446

	for(i=0; i<4; i++) /// scanning partitions in the physical disk
	{
		stDrive.wCylinder = PartitionTbl->chCylinder;
		stDrive.wHead = PartitionTbl->chHead;
		stDrive.wSector = PartitionTbl->chSector;
		stDrive.dwNumSectors = PartitionTbl->dwNumberSectors;
		stDrive.wType = ((PartitionTbl->chType == PART_EXTENDED) || (PartitionTbl->chType == PART_DOSX13X)) ? EXTENDED_PART:BOOT_RECORD;

		if((PartitionTbl->chType == PART_EXTENDED) || (PartitionTbl->chType == PART_DOSX13X))
		{
			dwMainPrevRelSector			= PartitionTbl->dwRelativeSector;
			stDrive.dwNTRelativeSector	= dwMainPrevRelSector;
		}
		else
		{
			stDrive.dwNTRelativeSector = dwMainPrevRelSector + PartitionTbl->dwRelativeSector;
		}

		if(stDrive.wType == EXTENDED_PART)
			break;

		if(PartitionTbl->chType == 0)
			break;

		switch(PartitionTbl->chType)
		{
			case PART_DOS2_FAT:
				strcpy(szTmpStr, "FAT12");
				break;
			case PART_DOSX13:
			case PART_DOS4_FAT:
			case PART_DOS3_FAT:
				strcpy(szTmpStr, "FAT16");
				break;
			case PART_DOS32X:
			case PART_DOS32:
				strcpy(szTmpStr, "FAT32");			//Normal FAT32
				break;
			case PART_NTFS:
				strcpy(szTmpStr, "NTFS");	// NTFS 
				break;
			case PART_EXT2:
				strcpy(szTmpStr, "EXT2/3");	//ext2 or ext3 partition
				break;
			case PART_SWAP:
				strcpy(szTmpStr, "Linux SWAP");	//linux swap partition
				break;
			case PART_SKYFS:
				strcpy(szTmpStr, "SKYFS");	//skyfs partition
				//printf("found skyfs partition!\n");
				count++;
				if (count==partitionNr){
					//printf("saving partion data\n");
					// copy stDrive info to DeviceOpener
					memcpy(&partitionInfo,&stDrive,sizeof(DRIVEPACKET)); 
					//printf("partitionInfo.dwNTRelativeSector=%u,stDrive.dwNTRelativeSector=%u\n",partitionInfo.dwNTRelativeSector,stDrive.dwNTRelativeSector);
				}
				break;
			case PART_BFS:
				strcpy(szTmpStr, "BFS");	//bfs partition
				//printf("found bfs partition!\n");
				count++;
				if (count==partitionNr){
					//printf("saving partion data\n");
					// copy stDrive info to DeviceOpener
					memcpy(&partitionInfo,&stDrive,sizeof(DRIVEPACKET)); 
					//printf("partitionInfo.dwNTRelativeSector=%u,stDrive.dwNTRelativeSector=%u\n",partitionInfo.dwNTRelativeSector,stDrive.dwNTRelativeSector);
				}
			default:
				strcpy(szTmpStr, "Unknown");
				break;
		}

		//printf("%s Drive %d\n", szTmpStr,wDrive);
		PartitionTbl++;
		wDrive++;
	}

	if(i==4)
		return INVALID_HANDLE_VALUE;

	for(int LogiHard=0; LogiHard<50; LogiHard++) // scanning extended partitions
	{
		if(stDrive.wType == EXTENDED_PART)
		{
			LARGE_INTEGER n64Pos;

			n64Pos.QuadPart = ((LONGLONG) stDrive.dwNTRelativeSector) * 512;
			
			nRet = SetFilePointer(fDevice, n64Pos.LowPart,&n64Pos.HighPart, FILE_BEGIN);
			if(nRet == 0xffffffff)
				return INVALID_HANDLE_VALUE;

			dwBytes = 0;

			nRet = ReadFile(fDevice, szSector, 512, (DWORD *) &dwBytes, NULL);
			/*
	Status = NtReadFile(FileHandle,	// file Handle
			0,		// event Handle
			NULL,	// APC entry point
			NULL,	// APC context
			&Iosb,	// IOSB address
			q,//Message,// ptr to data buffer
			1024,	// length
			0,		// byte offset
			NULL);	// key
	*/
			if(!nRet)
				return INVALID_HANDLE_VALUE;

			if(dwBytes != 512)
				return INVALID_HANDLE_VALUE;
			
			PartitionTbl = (PARTITION *) (szSector+0x1BE);

			for(i=0; i<4; i++)
			{
				stDrive.wCylinder = PartitionTbl->chCylinder;
				stDrive.wHead = PartitionTbl->chHead;
				stDrive.dwNumSectors = PartitionTbl->dwNumberSectors;
				stDrive.wSector = PartitionTbl->chSector;
				stDrive.dwRelativeSector = 0;
				stDrive.wType = ((PartitionTbl->chType == PART_EXTENDED) || (PartitionTbl->chType == PART_DOSX13X)) ? EXTENDED_PART:BOOT_RECORD;
				
				if((PartitionTbl->chType == PART_EXTENDED) || (PartitionTbl->chType == PART_DOSX13X))
				{
					dwPrevRelSector = PartitionTbl->dwRelativeSector;
					stDrive.dwNTRelativeSector = dwPrevRelSector + dwMainPrevRelSector;
				}
				else
				{
					stDrive.dwNTRelativeSector = dwMainPrevRelSector + dwPrevRelSector + PartitionTbl->dwRelativeSector;
				}

				if(stDrive.wType == EXTENDED_PART)
					break;

				if(PartitionTbl->chType == 0)
					break;

				switch(PartitionTbl->chType)
				{
				case PART_DOS2_FAT:
					strcpy(szTmpStr, "FAT12");
					break;
				case PART_DOSX13:
				case PART_DOS4_FAT:
				case PART_DOS3_FAT:
					strcpy(szTmpStr, "FAT16");
					break;
				case PART_DOS32X:
				case PART_DOS32:
					strcpy(szTmpStr, "FAT32");			//Normal FAT32
					break;
				case 7:
					strcpy(szTmpStr, "NTFS");
					break;
				case 131:
					strcpy(szTmpStr, "EXT2/3");	//ext2 or ext3 partition
					break;
				case 130:
					strcpy(szTmpStr, "Linux SWAP");	//linux swap partition
					break;
				case 236:
					strcpy(szTmpStr, "SKYFS");	//skyfs partition
					//printf("found skyfs partition!\n");
					count++;
					if (count==partitionNr){
						//printf("saving partion data\n");
						// copy stDrive info to DeviceOpener
						memcpy(&partitionInfo,&stDrive,sizeof(DRIVEPACKET)); 
						//printf("partitionInfo.dwNTRelativeSector=%u,stDrive.dwNTRelativeSector=%u\n",partitionInfo.dwNTRelativeSector,stDrive.dwNTRelativeSector);
					}
					break;
				case PART_BFS:
					strcpy(szTmpStr, "BFS");	//bfs partition
					//printf("found bfs partition!\n");
					count++;
					if (count==partitionNr){
						//printf("saving partion data\n");
						// copy stDrive info to DeviceOpener
						memcpy(&partitionInfo,&stDrive,sizeof(DRIVEPACKET)); 
						//printf("partitionInfo.dwNTRelativeSector=%u,stDrive.dwNTRelativeSector=%u\n",partitionInfo.dwNTRelativeSector,stDrive.dwNTRelativeSector);
					}
				break;
				default:
					strcpy(szTmpStr, "Unknown");
					break;
				}
				//printf("%s Drive %d\n", szTmpStr, wDrive);
				PartitionTbl++;
				wDrive++;
			}
			if(i==4)
				break;
		}
	}


	CloseHandle(fDevice);
	// open again but with NTCreateFile
	NTSTATUS Status;
	UNICODE_STRING UnicodeFilespec;
	OBJECT_ATTRIBUTES ObjectAttributes;
	//HANDLE FileHandle;// fDevice
	IO_STATUS_BLOCK Iosb;
	RtlInitUnicodeString(&UnicodeFilespec, L"\\Device\\HardDisk0\\Partition3"); // skyfs partition?
	InitializeObjectAttributes(&ObjectAttributes,           // ptr to structure
                               &UnicodeFilespec,            // ptr to file spec
                               OBJ_CASE_INSENSITIVE,        // attributes
                               NULL,                        // root directory handle
                               NULL );                      // ptr to security descriptor
	//Status = NtCreateFile(&FileHandle,                      // returned file handle
	Status = NtCreateFile(&fDevice,                      // returned file handle
						  FILE_READ_DATA | SYNCHRONIZE,		// desired access
                          &ObjectAttributes,                // ptr to object attributes
                          &Iosb,                            // ptr to I/O status block
                          NULL,                             // allocation size
                          FILE_ATTRIBUTE_NORMAL,            // file attributes
                          FILE_SHARE_READ | FILE_SHARE_WRITE,// share access (was 0)
						  FILE_OPEN,
                          FILE_SYNCHRONOUS_IO_NONALERT,     // create options
                          NULL,                             // ptr to extended attributes
                          0);                               // length of ea buffer
	debug<<"DeviceOpener::Open: NtCreateFile status = "<<Status<<std::endl;

	//if (fDevice < 0 && mode == O_RDWR) {
	if (fDevice == INVALID_HANDLE_VALUE && mode == O_RDWR) {
		// try again to open read-only (don't rely on a specific error code)
		return Open(device, partitionNr, O_RDONLY);
	}

	//TODO: 
	//if (fDevice >= 0) {
	if (fDevice != INVALID_HANDLE_VALUE) {
		// opening succeeded
		fMode = mode;
		if (mode == O_RDWR) {
			// check out if the device really allows for read/write access
			device_geometry geometry;
			//TODO: something like this but for read_only stuff
/*			bResult = DeviceIoControl(fDevice, // device we are querying
				IOCTL_DISK_GET_DRIVE_GEOMETRY, // operation to perform
				NULL, 0, // no input buffer, so pass zero
				pdg, sizeof(*pdg), // output buffer
				&junk, // discard count of bytes returned
				(LPOVERLAPPED) NULL); // synchronous I/O*/
// TODO: convert this to windows
/*			if (!ioctl(fDevice, B_GET_GEOMETRY, &geometry)) {
				if (geometry.read_only) {
					// reopen device read-only
					//close(fDevice);
					CloseHandle(fDevice);
					//Status = NtClose(FileHandle);
					return Open(device, O_RDONLY);
				}
			}*/
		}
	}
	//printf(" done %i %i\n",partitionInfo.dwBytesPerSector, partitionInfo.dwNumSectors);
	return fDevice;
}

HANDLE
DeviceOpener::Open(int diskNr, int partitionNr, int mode)
{
	printf("Requested: \\Device\\HardDisk%i\\Partition%i\n",diskNr,partitionNr+1);
	char device[60];
	sprintf(device,"\\\\.\\PhysicalDrive%i",diskNr);
//	DISK_GEOMETRY *pdg;
	//todo: replace open with CreateFile!
	//fDevice = open(device, mode);
	//TODO: ignore mode for now
	BOOL bResult; // results flag
	DWORD junk; // discard results
	
	fDevice = CreateFile(device, // drive to open
		GENERIC_READ, // we'd like read access
		FILE_SHARE_READ | FILE_SHARE_WRITE, // share mode
		NULL, // default security attributes
		OPEN_EXISTING, // disposition
		0, // file attributes
		NULL); // don't copy any file's attributes
	
	
	
	//TODO: removed this, seems double checking 
	//if (fDevice < 0)
	//if (fDevice == INVALID_HANDLE_VALUE)
		//fDevice = errno;  

	int i,nRet,count=-1;
	DWORD dwBytes;
	PARTITION *PartitionTbl;
	DRIVEPACKET stDrive;

	unsigned char szSector[512];
	WORD wDrive =0;

	char szTmpStr[64];
	
	DWORD dwMainPrevRelSector =0;
	DWORD dwPrevRelSector =0;
	if (fDevice== INVALID_HANDLE_VALUE){
		printf("DeviceOpene::Open: CreateFile failed\n");
		return INVALID_HANDLE_VALUE;
	}else printf("DeviceOpener::Open: CreateFile success: 0x%p\n",fDevice);

	nRet = ReadFile(fDevice,szSector,512,&dwBytes,0);
	
	if(!nRet){
		printf("DeviceOpene::Open: ReadFile error: %i\n",GetLastError());
		return INVALID_HANDLE_VALUE;
	}

	dwPrevRelSector =0;
	dwMainPrevRelSector =0;

	PartitionTbl = (PARTITION*)(szSector+0x1BE); //0x1be=446

	for(i=0; i<4; i++) /// scanning partitions in the physical disk
	{
		stDrive.wCylinder = PartitionTbl->chCylinder;
		stDrive.wHead = PartitionTbl->chHead;
		stDrive.wSector = PartitionTbl->chSector;
		stDrive.dwNumSectors = PartitionTbl->dwNumberSectors;
		stDrive.wType = ((PartitionTbl->chType == PART_EXTENDED) || (PartitionTbl->chType == PART_DOSX13X)) ? EXTENDED_PART:BOOT_RECORD;

		if((PartitionTbl->chType == PART_EXTENDED) || (PartitionTbl->chType == PART_DOSX13X))
		{
			dwMainPrevRelSector			= PartitionTbl->dwRelativeSector;
			stDrive.dwNTRelativeSector	= dwMainPrevRelSector;
		}
		else
		{
			stDrive.dwNTRelativeSector = dwMainPrevRelSector + PartitionTbl->dwRelativeSector;
		}

		if(stDrive.wType == EXTENDED_PART)
			break;

		if(PartitionTbl->chType == 0)
			break;

		switch(PartitionTbl->chType)
		{
			case PART_DOS2_FAT:
				strcpy(szTmpStr, "FAT12");
				break;
			case PART_DOSX13:
			case PART_DOS4_FAT:
			case PART_DOS3_FAT:
				strcpy(szTmpStr, "FAT16");
				break;
			case PART_DOS32X:
			case PART_DOS32:
				strcpy(szTmpStr, "FAT32");			//Normal FAT32
				break;
			case PART_NTFS:
				strcpy(szTmpStr, "NTFS");	// NTFS 
				break;
			case PART_EXT2:
				strcpy(szTmpStr, "EXT2/3");	//ext2 or ext3 partition
				break;
			case PART_SWAP:
				strcpy(szTmpStr, "Linux SWAP");	//linux swap partition
				break;
			case PART_SKYFS:
				strcpy(szTmpStr, "SKYFS");	//skyfs partition
				//printf("found skyfs partition!\n");
				count++;
				if (count==partitionNr){
					//printf("saving partion data\n");
					// copy stDrive info to DeviceOpener
					memcpy(&partitionInfo,&stDrive,sizeof(DRIVEPACKET)); 
					//printf("partitionInfo.dwNTRelativeSector=%u,stDrive.dwNTRelativeSector=%u\n",partitionInfo.dwNTRelativeSector,stDrive.dwNTRelativeSector);
				}
				break;
			case PART_BFS:
				strcpy(szTmpStr, "BFS");	//bfs partition
				//printf("found bfs partition!\n");
				count++;
				if (count==partitionNr){
					//printf("saving partion data\n");
					// copy stDrive info to DeviceOpener
					memcpy(&partitionInfo,&stDrive,sizeof(DRIVEPACKET)); 
					//printf("partitionInfo.dwNTRelativeSector=%u,stDrive.dwNTRelativeSector=%u\n",partitionInfo.dwNTRelativeSector,stDrive.dwNTRelativeSector);
				}
			default:
				strcpy(szTmpStr, "Unknown");
				break;
		}

		//printf("%s Drive %d\n", szTmpStr,wDrive);
		PartitionTbl++;
		wDrive++;
	}

	if(i==4)
		return INVALID_HANDLE_VALUE;

	for(int LogiHard=0; LogiHard<50; LogiHard++) // scanning extended partitions
	{
		if(stDrive.wType == EXTENDED_PART)
		{
			LARGE_INTEGER n64Pos;

			n64Pos.QuadPart = ((LONGLONG) stDrive.dwNTRelativeSector) * 512;
			
			nRet = SetFilePointer(fDevice, n64Pos.LowPart,&n64Pos.HighPart, FILE_BEGIN);
			if(nRet == 0xffffffff)
				return INVALID_HANDLE_VALUE;

			dwBytes = 0;

			nRet = ReadFile(fDevice, szSector, 512, (DWORD *) &dwBytes, NULL);
			
			if(!nRet)
				return INVALID_HANDLE_VALUE;

			if(dwBytes != 512)
				return INVALID_HANDLE_VALUE;
			
			PartitionTbl = (PARTITION *) (szSector+0x1BE);

			for(i=0; i<4; i++)
			{
				stDrive.wCylinder = PartitionTbl->chCylinder;
				stDrive.wHead = PartitionTbl->chHead;
				stDrive.dwNumSectors = PartitionTbl->dwNumberSectors;
				stDrive.wSector = PartitionTbl->chSector;
				stDrive.dwRelativeSector = 0;
				stDrive.wType = ((PartitionTbl->chType == PART_EXTENDED) || (PartitionTbl->chType == PART_DOSX13X)) ? EXTENDED_PART:BOOT_RECORD;
				
				if((PartitionTbl->chType == PART_EXTENDED) || (PartitionTbl->chType == PART_DOSX13X))
				{
					dwPrevRelSector = PartitionTbl->dwRelativeSector;
					stDrive.dwNTRelativeSector = dwPrevRelSector + dwMainPrevRelSector;
				}
				else
				{
					stDrive.dwNTRelativeSector = dwMainPrevRelSector + dwPrevRelSector + PartitionTbl->dwRelativeSector;
				}

				if(stDrive.wType == EXTENDED_PART)
					break;

				if(PartitionTbl->chType == 0)
					break;

				switch(PartitionTbl->chType)
				{
				case PART_DOS2_FAT:
					strcpy(szTmpStr, "FAT12");
					break;
				case PART_DOSX13:
				case PART_DOS4_FAT:
				case PART_DOS3_FAT:
					strcpy(szTmpStr, "FAT16");
					break;
				case PART_DOS32X:
				case PART_DOS32:
					strcpy(szTmpStr, "FAT32");			//Normal FAT32
					break;
				case 7:
					strcpy(szTmpStr, "NTFS");
					break;
				case 131:
					strcpy(szTmpStr, "EXT2/3");	//ext2 or ext3 partition
					break;
				case 130:
					strcpy(szTmpStr, "Linux SWAP");	//linux swap partition
					break;
				case 236:
					strcpy(szTmpStr, "SKYFS");	//skyfs partition
					//printf("found skyfs partition!\n");
					count++;
					if (count==partitionNr){
						//printf("saving partion data\n");
						// copy stDrive info to DeviceOpener
						memcpy(&partitionInfo,&stDrive,sizeof(DRIVEPACKET)); 
						//printf("partitionInfo.dwNTRelativeSector=%u,stDrive.dwNTRelativeSector=%u\n",partitionInfo.dwNTRelativeSector,stDrive.dwNTRelativeSector);
					}
					break;
				case PART_BFS:
					strcpy(szTmpStr, "BFS");	//bfs partition
					//printf("found bfs partition!\n");
					count++;
					if (count==partitionNr){
						//printf("saving partion data\n");
						// copy stDrive info to DeviceOpener
						memcpy(&partitionInfo,&stDrive,sizeof(DRIVEPACKET)); 
						//printf("partitionInfo.dwNTRelativeSector=%u,stDrive.dwNTRelativeSector=%u\n",partitionInfo.dwNTRelativeSector,stDrive.dwNTRelativeSector);
					}
				break;
				default:
					strcpy(szTmpStr, "Unknown");
					break;
				}
				//printf("%s Drive %d\n", szTmpStr, wDrive);
				PartitionTbl++;
				wDrive++;
			}
			if(i==4)
				break;
		}
	}


	CloseHandle(fDevice);
	// open again but with NTCreateFile
	NTSTATUS Status;
	UNICODE_STRING UnicodeFilespec;
	OBJECT_ATTRIBUTES ObjectAttributes;
	//HANDLE FileHandle;// fDevice
	IO_STATUS_BLOCK Iosb;
	printf("Requested: \\Device\\HardDisk%i\\Partition%i\n",diskNr,partitionNr+1);
	char disk[100];
	sprintf(disk,"\\Device\\HardDisk%i\\Partition%i",diskNr,partitionNr+1);
	 WCHAR wsDisk[256];          // Unicode string
	  MultiByteToWideChar( CP_ACP, 0, disk,
        strlen(disk)+1, wsDisk,   
     sizeof(wsDisk)/sizeof(wsDisk[0]) );
	/*int MultiByteToWideChar(
		CP_SYMBOL,		//UINT CodePage, // code page
		MB_PRECOMPOSED,	//DWORD dwFlags, // character-type options
		disk//LPCSTR lpMultiByteStr, // string to map
		int cbMultiByte, // number of bytes in string
		LPWSTR lpWideCharStr, // wide-character buffer
		int cchWideChar // size of buffer
		);*/
	//RtlInitUnicodeString(&UnicodeFilespec, L"\\Device\\HardDisk0\\Partition3"); // skyfs partition?
	RtlInitUnicodeString(&UnicodeFilespec, wsDisk); // skyfs partition?
	InitializeObjectAttributes(&ObjectAttributes,           // ptr to structure
                               &UnicodeFilespec,            // ptr to file spec
                               OBJ_CASE_INSENSITIVE,        // attributes
                               NULL,                        // root directory handle
                               NULL );                      // ptr to security descriptor
	//Status = NtCreateFile(&FileHandle,                      // returned file handle
	Status = NtCreateFile(&fDevice,                      // returned file handle
						  FILE_READ_DATA | SYNCHRONIZE,		// desired access
                          &ObjectAttributes,                // ptr to object attributes
                          &Iosb,                            // ptr to I/O status block
                          NULL,                             // allocation size
                          FILE_ATTRIBUTE_NORMAL,            // file attributes
                          FILE_SHARE_READ | FILE_SHARE_WRITE,// share access (was 0)
						  FILE_OPEN,
                          FILE_SYNCHRONOUS_IO_NONALERT,     // create options
                          NULL,                             // ptr to extended attributes
                          0);                               // length of ea buffer
	debug<<"DeviceOpener::Open: NtCreateFile status = "<<Status<<std::endl;

	//if (fDevice < 0 && mode == O_RDWR) {
	if (fDevice == INVALID_HANDLE_VALUE && mode == O_RDWR) {
		// try again to open read-only (don't rely on a specific error code)
		return Open(device, partitionNr, O_RDONLY);
	}

	//TODO: 
	//if (fDevice >= 0) {
	if (fDevice != INVALID_HANDLE_VALUE) {
		// opening succeeded
		fMode = mode;
		if (mode == O_RDWR) {
			// check out if the device really allows for read/write access
			device_geometry geometry;
			//TODO: something like this but for read_only stuff
/*			bResult = DeviceIoControl(fDevice, // device we are querying
				IOCTL_DISK_GET_DRIVE_GEOMETRY, // operation to perform
				NULL, 0, // no input buffer, so pass zero
				pdg, sizeof(*pdg), // output buffer
				&junk, // discard count of bytes returned
				(LPOVERLAPPED) NULL); // synchronous I/O*/
// TODO: convert this to windows
/*			if (!ioctl(fDevice, B_GET_GEOMETRY, &geometry)) {
				if (geometry.read_only) {
					// reopen device read-only
					//close(fDevice);
					CloseHandle(fDevice);
					//Status = NtClose(FileHandle);
					return Open(device, O_RDONLY);
				}
			}*/
		}
	}
	//printf(" done %i %i\n",partitionInfo.dwBytesPerSector, partitionInfo.dwNumSectors);
	return fDevice;
}


void *
DeviceOpener::InitCache(off_t numBlocks, uint32 blockSize)
{
	return block_cache_create(fDevice, numBlocks, blockSize, fMode == O_RDONLY);
}


void
DeviceOpener::RemoveCache(bool allowWrites)
{
	if (fBlockCache == NULL)
		return;

	block_cache_delete(fBlockCache, allowWrites);
	fBlockCache = NULL;
}


void
DeviceOpener::Keep()
{
	//fDevice = -1;
    fDevice = INVALID_HANDLE_VALUE;	
}


/** Returns the size of the device in bytes. It uses B_GET_GEOMETRY
 *	to compute the size, or fstat() if that failed.
 */

status_t
DeviceOpener::GetSize(off_t *_size, uint32 *_blockSize)
{
	/*
typedef struct _DISK_GEOMETRY {
    LARGE_INTEGER Cylinders;
    MEDIA_TYPE MediaType;
    DWORD TracksPerCylinder;
    DWORD SectorsPerTrack;
    DWORD BytesPerSector;
} DISK_GEOMETRY, *PDISK_GEOMETRY;
*/
	BOOL bResult; // results flag
	DWORD junk; // discard results
	DISK_GEOMETRY pdg;
	bResult = DeviceIoControl(fDevice, // device we are querying
		IOCTL_DISK_GET_DRIVE_GEOMETRY, // operation to perform
		NULL, 0, // no input buffer, so pass zero
		&pdg, sizeof(pdg), // output buffer
		&junk, // discard count of bytes returned
		(LPOVERLAPPED) NULL); // synchronous I/O
	if (_blockSize)
		*_blockSize = pdg.BytesPerSector;
	if (_size) {
		//TODO: add head_count
		//*_size = 1LL //* geometry.head_count 
		//	* pdg.Cylinders.QuadPart
		//	* pdg.SectorsPerTrack *  pdg.BytesPerSector;
		*_size = 1LL * /*partitionInfo.dwBytesPerSector*/ 512 * partitionInfo.dwNumSectors;
		//printf("DeviceOpener::GetSize: %I64d\n",*_size);
	}
/*
	device_geometry geometry;
	if (ioctl(fDevice, B_GET_GEOMETRY, &geometry) < 0) {
		// maybe it's just a file
		// todo: what with these 3 lines?
		struct stat stat;
		//if (fstat(fDevice, &stat) < 0)
		//	return B_ERROR;

		if (_size)
			*_size = stat.st_size;
		if (_blockSize)	// that shouldn't cause us any problems
			*_blockSize = 512;

		return B_OK;
	}

	if (_size) {
		*_size = 1LL * geometry.head_count * geometry.cylinder_count
			* geometry.sectors_per_track * geometry.bytes_per_sector;
	}
	if (_blockSize)
		*_blockSize = geometry.bytes_per_sector;
*/
	return B_OK;
}


//	#pragma mark -


bool
disk_super_block::IsValid()
{
	//check for a valid SKYFS or BFS partition
	if ((Magic1() != (int32)SUPER_BLOCK_MAGIC1 && Magic1() != (int32)BFS_SUPER_BLOCK_MAGIC1)
		|| (Magic2() != (int32)SUPER_BLOCK_MAGIC2 && Magic2() != (int32)BFS_SUPER_BLOCK_MAGIC2)
		|| (Magic3() != (int32)SUPER_BLOCK_MAGIC3 && Magic3() != (int32)BFS_SUPER_BLOCK_MAGIC3)
		|| (int32)block_size != inode_size
		|| ByteOrder() != SUPER_BLOCK_FS_LENDIAN
		|| (1UL << BlockShift()) != BlockSize()
		|| AllocationGroups() < 1
		|| AllocationGroupShift() < 1
		|| BlocksPerAllocationGroup() < 1
		|| NumBlocks() < 10
		|| AllocationGroups() != divide_roundup((int64_)NumBlocks(),
			1L << AllocationGroupShift()))
	{
		//printf("IsValid failed\n");
		return false;
	}
	//printf("IsValid successfull\n");
	return true;
}


void
disk_super_block::Initialize(const char *diskName, off_t numBlocks, uint32 blockSize)
{
	memset(this, 0, sizeof(disk_super_block));

	magic1 = HOST_ENDIAN_TO_BFS_INT32(SUPER_BLOCK_MAGIC1);
	magic2 = HOST_ENDIAN_TO_BFS_INT32(SUPER_BLOCK_MAGIC2);
	magic3 = HOST_ENDIAN_TO_BFS_INT32(SUPER_BLOCK_MAGIC3);
	fs_byte_order = HOST_ENDIAN_TO_BFS_INT32(SUPER_BLOCK_FS_LENDIAN);
	flags = HOST_ENDIAN_TO_BFS_INT32(SUPER_BLOCK_DISK_CLEAN);
//TODO, this should be strlcpy
	strcpy(name, diskName/*, sizeof(name)*/);

	int32 blockShift = 9;
	while ((1UL << blockShift) < blockSize) {
		blockShift++;
	}

	block_size = inode_size = HOST_ENDIAN_TO_BFS_INT32(blockSize);
	block_shift = HOST_ENDIAN_TO_BFS_INT32(blockShift);

	num_blocks = HOST_ENDIAN_TO_BFS_INT64(numBlocks);
	used_blocks = 0;

	// Get the minimum ag_shift (that's determined by the block size)

	int32 bitsPerBlock = blockSize << 3;
	off_t bitmapBlocks = (numBlocks + bitsPerBlock - 1) / bitsPerBlock;
	int32 blocksPerGroup = 1;
	int32 groupShift = 13;

	for (int32 i = 8192; i < bitsPerBlock; i *= 2) {
		groupShift++;
	}

	// Many allocation groups help applying allocation policies, but if
	// they are too small, we will need to many block_runs to cover large
	// files (see above to get an explanation of the kDesiredAllocationGroups
	// constant).

	int32 numGroups;

	while (true) {
		numGroups = (bitmapBlocks + blocksPerGroup - 1) / blocksPerGroup;
		if (numGroups > kDesiredAllocationGroups) {
			if (groupShift == 16)
				break;

			groupShift++;
			blocksPerGroup *= 2;
		} else
			break;
	}

	num_ags = HOST_ENDIAN_TO_BFS_INT32(numGroups);
	blocks_per_ag = HOST_ENDIAN_TO_BFS_INT32(blocksPerGroup);
	ag_shift = HOST_ENDIAN_TO_BFS_INT32(groupShift);
}


//	#pragma mark -


Volume::Volume(mount_id id)
	:
	fID(id),
	fBlockAllocator(this),
	fLock("skyfs volume"),
	fRootNode(NULL),
	fIndicesNode(NULL),
	fDirtyCachedBlocks(0),
	fUniqueID(0),
	fFlags(0)
{
}

Volume::~Volume()
{
}


bool
Volume::IsValidSuperBlock()
{
	return fSuperBlock.IsValid();
}


void
Volume::Panic()
{
	FATAL(("we have to panic... switch to read-only mode!\n"));
	fFlags |= VOLUME_READ_ONLY;
#ifdef DEBUG
	DEBUGGER(("BFS panics!"));
#endif
}
void
dump_inode2(const bfs_inode *inode)
{
	printf("Root inode:\n");
	printf("  magic1             = %08x (%s) %s\n", (int)inode->magic1,
		get_tupel2(inode->magic1), (inode->magic1 == INODE_MAGIC1 ? "valid" : "INVALID"));
	dump_block_run2(	"  inode_num          = ", inode->inode_num);
	printf("  uid                = %u\n", (unsigned)inode->uid);
	printf("  gid                = %u\n", (unsigned)inode->gid);
	printf("  mode               = %08x\n", (int)inode->mode);
	printf("  flags              = %08x\n", (int)inode->flags);
	printf("  create_time        = %Ld (%Ld)\n", inode->create_time,
		inode->create_time >> INODE_TIME_SHIFT);
	printf("  last_modified_time = %Ld (%Ld)\n", inode->last_modified_time,
		inode->last_modified_time >> INODE_TIME_SHIFT);
	dump_block_run2(	"  parent             = ", inode->parent);
	dump_block_run2(	"  attributes         = ", inode->attributes);
	printf("  type               = %u\n", (unsigned)inode->type);
	printf("  inode_size         = %u\n", (unsigned)inode->inode_size);
	printf("  etc                = %#08x\n", (int)inode->etc);
	printf("  short_symlink      = %s\n",
		S_ISLNK(inode->mode) && (inode->flags & INODE_LONG_SYMLINK) == 0 ?
			inode->short_symlink : "-");
	dump_data_stream2(&(inode->data));
	printf("  --\n  pad[0]             = %08x\n", (int)inode->pad[0]);
	printf("  pad[1]             = %08x\n", (int)inode->pad[1]);
	printf("  pad[2]             = %08x\n", (int)inode->pad[2]);
	printf("  pad[3]             = %08x\n", (int)inode->pad[3]);
}

status_t
Volume::Mount(const char *deviceName, int partitionNr, uint32 flags)
{
	// partitionNr x is the x'th skyfs partition, starting at 0 for the first partition
	skyfsNr = partitionNr;
	// ToDo: validate the FS in write mode as well!
#if (B_HOST_IS_LENDIAN && defined(BFS_BIG_ENDIAN_ONLY)) \
	|| (B_HOST_IS_BENDIAN && defined(BFS_LITTLE_ENDIAN_ONLY))
	// in big endian mode, we only mount read-only for now
	flags |= B_MOUNT_READ_ONLY;
#endif

	DeviceOpener opener(deviceName, partitionNr, flags & B_MOUNT_READ_ONLY ? O_RDONLY : O_RDWR);
	fDevice = opener.Device();
	partitionInfo = opener.PartitionInfo();
//	printf("done opening\n");
	int m_dwBytesPerSector = 512;
	DWORD partStartSector = partitionInfo.dwNTRelativeSector;
//	printf("partStartSector=%u\n",partStartSector);
	fStartPos.QuadPart = (LONGLONG)m_dwBytesPerSector*partStartSector; 
//	printf("partStartSector=%I64d\n",fStartPos.QuadPart);
//	if (fDevice < B_OK)
	if (fDevice == INVALID_HANDLE_VALUE)
		//RETURN_ERROR(fDevice);
		RETURN_ERROR(-1);

	if (opener.Mode() == O_RDONLY)
		fFlags |= VOLUME_READ_ONLY;

	// check if it's a regular file, and if so, disable the cache for the
	// underlaying file system
	//TODO: what to do with these 3 lines? removed due to type issue
	//struct stat stat;
	//if (fstat(fDevice, &stat) < 0)
	//	RETURN_ERROR(B_ERROR);

// TODO: allow turning off caching of the underlying file (once O_NOCACHE works)
#if 0
#ifndef NO_FILE_UNCACHED_IO
	if ((stat.st_mode & S_FILE) != 0 && ioctl(fDevice, IOCTL_FILE_UNCACHED_IO, NULL) < 0) {
		// mount read-only if the cache couldn't be disabled
#	ifdef DEBUG
		FATAL(("couldn't disable cache for image file - system may dead-lock!\n"));
#	else
		FATAL(("couldn't disable cache for image file!\n"));
		Panic();
#	endif
	}
#endif
#endif
printf("going to read superblock\n");
	// read the super block
	if (Identify(fDevice, &fSuperBlock) != B_OK) {
		FATAL(("invalid super block!\n"));
		return B_BAD_VALUE;
	}
//printf("done reading superblock\n");
	// initialize short hands to the super block (to save byte swapping)
	fBlockSize = fSuperBlock.BlockSize();
	fBlockShift = fSuperBlock.BlockShift();
	fAllocationGroupShift = fSuperBlock.AllocationGroupShift();

	// check if the device size is large enough to hold the file system
	off_t diskSize;
	opener.GetSize(&diskSize);
	printf("diskSize=%I64d\n",diskSize);
	if (opener.GetSize(&diskSize) < B_OK)
		RETURN_ERROR(B_ERROR);
//	printf("Mount: %I64d, %I64d, %u, %I64d.\n",diskSize,NumBlocks(),BlockShift(),(NumBlocks() << BlockShift()));
	if (diskSize < (NumBlocks() << BlockShift()))
		RETURN_ERROR(B_BAD_VALUE); 

	// set the current log pointers, so that journaling will work correctly
	fLogStart = fSuperBlock.LogStart();
	fLogEnd = fSuperBlock.LogEnd();
printf("Mount:InitCache\n");
	if ((fBlockCache = opener.InitCache(NumBlocks(), fBlockSize)) == NULL)
		return B_ERROR;
printf("Mount:Journal\n");
	fJournal = new Journal(this);
	// replaying the log is the first thing we will do on this disk
// TODO: we ignore journalling for now...
	//if (fJournal && fJournal->InitCheck() < B_OK 
	//	|| fBlockAllocator.Initialize() < B_OK) {
	//	// ToDo: improve error reporting for a bad journal
	//	FATAL(("could not initialize journal/block bitmap allocator!\n"));
	//	return B_NO_MEMORY;
	//}
printf("Mount:RootNode\n============================\n");
	status_t status = B_OK;

	fRootNode = new Inode(this, ToVnode(Root()));  //ToVnode(Root()) = 524288
	if (fRootNode && fRootNode->InitCheck() == B_OK) {
		//printf("  fRootNode && fRootNode->InitCheck() == B_OK\n");
		status = publish_vnode(fID, ToVnode(Root()), (void *)fRootNode);
		if (status == B_OK) {
			printf("  status == B_OK\n");
			// try to get indices root dir

			// question: why doesn't get_vnode() work here??
			// answer: we have not yet backpropagated the pointer to the
			// volume in bfs_mount(), so bfs_read_vnode() can't get it.
			// But it's not needed to do that anyway.

			if (!Indices().IsZero()){
				printf("creating indices Inode\n============================\n");
				fIndicesNode = new Inode(this, ToVnode(Indices()));
			}

			if (fIndicesNode == NULL
				|| fIndicesNode->InitCheck() < B_OK
				|| !fIndicesNode->IsContainer()) {
				INFORM(("bfs: volume doesn't have indices!\n"));

				if (fIndicesNode) {
					// if this is the case, the index root node is gone bad, and
					// BFS switch to read-only mode
					fFlags |= VOLUME_READ_ONLY;
					delete fIndicesNode;
					fIndicesNode = NULL;
				}
			}
			//printf("  all went fine\nroot inode: \n");
			//dump_inode2(&(fRootNode->Node()));
			//printf("index inode: \n");
			//dump_inode2(&(fIndicesNode->Node()));
			// all went fine
			opener.Keep();
			printf("Mounting done\n");
			return B_OK;
		} else{
			printf("  status != B_OK\n");
			FATAL(("could not create root node: publish_vnode() failed!\n"));
		}

		delete fRootNode;
	} else {
		status = B_BAD_VALUE;
		FATAL(("could not create root node!\n"));
	}
	printf("Mounting done\n");
	return status;
}
status_t
Volume::Mount(int diskNr, int partitionNr, uint32 flags)
{
	debug<<"Volume::Mount\n";
	// partitionNr x is the x'th skyfs partition, starting at 0 for the first partition
	//skyfsNr = partitionNr;
	// ToDo: validate the FS in write mode as well!
#if (B_HOST_IS_LENDIAN && defined(BFS_BIG_ENDIAN_ONLY)) \
	|| (B_HOST_IS_BENDIAN && defined(BFS_LITTLE_ENDIAN_ONLY))
	// in big endian mode, we only mount read-only for now
	flags |= B_MOUNT_READ_ONLY;
#endif

	DeviceOpener opener(diskNr, partitionNr, flags & B_MOUNT_READ_ONLY ? O_RDONLY : O_RDWR);
	fDevice = opener.Device();
	partitionInfo = opener.PartitionInfo();
//	printf("done opening\n");
	int m_dwBytesPerSector = 512;
// TODO: do we really still need this?
	DWORD partStartSector = partitionInfo.dwNTRelativeSector;
//	printf("partStartSector=%u\n",partStartSector);
	fStartPos.QuadPart = (LONGLONG)m_dwBytesPerSector*partStartSector; 
//	printf("partStartSector=%I64d\n",fStartPos.QuadPart);
//	if (fDevice < B_OK)
	if (fDevice == INVALID_HANDLE_VALUE)
		//RETURN_ERROR(fDevice);
		RETURN_ERROR(-1);

	if (opener.Mode() == O_RDONLY)
		fFlags |= VOLUME_READ_ONLY;

	// check if it's a regular file, and if so, disable the cache for the
	// underlaying file system
	//TODO: what to do with these 3 lines? removed due to type issue
	//struct stat stat;
	//if (fstat(fDevice, &stat) < 0)
	//	RETURN_ERROR(B_ERROR);

// TODO: allow turning off caching of the underlying file (once O_NOCACHE works)
#if 0
#ifndef NO_FILE_UNCACHED_IO
	if ((stat.st_mode & S_FILE) != 0 && ioctl(fDevice, IOCTL_FILE_UNCACHED_IO, NULL) < 0) {
		// mount read-only if the cache couldn't be disabled
#	ifdef DEBUG
		FATAL(("couldn't disable cache for image file - system may dead-lock!\n"));
#	else
		FATAL(("couldn't disable cache for image file!\n"));
		Panic();
#	endif
	}
#endif
#endif
printf("going to read superblock\n");
debug<<"Reading superblock..."<<std::endl;
	// read the super block
	if (Identify(fDevice, &fSuperBlock) != B_OK) {
		FATAL(("invalid super block!\n"));
		debug<<"Volume::Mount: invalid superblock\n";
		return B_BAD_VALUE;
	}
//printf("done reading superblock\n");
	// initialize short hands to the super block (to save byte swapping)
	fBlockSize = fSuperBlock.BlockSize();
	fBlockShift = fSuperBlock.BlockShift();
	fAllocationGroupShift = fSuperBlock.AllocationGroupShift();

	// check if the device size is large enough to hold the file system
	off_t diskSize;
	if (opener.GetSize(&diskSize) < B_OK){
		debug<<"Volume::Mount: opener.GetSize(&diskSize) < B_OK: "<<opener.GetSize(&diskSize)<<std::endl;
		RETURN_ERROR(B_ERROR);
	}
//	printf("Mount: %I64d, %I64d, %u, %I64d.\n",diskSize,NumBlocks(),BlockShift(),(NumBlocks() << BlockShift()));
	if (diskSize < (NumBlocks() << BlockShift())){
		debug<<"Volume::Mount: diskSize < (NumBlocks() << BlockShift()): "<<diskSize<<", "<<(NumBlocks() << BlockShift())<<std::endl;
		RETURN_ERROR(B_BAD_VALUE); 
	}

	// set the current log pointers, so that journaling will work correctly
	fLogStart = fSuperBlock.LogStart();
	fLogEnd = fSuperBlock.LogEnd();
printf("Mount:InitCache\n");
	debug<<"Volume::Mount: InitCache"<<std::endl;
	if ((fBlockCache = opener.InitCache(NumBlocks(), fBlockSize)) == NULL)
		return B_ERROR;
	debug<<"Volume::Mount: creating journal"<<std::endl;
printf("Mount:Journal\n");
	fJournal = new Journal(this);
	// replaying the log is the first thing we will do on this disk
// TODO: we ignore journalling for now...
	//if (fJournal && fJournal->InitCheck() < B_OK 
	//	|| fBlockAllocator.Initialize() < B_OK) {
	//	// ToDo: improve error reporting for a bad journal
	//	FATAL(("could not initialize journal/block bitmap allocator!\n"));
	//	return B_NO_MEMORY;
	//}
printf("Mount:RootNode\n============================\n");
	debug<<"Volume::Mount: creating rootnode"<<std::endl;
	status_t status = B_OK;

	fRootNode = new Inode(this, ToVnode(Root()));  //ToVnode(Root()) = 524288
	if (fRootNode && fRootNode->InitCheck() == B_OK) {
		//printf("  fRootNode && fRootNode->InitCheck() == B_OK\n");
		status = publish_vnode(fID, ToVnode(Root()), (void *)fRootNode);
		if (status == B_OK) {
			printf("  status == B_OK\n");
			// try to get indices root dir

			// question: why doesn't get_vnode() work here??
			// answer: we have not yet backpropagated the pointer to the
			// volume in bfs_mount(), so bfs_read_vnode() can't get it.
			// But it's not needed to do that anyway.

			if (!Indices().IsZero()){
				printf("creating indices Inode\n============================\n");
				fIndicesNode = new Inode(this, ToVnode(Indices()));
			}

			if (fIndicesNode == NULL
				|| fIndicesNode->InitCheck() < B_OK
				|| !fIndicesNode->IsContainer()) {
				INFORM(("bfs: volume doesn't have indices!\n"));

				if (fIndicesNode) {
					// if this is the case, the index root node is gone bad, and
					// BFS switch to read-only mode
					fFlags |= VOLUME_READ_ONLY;
					delete fIndicesNode;
					fIndicesNode = NULL;
				}
			}
			//printf("  all went fine\nroot inode: \n");
			//dump_inode2(&(fRootNode->Node()));
			//printf("index inode: \n");
			//dump_inode2(&(fIndicesNode->Node()));
			// all went fine
			opener.Keep();
			printf("Mounting done\n");
			return B_OK;
		} else{
			printf("  status != B_OK\n");
			debug<<"Volume::Mount: could not create root node: publish_vnode() failed!"<<status<<std::endl;
			FATAL(("could not create root node: publish_vnode() failed!\n"));
		}

		delete fRootNode;
	} else {
		status = B_BAD_VALUE;
		FATAL(("could not create root node!\n"));
		debug<<"Volume::Mount: could not create root node!"<<std::endl;
	}
	printf("Mounting done\n");
	debug<<"Volume::Mount: mounting done, status="<<status<<std::endl;
	return status;
}
status_t
Volume::MountBFS(const char *deviceName, int partitionNr, uint32 flags)
{
	// partitionNr x is the x'th skyfs partition, starting at 0 for the first partition
	skyfsNr = partitionNr;
	// ToDo: validate the FS in write mode as well!
#if (B_HOST_IS_LENDIAN && defined(BFS_BIG_ENDIAN_ONLY)) \
	|| (B_HOST_IS_BENDIAN && defined(BFS_LITTLE_ENDIAN_ONLY))
	// in big endian mode, we only mount read-only for now
	flags |= B_MOUNT_READ_ONLY;
#endif

	DeviceOpener opener(deviceName, partitionNr, flags & B_MOUNT_READ_ONLY ? O_RDONLY : O_RDWR);
	fDevice = opener.Device();
	partitionInfo = opener.PartitionInfo();
//	printf("done opening\n");
	int m_dwBytesPerSector = 512;
	DWORD partStartSector = partitionInfo.dwNTRelativeSector;
//	printf("partStartSector=%u\n",partStartSector);
	fStartPos.QuadPart = (LONGLONG)m_dwBytesPerSector*partStartSector; 
//	printf("partStartSector=%I64d\n",fStartPos.QuadPart);
//	if (fDevice < B_OK)
	if (fDevice == INVALID_HANDLE_VALUE)
		//RETURN_ERROR(fDevice);
		RETURN_ERROR(-1);

	if (opener.Mode() == O_RDONLY)
		fFlags |= VOLUME_READ_ONLY;

	// check if it's a regular file, and if so, disable the cache for the
	// underlaying file system
	//TODO: what to do with these 3 lines? removed due to type issue
	//struct stat stat;
	//if (fstat(fDevice, &stat) < 0)
	//	RETURN_ERROR(B_ERROR);

// TODO: allow turning off caching of the underlying file (once O_NOCACHE works)
#if 0
#ifndef NO_FILE_UNCACHED_IO
	if ((stat.st_mode & S_FILE) != 0 && ioctl(fDevice, IOCTL_FILE_UNCACHED_IO, NULL) < 0) {
		// mount read-only if the cache couldn't be disabled
#	ifdef DEBUG
		FATAL(("couldn't disable cache for image file - system may dead-lock!\n"));
#	else
		FATAL(("couldn't disable cache for image file!\n"));
		Panic();
#	endif
	}
#endif
#endif
printf("going to read superblock\n");
	// read the super block
	if (Identify(fDevice, &fSuperBlock) != B_OK) {
		FATAL(("invalid super block!\n"));
		return B_BAD_VALUE;
	}
//printf("done reading superblock\n");
	// initialize short hands to the super block (to save byte swapping)
	fBlockSize = fSuperBlock.BlockSize();
	fBlockShift = fSuperBlock.BlockShift();
	fAllocationGroupShift = fSuperBlock.AllocationGroupShift();

	// check if the device size is large enough to hold the file system
	off_t diskSize;
	if (opener.GetSize(&diskSize) < B_OK)
		RETURN_ERROR(B_ERROR);
//	printf("Mount: %I64d, %I64d, %u, %I64d.\n",diskSize,NumBlocks(),BlockShift(),(NumBlocks() << BlockShift()));
	if (diskSize < (NumBlocks() << BlockShift()))
		RETURN_ERROR(B_BAD_VALUE); 

	// set the current log pointers, so that journaling will work correctly
	fLogStart = fSuperBlock.LogStart();
	fLogEnd = fSuperBlock.LogEnd();
printf("Mount:InitCache\n");
	if ((fBlockCache = opener.InitCache(NumBlocks(), fBlockSize)) == NULL)
		return B_ERROR;
printf("Mount:Journal\n");
	fJournal = new Journal(this);
	// replaying the log is the first thing we will do on this disk
// TODO: we ignore journalling for now...
	//if (fJournal && fJournal->InitCheck() < B_OK 
	//	|| fBlockAllocator.Initialize() < B_OK) {
	//	// ToDo: improve error reporting for a bad journal
	//	FATAL(("could not initialize journal/block bitmap allocator!\n"));
	//	return B_NO_MEMORY;
	//}
printf("Mount:RootNode\n============================\n");
	status_t status = B_OK;

	fRootNode = new Inode(this, ToVnode(Root()));  //ToVnode(Root()) = 524288
	if (fRootNode && fRootNode->InitCheck() == B_OK) {
		//printf("  fRootNode && fRootNode->InitCheck() == B_OK\n");
		status = publish_vnode(fID, ToVnode(Root()), (void *)fRootNode);
		if (status == B_OK) {
			printf("  status == B_OK\n");
			// try to get indices root dir

			// question: why doesn't get_vnode() work here??
			// answer: we have not yet backpropagated the pointer to the
			// volume in bfs_mount(), so bfs_read_vnode() can't get it.
			// But it's not needed to do that anyway.

			if (!Indices().IsZero()){
				printf("creating indices Inode\n============================\n");
				fIndicesNode = new Inode(this, ToVnode(Indices()));
			}

			if (fIndicesNode == NULL
				|| fIndicesNode->InitCheck() < B_OK
				|| !fIndicesNode->IsContainer()) {
				INFORM(("bfs: volume doesn't have indices!\n"));

				if (fIndicesNode) {
					// if this is the case, the index root node is gone bad, and
					// BFS switch to read-only mode
					fFlags |= VOLUME_READ_ONLY;
					delete fIndicesNode;
					fIndicesNode = NULL;
				}
			}
			//printf("  all went fine\nroot inode: \n");
			//dump_inode2(&(fRootNode->Node()));
			//printf("index inode: \n");
			//dump_inode2(&(fIndicesNode->Node()));
			// all went fine
			opener.Keep();
			printf("Mounting done\n");
			return B_OK;
		} else{
			printf("  status != B_OK\n");
			FATAL(("could not create root node: publish_vnode() failed!\n"));
		}

		delete fRootNode;
	} else {
		status = B_BAD_VALUE;
		FATAL(("could not create root node!\n"));
	}
	printf("Mounting bfs done\n");
	return status;
}
int Volume::CountSkyfsPartitions(const char* DRIVE,bool skyfs){
	int i,nRet,count=0;
	DWORD dwBytes;
	PARTITION *PartitionTbl;
	DRIVEPACKET stDrive;

	BYTE szSector[512];
	WORD wDrive =0;

	char szTmpStr[64];
	
	DWORD dwMainPrevRelSector =0;
	DWORD dwPrevRelSector =0;

	HANDLE hDrive = CreateFile(DRIVE,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);
	/*
	Status = NtCreateFile(&FileHandle,                      // returned file handle
                          //(GENERIC_WRITE | SYNCHRONIZE),    // desired access
						  FILE_READ_DATA | SYNCHRONIZE,		// desired access
                          &ObjectAttributes,                // ptr to object attributes
                          &Iosb,                            // ptr to I/O status block
                          NULL,                             // allocation size
                          FILE_ATTRIBUTE_NORMAL,            // file attributes
                          FILE_SHARE_READ | FILE_SHARE_WRITE,// share access (was 0)
                          //FILE_SUPERSEDE,                   // create disposition
						  FILE_OPEN,
                          FILE_SYNCHRONOUS_IO_NONALERT,     // create options
                          NULL,                             // ptr to extended attributes
                          0);                               // length of ea buffer
	*/
	if(hDrive == INVALID_HANDLE_VALUE){
		printf("ScanPartitions: CreateFile error: %i\n",GetLastError());
		return GetLastError();
	}

	nRet = ReadFile(hDrive,szSector,512,&dwBytes,0);
	/*
	Status = NtReadFile(FileHandle,	// file Handle
			0,		// event Handle
			NULL,	// APC entry point
			NULL,	// APC context
			&Iosb,	// IOSB address
			q,//Message,// ptr to data buffer
			1024,	// length
			0,		// byte offset
			NULL);	// key
	*/
	if(!nRet){
		printf("ScanPartitions: ReadFile error: %i\n",GetLastError());
		return GetLastError();
	}

	dwPrevRelSector =0;
	dwMainPrevRelSector =0;

	PartitionTbl = (PARTITION*)(szSector+0x1BE); //0x1be=446

	for(i=0; i<4; i++) /// scanning partitions in the physical disk
	{
		stDrive.wCylinder = PartitionTbl->chCylinder;
		stDrive.wHead = PartitionTbl->chHead;
		stDrive.wSector = PartitionTbl->chSector;
		stDrive.dwNumSectors = PartitionTbl->dwNumberSectors;
		stDrive.wType = ((PartitionTbl->chType == PART_EXTENDED) || (PartitionTbl->chType == PART_DOSX13X)) ? EXTENDED_PART:BOOT_RECORD;

		if((PartitionTbl->chType == PART_EXTENDED) || (PartitionTbl->chType == PART_DOSX13X))
		{
			dwMainPrevRelSector			= PartitionTbl->dwRelativeSector;
			stDrive.dwNTRelativeSector	= dwMainPrevRelSector;
		}
		else
		{
			stDrive.dwNTRelativeSector = dwMainPrevRelSector + PartitionTbl->dwRelativeSector;
		}

		if(stDrive.wType == EXTENDED_PART)
			break;

		if(PartitionTbl->chType == 0)
			break;

		switch(PartitionTbl->chType)
		{
			case PART_DOS2_FAT:
				strcpy(szTmpStr, "FAT12");
				break;
			case PART_DOSX13:
			case PART_DOS4_FAT:
			case PART_DOS3_FAT:
				strcpy(szTmpStr, "FAT16");
				break;
			case PART_DOS32X:
			case PART_DOS32:
				strcpy(szTmpStr, "FAT32");			//Normal FAT32
				break;
			case PART_NTFS:
				strcpy(szTmpStr, "NTFS");	// NTFS 
				break;
			case PART_EXT2:
				strcpy(szTmpStr, "EXT2/3");	//ext2 or ext3 partition
				break;
			case PART_SWAP:
				strcpy(szTmpStr, "Linux SWAP");	//linux swap partition
				break;
			case PART_SKYFS:
				strcpy(szTmpStr, "SKYFS");	//skyfs partition
				if (skyfs){ 
					printf("found skyfs partition!\n");
					count++;
				}
				break;
			case PART_BFS:
				strcpy(szTmpStr, "BFS");	//skyfs partition
				if (!skyfs){ 
					printf("found bfs partition!\n");
					count++;
				}
				break;
			default:
				strcpy(szTmpStr, "Unknown");
				break;
		}

		printf("%s Drive %d, %i\n", szTmpStr,wDrive,PartitionTbl->chType);
		PartitionTbl++;
		wDrive++;
	}

	if(i==4)
		return ERROR_SUCCESS;

	for(int LogiHard=0; LogiHard<50; LogiHard++) // scanning extended partitions
	{
		if(stDrive.wType == EXTENDED_PART)
		{
			LARGE_INTEGER n64Pos;

			n64Pos.QuadPart = ((LONGLONG) stDrive.dwNTRelativeSector) * 512;
			
			nRet = SetFilePointer(hDrive, n64Pos.LowPart,&n64Pos.HighPart, FILE_BEGIN);
			if(nRet == 0xffffffff)
				return GetLastError();;

			dwBytes = 0;

			nRet = ReadFile(hDrive, szSector, 512, (DWORD *) &dwBytes, NULL);
			/*
	Status = NtReadFile(FileHandle,	// file Handle
			0,		// event Handle
			NULL,	// APC entry point
			NULL,	// APC context
			&Iosb,	// IOSB address
			q,//Message,// ptr to data buffer
			1024,	// length
			0,		// byte offset
			NULL);	// key
	*/
			if(!nRet)
				return GetLastError();

			if(dwBytes != 512)
				return ERROR_READ_FAULT;
			
			PartitionTbl = (PARTITION *) (szSector+0x1BE);

			for(i=0; i<4; i++)
			{
				stDrive.wCylinder = PartitionTbl->chCylinder;
				stDrive.wHead = PartitionTbl->chHead;
				stDrive.dwNumSectors = PartitionTbl->dwNumberSectors;
				stDrive.wSector = PartitionTbl->chSector;
				stDrive.dwRelativeSector = 0;
				stDrive.wType = ((PartitionTbl->chType == PART_EXTENDED) || (PartitionTbl->chType == PART_DOSX13X)) ? EXTENDED_PART:BOOT_RECORD;
				
				if((PartitionTbl->chType == PART_EXTENDED) || (PartitionTbl->chType == PART_DOSX13X))
				{
					dwPrevRelSector = PartitionTbl->dwRelativeSector;
					stDrive.dwNTRelativeSector = dwPrevRelSector + dwMainPrevRelSector;
				}
				else
				{
					stDrive.dwNTRelativeSector = dwMainPrevRelSector + dwPrevRelSector + PartitionTbl->dwRelativeSector;
				}

				if(stDrive.wType == EXTENDED_PART)
					break;

				if(PartitionTbl->chType == 0)
					break;

				switch(PartitionTbl->chType)
				{
				case PART_DOS2_FAT:
					strcpy(szTmpStr, "FAT12");
					break;
				case PART_DOSX13:
				case PART_DOS4_FAT:
				case PART_DOS3_FAT:
					strcpy(szTmpStr, "FAT16");
					break;
				case PART_DOS32X:
				case PART_DOS32:
					strcpy(szTmpStr, "FAT32");			//Normal FAT32
					break;
				case 7:
					strcpy(szTmpStr, "NTFS");
					break;
				case 131:
					strcpy(szTmpStr, "EXT2/3");	//ext2 or ext3 partition
					break;
				case 130:
					strcpy(szTmpStr, "Linux SWAP");	//linux swap partition
					break;
				case PART_SKYFS:
					strcpy(szTmpStr, "SKYFS");	//skyfs partition
					if (skyfs){
						printf("found skyfs partition!\n");
						count++;
					}
					break;
				case PART_BFS:
					strcpy(szTmpStr, "BFS");	//skyfs partition
					if (!skyfs){
						printf("found bfs partition!\n");
						count++;
					}
					break;
				default:
					strcpy(szTmpStr, "Unknown");
					break;
				}
				printf("%s Drive %d, %i\n", szTmpStr,wDrive,PartitionTbl->chType);
				PartitionTbl++;
				wDrive++;
			}
			if(i==4)
				break;
		}
	}

	CloseHandle(hDrive);
	//NtClose(hDrive);
	/*Status = NtClose(FileHandle);*/
	return count;
}
status_t
Volume::Unmount()
{
	// Unlike in BeOS, we need to put the reference to our root node ourselves
	put_vnode(fID, ToVnode(Root()));

	// This will also flush the log & all blocks to disk
	delete fJournal;
	fJournal = NULL;

	delete fIndicesNode;

	block_cache_delete(fBlockCache, !IsReadOnly());
	//close(fDevice);
	//CloseHandle(fDevice);
	NtClose(fDevice);
	/*Status = NtClose(FileHandle);*/

	return B_OK;
}


status_t
Volume::Sync()
{
	return fJournal->FlushLogAndBlocks();
}


status_t
Volume::ValidateBlockRun(block_run run)
{
	if (run.AllocationGroup() < 0 || run.AllocationGroup() > (int32)AllocationGroups()
		|| run.Start() > (1UL << AllocationGroupShift())
		|| run.length == 0
		|| uint32(run.Length() + run.Start()) > (1UL << AllocationGroupShift())) {
		Panic();
		FATAL(("*** invalid run(%d,%d,%d)\n", (int)run.AllocationGroup(), run.Start(), run.Length()));
		return B_BAD_DATA;
	}
	return B_OK;
}


block_run
Volume::ToBlockRun(off_t block) const
{
	block_run run;
	run.allocation_group = HOST_ENDIAN_TO_BFS_INT32(block >> AllocationGroupShift());
	run.start = HOST_ENDIAN_TO_BFS_INT16(block & ((1LL << AllocationGroupShift()) - 1));
	run.length = HOST_ENDIAN_TO_BFS_INT16(1);
	return run;
}


status_t
Volume::CreateIndicesRoot(Transaction &transaction)
{
	off_t id;
	status_t status = Inode::Create(transaction, NULL, NULL,
		S_INDEX_DIR | S_STR_INDEX | S_DIRECTORY | 0700, 0, 0, &id, &fIndicesNode);
	if (status < B_OK)
		RETURN_ERROR(status);

	fSuperBlock.indices = ToBlockRun(id);
	return WriteSuperBlock();
}


status_t
Volume::AllocateForInode(Transaction &transaction, const Inode *parent, mode_t type, block_run &run)
{
	return fBlockAllocator.AllocateForInode(transaction, &parent->BlockRun(), type, run);
}


status_t
Volume::WriteSuperBlock()
{
	if (write_pos(fDevice, 512, &fSuperBlock, sizeof(disk_super_block)) != sizeof(disk_super_block))
		return B_IO_ERROR;

	return B_OK;
}


void
Volume::UpdateLiveQueries(Inode *inode, const char *attribute, int32 type, const uint8 *oldKey,
	size_t oldLength, const uint8 *newKey, size_t newLength)
{
	if (fQueryLock.Lock() < B_OK)
		return;

	Query *query = NULL;
	while ((query = fQueries.Next(query)) != NULL)
		query->LiveUpdate(inode, attribute, type, oldKey, oldLength, newKey, newLength);

	fQueryLock.Unlock();
}


/** Checks if there is a live query whose results depend on the presence
 *	or value of the specified attribute.
 *	Don't use it if you already have all the data together to evaluate
 *	the queries - it wouldn't safe you anything in this case.
 */

bool
Volume::CheckForLiveQuery(const char *attribute)
{
	// ToDo: check for a live query that depends on the specified attribute
	return true;
}


void
Volume::AddQuery(Query *query)
{
	if (fQueryLock.Lock() < B_OK)
		return;

	fQueries.Add(query);

	fQueryLock.Unlock();
}


void
Volume::RemoveQuery(Query *query)
{
	if (fQueryLock.Lock() < B_OK)
		return;

	fQueries.Remove(query);

	fQueryLock.Unlock();
}


//	#pragma mark -
//	Disk scanning and initialization
char *
get_tupel2(uint32 id)
{
	static unsigned char tupel[5];

	tupel[0] = 0xff & (id >> 24);
	tupel[1] = 0xff & (id >> 16);
	tupel[2] = 0xff & (id >> 8);
	tupel[3] = 0xff & (id);
	tupel[4] = 0;
	for (int16 i = 0;i < 4;i++) {
		if (tupel[i] < ' ' || tupel[i] > 128)
			tupel[i] = '.';
	}

	return (char *)tupel;
}

void dump_block_run2(const char *prefix, const block_run &run)
{
	printf("%s(%d, %d, %d)\n", prefix, (int)run.allocation_group, run.start, run.length);
}
void
dump_data_stream2(const data_stream *stream)
{
	printf("data_stream:\n");
	for (int i = 0; i < NUM_DIRECT_BLOCKS; i++) {
		if (!stream->direct[i].IsZero()) {
			printf("  direct[%02d]                = ",i);
			dump_block_run2("",stream->direct[i]);
		}
	}
	printf("  max_direct_range          = %Lu\n", stream->max_direct_range);

	if (!stream->indirect.IsZero())
		dump_block_run2("  indirect                  = ", stream->indirect);

	printf("  max_indirect_range        = %Lu\n", stream->max_indirect_range);

	if (!stream->double_indirect.IsZero())
		dump_block_run2("  double_indirect           = ", stream->double_indirect);

	printf("  max_double_indirect_range = %Lu\n", stream->max_double_indirect_range);
	printf("  size                      = %Lu\n", stream->size);
}

status_t
Volume::Identify(HANDLE fd, disk_super_block *superBlock)
{
	debug<<"Volume::Identify\n";
	char buffer[1024];
	printf("Reading superblock\n");
	if (read_pos(fd,0, buffer, sizeof(buffer)) != sizeof(buffer))
		return B_IO_ERROR;
	debug<<"Volume::Identify: read_pos=ok\n";
	// Note: that does work only for x86, for PowerPC, the super block
	// may be located at offset 0!
	memcpy(superBlock, buffer + 512, sizeof(disk_super_block));

	printf("disk_super_block:\n");
	printf("  name           = %s\n", superBlock->name);
	printf("  magic1         = %#08x (%s) %s\n", (int)superBlock->magic1, get_tupel2(superBlock->magic1), ((superBlock->magic1 == SUPER_BLOCK_MAGIC1) || (superBlock->magic1 == BFS_SUPER_BLOCK_MAGIC1)? "valid" : "INVALID"));
	printf("  fs_byte_order  = %#08x (%s)\n", (int)superBlock->fs_byte_order, get_tupel2(superBlock->fs_byte_order));
	printf("  block_size     = %u\n", (unsigned)superBlock->block_size);
	printf("  block_shift    = %u\n", (unsigned)superBlock->block_shift);
	printf("  num_blocks     = %Lu\n", superBlock->num_blocks);
	printf("  used_blocks    = %Lu\n", superBlock->used_blocks);
	printf("  inode_size     = %u\n", (unsigned)superBlock->inode_size);
	printf("  magic2         = %#08x (%s) %s\n", (int)superBlock->magic2, get_tupel2(superBlock->magic2), ((superBlock->magic2 == (int)SUPER_BLOCK_MAGIC2) || (superBlock->magic2 == BFS_SUPER_BLOCK_MAGIC2) ? "valid" : "INVALID"));
	printf("  blocks_per_ag  = %u\n", (unsigned)superBlock->blocks_per_ag);
	printf("  ag_shift       = %u (%ld bytes)\n", (unsigned)superBlock->ag_shift, 1L << superBlock->ag_shift);
	printf("  num_ags        = %u\n", (unsigned)superBlock->num_ags);
	printf("  flags          = %#08x (%s)\n", (int)superBlock->flags, get_tupel2(superBlock->flags));
	dump_block_run2("  log_blocks     = ", superBlock->log_blocks);
	printf("  log_start      = %Lu\n", superBlock->log_start);
	printf("  log_end        = %Lu\n", superBlock->log_end);
	printf("  magic3         = %#08x (%s) %s\n", (int)superBlock->magic3, get_tupel2(superBlock->magic3), ((superBlock->magic3 == SUPER_BLOCK_MAGIC3) || (superBlock->magic3 == BFS_SUPER_BLOCK_MAGIC3) ? "valid" : "INVALID"));
	dump_block_run2("  root_dir       = ", superBlock->root_dir);
	dump_block_run2("  indices        = ", superBlock->indices);

	debug<<"disk_super_block:\n";
	debug<<"  name           = "<<superBlock->name<<std::endl;
	debug<<"  magic1         = "<<(int)superBlock->magic1<<" "<<get_tupel2(superBlock->magic1)<<"  "<< ((superBlock->magic1 == SUPER_BLOCK_MAGIC1) || (superBlock->magic1 == BFS_SUPER_BLOCK_MAGIC1)? "valid" : "INVALID")<<std::endl;
	debug<<"  fs_byte_order  = "<< (int)superBlock->fs_byte_order<<"  "<<get_tupel2(superBlock->fs_byte_order)<<std::endl;
	debug<<"  block_size     = "<<(unsigned)superBlock->block_size<<std::endl;
	debug<<"  block_shift    = "<<(unsigned)superBlock->block_shift<<std::endl;
	debug<<"  num_blocks     = "<<superBlock->num_blocks<<std::endl;
	debug<<"  used_blocks    = "<<superBlock->used_blocks<<std::endl;
	debug<<"  inode_size     = "<<(unsigned)superBlock->inode_size<<std::endl;
	debug<<"  magic2         = "<<(int)superBlock->magic2<<"  "<<get_tupel2(superBlock->magic2)<<"  "<<((superBlock->magic2 == (int)SUPER_BLOCK_MAGIC2) || (superBlock->magic2 == BFS_SUPER_BLOCK_MAGIC2) ? "valid" : "INVALID")<<std::endl;
	debug<<"  blocks_per_ag  = "<<(unsigned)superBlock->blocks_per_ag<<std::endl;
	debug<<"  ag_shift       = "<<(unsigned)superBlock->ag_shift<<"  "<<(1L << superBlock->ag_shift)<<" bytes\n";
	debug<<"  num_ags        = "<<(unsigned)superBlock->num_ags<<std::endl;
	debug<<"  flags          = "<<(int)superBlock->flags<<"  "<<get_tupel2(superBlock->flags)<<std::endl;
	//dump_block_run2("  log_blocks     = ", superBlock->log_blocks<<std::endl;
	debug<<"  log_blocks     = ("<<(int)superBlock->log_blocks.allocation_group<<", "<<superBlock->log_blocks.start<<", "<<superBlock->log_blocks.length<<")\n";
	debug<<"  log_start      = "<<superBlock->log_start<<std::endl;
	debug<<"  log_end        = "<<superBlock->log_end<<std::endl;
	debug<<"  magic3         = "<<(int)superBlock->magic3<<"  "<<get_tupel2(superBlock->magic3)<<"  "<<((superBlock->magic3 == SUPER_BLOCK_MAGIC3) || (superBlock->magic3 == BFS_SUPER_BLOCK_MAGIC3) ? "valid" : "INVALID")<<std::endl;
	//dump_block_run2("  root_dir       = ", superBlock->root_dir);
	debug<<"  root_dir       = ("<<(int)superBlock->root_dir.allocation_group<<", "<<superBlock->root_dir.start<<", "<<superBlock->root_dir.length<<")\n";
	//dump_block_run2("  indices        = ", superBlock->indices);
	debug<<"  indices        = ("<<(int)superBlock->indices.allocation_group<<", "<<superBlock->indices.start<<", "<<superBlock->indices.length<<")\n";

	if (!superBlock->IsValid()) {
#ifndef BFS_LITTLE_ENDIAN_ONLY
		memcpy(superBlock, buffer, sizeof(disk_super_block));
		if (!superBlock->IsValid())
			return B_BAD_VALUE;
#else
		debug<<"Volume::Identify: not valid.\n";
		return B_BAD_VALUE;
#endif
	}
	debug<<"Volume::Identify: valid.\n";
	return B_OK;
}


status_t
Volume::Initialize(const char *device, const char *name, uint32 blockSize,
	uint32 flags)
{
	// although there is no really good reason for it, we won't
	// accept '/' in disk names (mkbfs does this, too - and since
	// Tracker names mounted volumes like their name)
	if (strchr(name, '/') != NULL)
		return B_BAD_VALUE;

	if (blockSize != 1024 && blockSize != 2048 && blockSize != 4096 && blockSize != 8192)
		return B_BAD_VALUE;

	DeviceOpener opener(device, skyfsNr, O_RDWR);
	//if (opener.Device() < B_OK)
	if (opener.Device() ==INVALID_HANDLE_VALUE)
		return B_BAD_VALUE;

	fDevice = opener.Device();

	uint32 deviceBlockSize;
	off_t deviceSize;
	if (opener.GetSize(&deviceSize, &deviceBlockSize) < B_OK)
		return B_ERROR;

	off_t numBlocks = deviceSize / blockSize;

	// create valid super block

	fSuperBlock.Initialize(name, numBlocks, blockSize);
	
	// initialize short hands to the super block (to save byte swapping)
	fBlockSize = fSuperBlock.BlockSize();
	fBlockShift = fSuperBlock.BlockShift();
	fAllocationGroupShift = fSuperBlock.AllocationGroupShift();

	// since the allocator has not been initialized yet, we
	// cannot use BlockAllocator::BitmapSize() here
	fSuperBlock.log_blocks = ToBlockRun(AllocationGroups()
		* fSuperBlock.BlocksPerAllocationGroup() + 1);
	fSuperBlock.log_blocks.length = HOST_ENDIAN_TO_BFS_INT16(2048);
		// ToDo: set the log size depending on the disk size
	fSuperBlock.log_start = fSuperBlock.log_end = HOST_ENDIAN_TO_BFS_INT64(ToBlock(Log()));

	// set the current log pointers, so that journaling will work correctly
	fLogStart = fSuperBlock.LogStart();
	fLogEnd = fSuperBlock.LogEnd();

	if (!IsValidSuperBlock())
		RETURN_ERROR(B_ERROR);

	if ((fBlockCache = opener.InitCache(NumBlocks(), fBlockSize)) == NULL)
		return B_ERROR;

	fJournal = new Journal(this);
	if (fJournal == NULL || fJournal->InitCheck() < B_OK)
		RETURN_ERROR(B_ERROR);

	// ready to write data to disk

	Transaction transaction(this, 0);

	if (fBlockAllocator.InitializeAndClearBitmap(transaction) < B_OK)
		RETURN_ERROR(B_ERROR);

	off_t id;
	status_t status = Inode::Create(transaction, NULL, NULL,
		S_DIRECTORY | 0755, 0, 0, &id, &fRootNode);
	if (status < B_OK)
		RETURN_ERROR(status);

	fSuperBlock.root_dir = ToBlockRun(id);

	if ((flags & VOLUME_NO_INDICES) == 0) {
		// The indices root directory will be created automatically
		// when the standard indices are created (or any other).
		Index index(this);
		status = index.Create(transaction, "name", B_STRING_TYPE);
		if (status < B_OK)
			return status;

		status = index.Create(transaction, "last_modified", B_INT64_TYPE);
		if (status < B_OK)
			return status;

		status = index.Create(transaction, "size", B_INT64_TYPE);
		if (status < B_OK)
			return status;
	}

	WriteSuperBlock();
	transaction.Done();

// 	put_vnode(ID(), fRootNode->ID());
// 	if (fIndicesNode != NULL)
// 		put_vnode(ID(), fIndicesNode->ID());

	Sync();
	opener.RemoveCache(true);
	return B_OK;
}
