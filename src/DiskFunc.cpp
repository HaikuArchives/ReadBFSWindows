/* Author: Peter Speybrouck - peter.speybrouck@gmail.com
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file. As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return. Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */



#include "DiskFunc.h"
#include <sstream>
using namespace std;

int countPhysicalDrives(){
	int index=0;

	HANDLE hDevice; // handle to the drive to be examined
	char drive[60];
	
	sprintf(drive,"\\\\.\\PhysicalDrive%i",index);
	hDevice = CreateFile(drive, // drive to open
			0, // don't need any access to the drive
			FILE_SHARE_READ | FILE_SHARE_WRITE, // share mode
			NULL, // default security attributes
			OPEN_EXISTING, // disposition
			0, // file attributes
			NULL); // don't copy any file's attributes
	
	if (hDevice==INVALID_HANDLE_VALUE) return 0;
	while (hDevice != INVALID_HANDLE_VALUE){
		CloseHandle(hDevice);
		index++;
		sprintf(drive,"\\\\.\\PhysicalDrive%i",index);
		hDevice = CreateFile(drive, // drive to open
			0, // don't need any access to the drive
			FILE_SHARE_READ | FILE_SHARE_WRITE, // share mode
			NULL, // default security attributes
			OPEN_EXISTING, // disposition
			0, // file attributes
			NULL); // don't copy any file's attributes
	}

	return index;
}

int listDir(HWND hWnd, HTREEITEM * node, TV_ITEM * tvi,ofstream* debug, vector<Volume>* volumes){
	void *iter;
	dirent d;
	int children=0;
	uint32 num;
	TreeData* td = reinterpret_cast<TreeData*>(tvi->lParam);
	Volume* vv=(Volume*)td->extra;
	Volume* v=&(Volume)volumes->at(td->volume);
	*debug<<"ListDir: "<<tvi->pszText<<" volume="<<v<<" vol.Name="<<v->Name()<<"   hWnd="<<hWnd<<endl;	debug->flush();
	//if (vv->IsValidSuperBlock()) *debug<<"2: VALID SUPERBLOCK "<<vv<<"  td->volume="<<td->volume<<endl;
	//else *debug<<"2: NO VALID SUPERBLOCK!!! "<<v<<"  td->volume="<<td->volume<<endl;
	debug->flush();
	if (v->IsValidSuperBlock()) *debug<<"4: VALID SUPERBLOCK "<<v<<endl;
	else *debug<<"4: NO VALID SUPERBLOCK!!! "<<v<<endl;
	debug->flush();
	Inode *II;
	if (td->inode!=-1)
		II = new Inode(v,td->inode);
	else //we're reading the root directory
		II = v->RootNode();
	//skyfs_open_dir(v, v->RootNode(), &iter);
	if (II->IsDirectory()){ 
		skyfs_open_dir(v, II, &iter);
		*debug<<"after opendir...  inode: "<<td->inode<<endl;debug->flush();
		// second arg of skyfs_read_dir is not used actually
		//status_t ss = skyfs_read_dir(v, v->RootNode(), iter, &d,sizeof(dirent), &num);
		status_t ss = skyfs_read_dir(v, II, iter, &d,sizeof(dirent), &num);
		*debug<<"after readdir..."<<endl;debug->flush();
		//TODO: this should be the '.' entry...
		if (ss==B_OK && num>0) *debug<<d.d_name<<endl;
		debug->flush();
		while (ss==B_OK && num>0){
			*debug<<"before readdir\n";debug->flush();
			ss = skyfs_read_dir(v, v->RootNode(), iter, &d,sizeof(dirent), &num);
			if (ss==B_OK && num>0){ 
				//debug<<"readdir success\n";debug.flush();
				//printf("dirent: %s, len=%i, ino=%i\n",d.d_name,d.d_reclen,d.d_ino);
				Inode *I = new Inode(v,d.d_ino);
				if (I->IsDirectory()){ 
					*debug<<d.d_name<<"(dir)"<<endl;debug->flush();
					TreeData* data = new TreeData(td->level+1,d.d_name,d.d_ino);
					data->extra=v;
					data->volume=td->volume;
					addChild(hWnd,node, d.d_name,ICON_TREE,data,debug);
					children++;
				}
				if (I->IsFile()){ 
					*debug<<d.d_name<<"(file)"<<endl;debug->flush();
					TreeData* data = new TreeData(2,d.d_name,d.d_ino);
					data->extra=v;
					data->volume=td->volume;
					addChild(hWnd,node, d.d_name,ICON_FILE,data,debug);
					children++;
				}
				if (!I->IsFile() && !I->IsDirectory()){ 
					*debug<<d.d_name<<"(?)"<<endl;debug->flush();
					TreeData* data = new TreeData(2,d.d_name,d.d_ino);
					data->extra=v;
					data->volume=td->volume;
					addChild(hWnd,node, d.d_name,ICON_OTHER,data,debug);
					children++;
				}
				free(I);
			}
			//debug<<"at end while loop\n"; debug.flush();
		}
	}
	return children;
}

int addChild(HWND hWnd,HTREEITEM* parent, char* t1,int icon,TreeData* td, std::ofstream *debug){
	*debug<<"addChild: hWnd="<<hWnd<<endl;
	*debug<<"addChild: parent="<<*parent<<", level="<<td->level<<endl;
	TV_INSERTSTRUCT tvinsert;   // struct to config out tree control
	if (strcmp(t1,"..")!=0 && strcmp(t1,".")!=0){
		*debug<<"AddChild: "<<t1<<" volume="<<td->extra<<endl;
		tvinsert.hParent=*parent;	
		tvinsert.hInsertAfter=TVI_LAST; 
		tvinsert.item.mask=TVIF_TEXT|TVIF_IMAGE|TVIF_SELECTEDIMAGE|TVIF_PARAM;
		tvinsert.item.pszText=t1;
		if (td)
			tvinsert.item.lParam=reinterpret_cast<LPARAM>(td); //TreeData pointer
		if (icon==ICON_TREE){
			tvinsert.item.iImage=0;
			tvinsert.item.iSelectedImage=1;
		}
		if (icon==ICON_FILE){
			tvinsert.item.iImage=2;
			tvinsert.item.iSelectedImage=2;
		}
		//SendDlgItemMessage(hWnd,IDC_TREE1,TVM_INSERTITEM,0,(LPARAM)&tvinsert);
		TreeView_InsertItem(hWnd, (LPARAM)&tvinsert);
	}
	else *debug<<"AddChild: ignoring '.' or '..'"<<endl;
	return 0;
}
int listPartitions(int disk, HWND h, HTREEITEM * node, TV_INSERTSTRUCT * s, std::ofstream *debug){
	
	int i,nRet,count=0;
	DWORD dwBytes;
	PARTITION *PartitionTbl;
	DRIVEPACKET stDrive;

	BYTE szSector[512];
	WORD wDrive =0;

	char szTmpStr[64];
	
	DWORD dwMainPrevRelSector =0;
	DWORD dwPrevRelSector =0;
	char DRIVE[60];
	sprintf(DRIVE,"\\\\.\\PhysicalDrive%i",disk);
	HANDLE hDrive = CreateFile(DRIVE,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);
	
	if(hDrive == INVALID_HANDLE_VALUE){
		printf("\tError: could not open disk %i (error %i)\n",disk,GetLastError());
		*debug<<"Error: could not open disk "<<disk<<", "<<GetLastError()<<endl;
		return GetLastError();
	}

	nRet = ReadFile(hDrive,szSector,512,&dwBytes,0);
	
	if(!nRet){
		printf("\tRead error: %i\n",GetLastError());
		*debug<<"Read error: "<<GetLastError()<<endl;
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
			{
				strcpy(szTmpStr, "SKYFS");	//skyfs partition
				s->hParent=*node;
				s->item.mask=TVIF_TEXT|TVIF_IMAGE|TVIF_SELECTEDIMAGE| TVIF_PARAM;
				s->item.lParam=0;
				ostringstream oss;
				oss << "Partition" << wDrive;
				string t = oss.str();
				TreeData* td = new TreeData(1,t.c_str());
				td->iData1 = disk; 
				td->iData2 = wDrive;   // save partition number in an int
				s->item.lParam=reinterpret_cast<LPARAM>(td);
				oss << " ("<< szTmpStr<< ")";
				t = oss.str();
				s->item.pszText=(LPTSTR)(LPCTSTR)t.c_str();
				SendDlgItemMessage(h,IDC_TREE1,TVM_INSERTITEM,0,(LPARAM)s);
			}
				break;
			case PART_BFS:
				{
				strcpy(szTmpStr, "BFS");	//bfs partition
				s->hParent=*node;
				s->item.mask=TVIF_TEXT|TVIF_IMAGE|TVIF_SELECTEDIMAGE| TVIF_PARAM;
				s->item.lParam=0;
				ostringstream oss;
				oss << "Partition" << wDrive;
				string t = oss.str();
				TreeData* td = new TreeData(1,t.c_str());
				td->iData1 = disk; 
				td->iData2 = wDrive;   // save partition number in an int
				s->item.lParam=reinterpret_cast<LPARAM>(td);
				oss << " ("<< szTmpStr<< ")";
				t = oss.str();
				s->item.pszText=(LPTSTR)(LPCTSTR)t.c_str();
				SendDlgItemMessage(h,IDC_TREE1,TVM_INSERTITEM,0,(LPARAM)s);
			}
				break;
			case PART_BFS1:
			{
				strcpy(szTmpStr, "BFS?");	//bfs partition
				s->hParent=*node;
				s->item.mask=TVIF_TEXT|TVIF_IMAGE|TVIF_SELECTEDIMAGE| TVIF_PARAM;
				s->item.lParam=0;
				ostringstream oss;
				oss << "Partition" << wDrive;
				string t = oss.str();
				TreeData* td = new TreeData(1,t.c_str());
				td->iData1 = disk; 
				td->iData2 = wDrive;   // save partition number in an int
				s->item.lParam=reinterpret_cast<LPARAM>(td);
				oss << " ("<< szTmpStr<< ")";
				t = oss.str();
				s->item.pszText=(LPTSTR)(LPCTSTR)t.c_str();
				SendDlgItemMessage(h,IDC_TREE1,TVM_INSERTITEM,0,(LPARAM)s);
			}
				break;
			default:
				strcpy(szTmpStr, "Unknown");
				break;
		}

		printf("\t[%d] %s Drive, Primary (0x%x)\n", wDrive,szTmpStr,PartitionTbl->chType);
		*debug<<"\t["<<wDrive<<"] "<<szTmpStr<<" Drive, Primary ("<<(int)PartitionTbl->chType<<")"<<endl;
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
				{
					strcpy(szTmpStr, "SKYFS");	//skyfs partition
					s->hParent=*node;
					s->item.mask=TVIF_TEXT|TVIF_IMAGE|TVIF_SELECTEDIMAGE| TVIF_PARAM;
					s->item.lParam=0;
					ostringstream oss;
					oss << "Partition" << wDrive;
					string t = oss.str();
					TreeData* td = new TreeData(1,t.c_str());
					td->iData1 = disk; 
					td->iData2 = wDrive;   // save partition number in an int
					s->item.lParam=reinterpret_cast<LPARAM>(td);
					oss << " ("<< szTmpStr<< ")";
					t = oss.str();
					s->item.pszText=(LPTSTR)(LPCTSTR)t.c_str();
					SendDlgItemMessage(h,IDC_TREE1,TVM_INSERTITEM,0,(LPARAM)s);
				}
					break;
				case PART_BFS:
				{
					strcpy(szTmpStr, "BFS");	//bfs partition
					s->hParent=*node;
					s->item.mask=TVIF_TEXT|TVIF_IMAGE|TVIF_SELECTEDIMAGE| TVIF_PARAM;
					s->item.lParam=0;
					ostringstream oss;
					oss << "Partition" << wDrive;
					string t = oss.str();
					TreeData* td = new TreeData(1,t.c_str());
					td->iData1 = disk; 
					td->iData2 = wDrive;   // save partition number in an int
					s->item.lParam=reinterpret_cast<LPARAM>(td);
					oss << " ("<< szTmpStr<< ")";
					t = oss.str();
					s->item.pszText=(LPTSTR)(LPCTSTR)t.c_str();
					SendDlgItemMessage(h,IDC_TREE1,TVM_INSERTITEM,0,(LPARAM)s);
				}
					break;
				case PART_BFS1:
				{
					strcpy(szTmpStr, "BFS?");	//bfs partition
					s->hParent=*node;
					s->item.mask=TVIF_TEXT|TVIF_IMAGE|TVIF_SELECTEDIMAGE| TVIF_PARAM;
					s->item.lParam=0;
					ostringstream oss;
					oss << "Partition" << wDrive;
					string t = oss.str();
					TreeData* td = new TreeData(1,t.c_str());
					td->iData1 = disk; 
					td->iData2 = wDrive;   // save partition number in an int
					s->item.lParam=reinterpret_cast<LPARAM>(td);
					oss << " ("<< szTmpStr<< ")";
					t = oss.str();
					s->item.pszText=(LPTSTR)(LPCTSTR)t.c_str();
					SendDlgItemMessage(h,IDC_TREE1,TVM_INSERTITEM,0,(LPARAM)s);
				}
					break;
				default:
					strcpy(szTmpStr, "Unknown");
					break;
				}
				printf("\t[%d] %s Drive, Logical (0x%x)\n", wDrive,szTmpStr,PartitionTbl->chType);
				*debug<<"\t["<<wDrive<<"] "<<szTmpStr<<" Drive, Logical ("<<(int)PartitionTbl->chType<<")"<<endl;
				PartitionTbl++;
				wDrive++;
			}
			if(i==4)
				break;
		}
	}

	CloseHandle(hDrive);
	return count;
}