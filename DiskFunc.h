#ifndef DISKFUNC_H
#define DISKFUNC_H

// skyfs include files
#define _CRT_SECURE_NO_DEPRECATE 1
#include <iostream>
#include <fstream>
#include <vector>
#include "resource\resource.h"


#include "Volume.h"
#include "Inode.h"
#include "BPlusTree.h"
#include "SkyfsInterface.h"
#include "System.h"
#include <commctrl.h>


using namespace std;

#define ICON_TREE	1
#define ICON_FILE	2
#define ICON_OTHER	3
#define ICON_DISK	4

int countPhysicalDrives();
int listPartitions(int disk, HWND h, HTREEITEM * node, TV_INSERTSTRUCT * s,std::ofstream* debug);
int listDir(HWND h, HTREEITEM * node, TV_ITEM * s, ofstream * d, vector<Volume>* volumes);

class TreeData{
	public:
		TreeData(){}
		TreeData(int l,const char* n):level(l){
			sprintf(name,n);
		}
		TreeData(int l,const char* n,off_t i):level(l),inode(i){
			sprintf(name,n);
		}

		~TreeData(){ 
			if (name) free(name); 
			if (extra) free(extra);
		}
		
		int level;
		int iData1;
		int iData2;
		int volume;
		off_t inode;
		char name[255];
		void* extra;
};
int addChild(HWND hWnd,HTREEITEM* parent, char* t1,int icon,TreeData* td,std::ofstream*);

#endif