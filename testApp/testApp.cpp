//#include <commctrl.h>
#define _WIN32_WINNT 0x0500
extern "C"{
	#include <windows.h>
	#include <winioctl.h>
	#include <stdio.h>
}

#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
using namespace std;
	
const int DRIVE_A = 0x1;
const int DRIVE_B = 0x2;
const int DRIVE_C = 0x4;
const int DRIVE_D = 0x8;
const int DRIVE_E = 0x10;



//#include "stdafx.h"

// #include <diskguid.h>
const GUID PARTITION_LDM_DATA_GUID = {0xAF9B60A0L, 0x1431, 0x4F62, 0xBC,
0x68, 0x33, 0x11, 0x71, 0x4A, 0x69, 0xAD}; // Logical Disk Manager data partition


#define PHYSICALDRIVE TEXT("PhysicalDrive")

void ErrorExit(LPTSTR lpszFunction)
{
	LPVOID lpMsgBuf;
	DWORD dw = GetLastError();

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		dw,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf,
		0, NULL );

	wprintf(TEXT("%s failed with error %d: %s\n"),lpszFunction, dw, lpMsgBuf);

	LocalFree(lpMsgBuf);
	ExitProcess(dw);
}

bool IsPartitionDynamic(PARTITION_INFORMATION_EX *pPie)
{
	bool ret = false;
	if (pPie->PartitionStyle == PARTITION_STYLE_MBR)
	{
		ret = pPie->Mbr.PartitionType == PARTITION_LDM;
	} else if (pPie->PartitionStyle == PARTITION_STYLE_GPT)
	{
		ret = IsEqualGUID(pPie->Gpt.PartitionType, PARTITION_LDM_DATA_GUID);
	}
	return ret;
}

bool IsDiskDynamic(int nDiskNo)
{
	bool ret = false;
	TCHAR szDiskPath[MAX_PATH];
	wsprintf(szDiskPath, TEXT("\\\\.\\%s%u"), PHYSICALDRIVE, nDiskNo);
	HANDLE hDisk = CreateFile(szDiskPath, GENERIC_READ, FILE_SHARE_READ |
		FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if (hDisk != INVALID_HANDLE_VALUE) {
		DRIVE_LAYOUT_INFORMATION_EX *pInfo;
		DWORD dwBytesReturn = 0;
		int estimatedPartitionCount = 4;
	loop:
		DWORD dwSize = sizeof(DRIVE_LAYOUT_INFORMATION_EX) +
			estimatedPartitionCount * sizeof(PARTITION_INFORMATION_EX);
		pInfo = (DRIVE_LAYOUT_INFORMATION_EX *) new BYTE[dwSize];
		if (DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0,
			(LPVOID) pInfo,
			dwSize,
			&dwBytesReturn,
			NULL))
		{
			for (DWORD i = 0; i < pInfo->PartitionCount; i++)
			{
				if (IsPartitionDynamic(pInfo->PartitionEntry + i))
				{
					ret = true;
					break;
				}
			}
		} else {
			if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				estimatedPartitionCount *= 2;
				delete pInfo;
				goto loop;
			} else {
				ErrorExit(TEXT("DeviceIoControl"));
			}
		}
		CloseHandle(hDisk);
		delete pInfo;
	} else {
		ErrorExit(TEXT("CreateFile"));
	}
	return ret;
}


int main(int argc, char **argv)
{
	cout << "hello world!"<<endl;



DWORD drives = GetLogicalDrives();

if (drives & DRIVE_A)
  // There is a drive A
  cout<< "Drive A"<<endl;

if (drives & DRIVE_B)
  // There is a drive B 
  cout<< "Drive B"<<endl;

if (drives & DRIVE_C)
  // There is a drive B 
  cout<< "Drive C"<<endl;




wprintf(TEXT("Disk 0 is %s disk.\n"), IsDiskDynamic(0) ? TEXT("Dynamic") : TEXT("Basic"));


string a;
cin >> a;
	return 0;
}