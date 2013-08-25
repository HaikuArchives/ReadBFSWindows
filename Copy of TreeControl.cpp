#include <windows.h>
#include "resource\resource.h"
#include <commctrl.h>

#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include "DiskFunc.h"

// to fix LNK2005, linker options -> ignore libraries: LIBCD.lib ntdll.lib
// then additional dependancies: comctl32.lib LIBCD.lib ntdll.lib

using namespace std;

//======================Handles================================================//
HINSTANCE hInst; // main function handler
#define WIN32_LEAN_AND_MEAN // this will assume smaller exe
#define ICON_TREE	1
#define ICON_FILE	2
#define ICON_OTHER	3
TV_ITEM tvi;
HTREEITEM Selected;
TV_INSERTSTRUCT tvinsert;   // struct to config out tree control
HTREEITEM Parent;           // Tree item handle
HTREEITEM Before;           // .......
HTREEITEM Root;             // .......
HIMAGELIST hImageList;      // Image list array hadle
HBITMAP hBitMap;            // bitmap handler
bool flagSelected=false;

// for drag and drop
HWND hTree;
TVHITTESTINFO tvht; 
HTREEITEM hitTarget;
POINTS Pos;
bool Dragging;

// for lable editing
HWND hEdit;
// for debug output
ofstream debug;
// volume mounting stuff
int mountID=1;
vector<Volume> volumes;
void *iter;
void *it;
dirent d,dd;
uint32 num,num2;

//function prototypes:
int addChild(HWND hWnd,HTREEITEM* parent, char* t1,int icon,TreeData* td);
//====================MAIN DIALOG==========================================//
BOOL CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) // what are we doing ?
	{ 	 
		// This Window Message is the heart of the dialog //
		//================================================//
		case WM_INITDIALOG: 
		{
			debug.open ("debuglog.txt",ios::app|ios::out);
			debug << "Logging started\n";
			debug.flush();

			InitCommonControls();	    // make our tree control to work
			hTree=GetDlgItem(hWnd,IDC_TREE1);
			// creating image list and put it into the tree control
			//====================================================//
			hImageList=ImageList_Create(16,16,ILC_COLOR16,2,10);					  // Macro: 16x16:16bit with 2 pics [array]
			hBitMap=LoadBitmap(hInst,MAKEINTRESOURCE(IDB_TREE));					  // load the picture from the resource
			ImageList_Add(hImageList,hBitMap,NULL);								      // Macro: Attach the image, to the image list
			DeleteObject(hBitMap);													  // no need it after loading the bitmap
		    SendDlgItemMessage(hWnd,IDC_TREE1,TVM_SETIMAGELIST,0,(LPARAM)hImageList); // put it onto the tree control
			
			string strItem;
			int cd = countPhysicalDrives();
			debug<<"drives: "<< cd<<endl;
			for (int i=0;i<cd;i++){
				tvinsert.hParent=NULL;			// top most level no need handle
				tvinsert.hInsertAfter=TVI_ROOT; // work as root level
				tvinsert.item.mask=TVIF_TEXT|TVIF_IMAGE|TVIF_SELECTEDIMAGE|TVIF_PARAM;
				ostringstream oss;
				oss << "Device" << i;
				strItem=oss.str();//"Device"+i;

				TreeData* t=new TreeData(0,strItem.c_str());
				t->iData1=i; // save disk number
				tvinsert.item.lParam=reinterpret_cast<LPARAM>(t); //TreeData pointer
								
				tvinsert.item.pszText=(LPTSTR)(LPCTSTR)strItem.c_str();
				tvinsert.item.iImage=0;
				tvinsert.item.iSelectedImage=1;
				
				Parent=(HTREEITEM)SendDlgItemMessage(hWnd,IDC_TREE1,TVM_INSERTITEM,0,(LPARAM)&tvinsert);
				Root=Parent;
				listPartitions(i, hWnd, &Parent, &tvinsert);
			}	
		}
		break;

		case WM_LBUTTONDOWN: 
		{
			ReleaseCapture(); 
			SendMessage(hWnd,WM_NCLBUTTONDOWN,HTCAPTION,0); 
		}
		break;


		case WM_NOTIFY:
		{
		    case IDC_TREE1:
            
			if(((LPNMHDR)lParam)->code == NM_DBLCLK) // if code == NM_CLICK - Single click on an item
			{
				debug<<"DOUBLE CLICK! => Copy file to local dir if this is a file\n";debug.flush();
				char Text[255]="";
				memset(&tvi,0,sizeof(tvi));

				Selected=(HTREEITEM)SendDlgItemMessage(hWnd,IDC_TREE1,TVM_GETNEXTITEM,TVGN_CARET,(LPARAM)Selected);
				
				if(Selected==NULL)
				{
					MessageBox(hWnd,"No Items in TreeView","Error",MB_OK|MB_ICONINFORMATION);
					break;
				}
				TreeView_EnsureVisible(hWnd,Selected);
			    SendDlgItemMessage(hWnd,IDC_TREE1,TVM_SELECTITEM,TVGN_CARET,(LPARAM)Selected);
				flagSelected=true;
				tvi.mask=TVIF_TEXT|TVIF_PARAM|TVIF_CHILDREN;
				tvi.pszText=Text;
				tvi.cchTextMax=256;
				tvi.hItem=Selected;
				
							
				if(SendDlgItemMessage(hWnd,IDC_TREE1,TVM_GETITEM,TVGN_CARET,(LPARAM)&tvi))
				{
					TreeData* td = reinterpret_cast<TreeData*>(tvi.lParam);
					if (td){ 
						debug<<"Node Level="<<td->level<<" name="<<td->name<<" inode="<<td->inode<<endl;
						debug.flush();
						Volume* vol=(Volume*)td->extra;
						Inode *II = new Inode(vol,td->inode);
						debug<<"\tInode size: "<<II->Size()<<endl;
						if (II->IsFile()){ 
							debug<<"This is a FILE!!! go get it\n";debug.flush();
							Inode *I;
							void *f; // file_cookie
							file_cookie ff;
							//printf("skyfs_read_vnode result: %i\n",skyfs_read_vnode(v,135901,(void**)&I,true /*not used*/));
							debug<<"skyfs_read_vnode result: "<<skyfs_read_vnode(vol,td->inode,(void**)&I,true /*not used*/)<<endl;
							debug<<"open status: "<<skyfs_open(vol,I,O_RDONLY,&f)<<endl;;
							memcpy(&ff,f,sizeof(file_cookie));
							debug<<"filesize: "<<ff.last_size<<endl;
							char * q = (char *)calloc(ff.last_size,sizeof(char));
							//uint32 l = 1024;//(struct file_cookie)f->last_size;
							debug<<"read status: "<<skyfs_read(vol,I,f,0,q,&ff.last_size)<<endl;
							//for (int i=0; i<ff.last_size;i++) printf("%c",q[i]);
							skyfs_close(vol,I,&ff);
							ofstream file;
							file.open(td->name,ios::trunc|ios::out|ios::binary);
							if (file.is_open()) {
								/* ok, proceed with output */ 
								file.write(q,ff.last_size);
								file.close();
								ostringstream oss;
								oss << "File copied to local directory: " << td->name;
								string str=oss.str();
								MessageBox(hWnd,str.c_str(),"File copied",MB_OK|MB_ICONINFORMATION);
							}
						}
						free(II);
					}
				}
			}

			if(((LPNMHDR)lParam)->code == NM_CLICK) // if code == NM_CLICK - Single click on an item
				/*don't do this in CLICK but in 
				TVN_SELCHANGING: 
					pnmtv = (LPNMTREEVIEW) lParam 
					http://msdn2.microsoft.com/en-us/library/ms650152.aspx */
			//if(((LPNMHDR)lParam)->code == TVN_SELCHANGING)
			{
				//NMTREEVIEW * pnmtv = (LPNMTREEVIEW) lParam;
				char Text[255]="";
				memset(&tvi,0,sizeof(tvi));

				Selected=(HTREEITEM)SendDlgItemMessage(hWnd,IDC_TREE1,TVM_GETNEXTITEM,TVGN_CARET,(LPARAM)Selected);
				
				if(Selected==NULL)
				{
					MessageBox(hWnd,"No Items in TreeView","Error",MB_OK|MB_ICONINFORMATION);
					break;
				}
				TreeView_EnsureVisible(hWnd,Selected);
			    SendDlgItemMessage(hWnd,IDC_TREE1,TVM_SELECTITEM,TVGN_CARET,(LPARAM)Selected);
				flagSelected=true;
				tvi.mask=TVIF_TEXT|TVIF_PARAM|TVIF_CHILDREN;
				tvi.pszText=Text;
				tvi.cchTextMax=256;
				tvi.hItem=Selected;
				
							
				if(SendDlgItemMessage(hWnd,IDC_TREE1,TVM_GETITEM,TVGN_CARET,(LPARAM)&tvi))
				{
					TreeData* td = reinterpret_cast<TreeData*>(tvi.lParam);
					if (td){ 
						debug<<"Node Level="<<td->level<<" name="<<td->name<<" iData1="<<td->iData1<<endl;
						debug.flush();
						if (td->level==1){
							//mount device if not already mounted
							//add root dir if there are not children in this node
							if (tvi.cChildren==0){
								debug<<"Partition node has no children, mount partition"<<endl;

								Volume *v=new Volume(mountID);
								status_t s = v->Mount(td->iData1,td->iData2, 0 /*readonly*/);
								if (s == B_OK){
									debug<<"Mount successfull\n";
									// put volume data in partition tree node
									volumes.push_back(*v);
									td->extra=&(Volume)volumes[volumes.size()-1];
									tvi.lParam=reinterpret_cast<LPARAM>(td);
									TreeView_SetItem(hWnd,&tvi);
									// open rootdir:
									skyfs_open_dir(v, v->RootNode(), &iter);
									// second arg of skyfs_read_dir is not used actually
									status_t ss = skyfs_read_dir(v, v->RootNode(), iter, &d,sizeof(dirent), &num);
									//TODO: this should be the '.' entry...
									if (ss==B_OK && num>0) debug<<d.d_name<<endl;
									while (ss==B_OK && num>0){
										debug<<"before readdir\n";debug.flush();
										ss = skyfs_read_dir(v, v->RootNode(), iter, &d,sizeof(dirent), &num);
										if (ss==B_OK && num>0){ 
											//debug<<"readdir success\n";debug.flush();
											//printf("dirent: %s, len=%i, ino=%i\n",d.d_name,d.d_reclen,d.d_ino);
											Inode *I = new Inode(v,d.d_ino);
											if (I->IsDirectory()){ 
												debug<<d.d_name<<"(dir)"<<endl;debug.flush();
												TreeData* data = new TreeData(2,d.d_name,d.d_ino);
												data->extra=v;
												addChild(hWnd,&Selected, d.d_name,ICON_TREE,data);
											}
											if (I->IsFile()){ 
												debug<<d.d_name<<"(file)"<<endl;debug.flush();
												TreeData* data = new TreeData(2,d.d_name,d.d_ino);
												data->extra=v;
												addChild(hWnd,&Selected, d.d_name,ICON_FILE,data);
											}
											if (!I->IsFile() && !I->IsDirectory()){ 
												debug<<d.d_name<<"(?)"<<endl;debug.flush();
												TreeData* data = new TreeData(2,d.d_name,d.d_ino);
												data->extra=v;
												addChild(hWnd,&Selected, d.d_name,ICON_OTHER,data);
											}
											free(I);
										}
										//debug<<"at end while loop\n"; debug.flush();
									}
									//TreeView_Expand(hWnd,Selected,TVE_EXPAND);
									TreeView_Select(hWnd,Selected,TVGN_CARET);
									//TreeView_SelectDropTarget(GetDlgItem(hWnd,IDC_TREE1),Selected);
								}
								else{
									MessageBox(hWnd,"Unable to mount this partition.","Mount error",MB_OK|MB_ICONINFORMATION);
								}
								debug.flush();
							}
							else{
								debug<<"Partition node has children, partition mounted earlier"<<endl;
								debug.flush();
							}
						}
						else if (td->level>1){
							// we're somewhere in the partition, 
							// if dir, list contents, else do nothing
							// if has children, do nothing
							debug<<"somewhere in the partition\n";
							Volume* vol=(Volume*)td->extra;
							if (tvi.cChildren==0){
								debug<<"checking node type\n";
								Inode *II = new Inode(vol,td->inode);
								debug<<"\tInode size: "<<II->Size()<<endl;
								if (II->IsDirectory()){ 
									debug<<"Level "<<td->level<<" directory, listing contents\n";debug.flush();
									skyfs_open_dir(vol, II, &iter);
									status_t s1 = skyfs_read_dir(vol, II, iter, &d,sizeof(dirent), &num2);
									Inode* ii = new Inode(vol,d.d_ino);
									if (s1==B_OK && num2>0 && ii->IsDirectory()){ 
										//printf("\t%s (directory,%I64d)\n",dd.d_name,dd.d_ino);
										debug<<d.d_name<<"(dir)"<<endl;debug.flush();
										TreeData* data = new TreeData(td->level+1,d.d_name,d.d_ino);
										data->extra=vol;
										addChild(hWnd,&Selected, d.d_name,ICON_TREE,data);
									}
									if (s1==B_OK && num2>0 && ii->IsFile()){ 
										//printf("\t%s (file,%I64d)\n",dd.d_name,dd.d_ino);
										debug<<d.d_name<<"(file)"<<endl;debug.flush();
										TreeData* data = new TreeData(td->level+1,d.d_name,d.d_ino);
										data->extra=vol;
										addChild(hWnd,&Selected, d.d_name,ICON_OTHER,data);
									}
									if (s1==B_OK && num2>0 && !ii->IsDirectory() && !ii->IsFile()){ 
										//printf("\t%s (other,%I64d)\n",dd.d_name,dd.d_ino);
										debug<<d.d_name<<"(?)"<<endl;debug.flush();
										TreeData* data = new TreeData(2,d.d_name,d.d_ino);
										data->extra=vol;
										addChild(hWnd,&Selected, d.d_name,ICON_OTHER,data);
									}
									free(ii);
									while (s1==B_OK && num2>0){
										s1 = skyfs_read_dir(vol, II, iter, &d,sizeof(dirent), &num2);
										ii = new Inode(vol,d.d_ino);
										if (s1==B_OK && num2>0 && ii->IsDirectory()){ 
											//printf("\t%s (directory,%I64d)\n",dd.d_name,dd.d_ino);
											debug<<d.d_name<<"(dir)"<<endl;debug.flush();
											TreeData* data = new TreeData(td->level+1,d.d_name,d.d_ino);
											data->extra=vol;
											addChild(hWnd,&Selected, d.d_name,ICON_TREE,data);
										}
										if (s1==B_OK && num2>0 && ii->IsFile()){ 
											//printf("\t%s (file,%I64d)\n",dd.d_name,dd.d_ino);
											debug<<d.d_name<<"(file)"<<endl;debug.flush();
											TreeData* data = new TreeData(td->level+1,d.d_name,d.d_ino);
											data->extra=vol;
											addChild(hWnd,&Selected, d.d_name,ICON_FILE,data);
										}
										if (s1==B_OK && num2>0 && !ii->IsDirectory() && !ii->IsFile()){ 
											//printf("\t%s (other,%I64d)\n",dd.d_name,dd.d_ino);
											debug<<d.d_name<<"(?)"<<endl;debug.flush();
											TreeData* data = new TreeData(2,d.d_name,d.d_ino);
											data->extra=vol;
											addChild(hWnd,&Selected, d.d_name,ICON_FILE,data);
										}
										free(ii);
									}
									skyfs_close_dir(vol, II, &iter);
								}
								free(II);
							}
						}   
					}
				}
			}
			if(((LPNMHDR)lParam)->code == TVN_GETINFOTIP) // if code == NM_CLICK - Single click on an item
			{
				/*debug<<"infotip\n";
				debug.flush();
				LPNMTVGETINFOTIP pTip = (LPNMTVGETINFOTIP)lParam;
				HWND hTree = GetDlgItem(hWnd, IDC_TREE1);
				HTREEITEM item = pTip->hItem;

				// Get the text for the item.
				TVITEM tvitem;
				tvitem.mask = TVIF_TEXT;
				tvitem.hItem = item;
				TCHAR temp[1024];
				tvitem.pszText = "bla";
				tvitem.cchTextMax = sizeof(temp) / sizeof(TCHAR);
				TreeView_GetItem(hTree, &tvitem);

				// Copy the text to the infotip.
				wcscpy_s(pTip->pszText, pTip->cchTextMax, tvitem.pszText);
				*/
				break;
			}

			if(((LPNMHDR)lParam)->code == NM_RCLICK) // Right Click
			{
				Selected=(HTREEITEM)SendDlgItemMessage (hWnd,IDC_TREE1,TVM_GETNEXTITEM,TVGN_DROPHILITE,0);
				if(Selected==NULL)
				{
					MessageBox(hWnd,"No Items in TreeView","Error",MB_OK|MB_ICONINFORMATION);
					break;
				}
				// get attributes...
			/*	debug<<"DOUBLE CLICK! => Copy file to local dir if this is a file\n";debug.flush();
				char Text[255]="";
				memset(&tvi,0,sizeof(tvi));

				Selected=(HTREEITEM)SendDlgItemMessage(hWnd,IDC_TREE1,TVM_GETNEXTITEM,TVGN_CARET,(LPARAM)Selected);
				
				if(Selected==NULL)
				{
					MessageBox(hWnd,"No Items in TreeView","Error",MB_OK|MB_ICONINFORMATION);
					break;
				}
				TreeView_EnsureVisible(hWnd,Selected);
			    SendDlgItemMessage(hWnd,IDC_TREE1,TVM_SELECTITEM,TVGN_CARET,(LPARAM)Selected);
				flagSelected=true;
				tvi.mask=TVIF_TEXT|TVIF_PARAM|TVIF_CHILDREN;
				tvi.pszText=Text;
				tvi.cchTextMax=256;
				tvi.hItem=Selected;
				
							
				if(SendDlgItemMessage(hWnd,IDC_TREE1,TVM_GETITEM,TVGN_CARET,(LPARAM)&tvi))
				{
					TreeData* td = reinterpret_cast<TreeData*>(tvi.lParam);
					if (td){ 
						debug<<"Node Level="<<td->level<<" name="<<td->name<<" inode="<<td->inode<<endl;
						debug.flush();
						Volume* vol=(Volume*)td->extra;
						Inode *II = new Inode(vol,td->inode);
						debug<<"\tInode size: "<<II->Size()<<endl;
						if (II->IsFile()){ 
							debug<<"This is a FILE!!! go get it\n";debug.flush();
							Inode *I;
							void *f; // file_cookie
							file_cookie ff;
							//printf("skyfs_read_vnode result: %i\n",skyfs_read_vnode(v,135901,(void**)&I,true ));
							debug<<"skyfs_read_vnode result: "<<skyfs_read_vnode(vol,td->inode,(void**)&I,true )<<endl;
							debug<<"open status: "<<skyfs_open(vol,I,O_RDONLY,&f)<<endl;;
							memcpy(&ff,f,sizeof(file_cookie));
							debug<<"filesize: "<<ff.last_size<<endl;
							char * q = (char *)calloc(ff.last_size,sizeof(char));
							//uint32 l = 1024;//(struct file_cookie)f->last_size;
							debug<<"read status: "<<skyfs_read(vol,I,f,0,q,&ff.last_size)<<endl;
							//for (int i=0; i<ff.last_size;i++) printf("%c",q[i]);
							skyfs_close(vol,I,&ff);
							ofstream file;
							file.open(td->name,ios::trunc|ios::out|ios::binary);
							if (file.is_open()) {
								// ok, proceed with output 
								file.write(q,ff.last_size);
								file.close();
								ostringstream oss;
								oss << "File copied to local directory: " << td->name;
								string str=oss.str();
								MessageBox(hWnd,str.c_str(),"File copied",MB_OK|MB_ICONINFORMATION);
							}
						}
						free(II);
					}
				}
*/
				SendDlgItemMessage(hWnd,IDC_TREE1,TVM_SELECTITEM,TVGN_CARET,(LPARAM)Selected);
				SendDlgItemMessage(hWnd,IDC_TREE1,TVM_SELECTITEM,TVGN_DROPHILITE,(LPARAM)Selected);
				TreeView_EnsureVisible(hTree,Selected);
			}

			if(((LPNMHDR)lParam)->code == TVN_BEGINDRAG)
			{
				HIMAGELIST hImg;
				LPNMTREEVIEW lpnmtv = (LPNMTREEVIEW) lParam;
				hImg=TreeView_CreateDragImage(hTree, lpnmtv->itemNew.hItem);
				ImageList_BeginDrag(hImg, 0, 0, 0);
				ImageList_DragEnter(hTree,lpnmtv->ptDrag.x,lpnmtv->ptDrag.y);
				ShowCursor(FALSE); 
				SetCapture(hWnd); 
				Dragging = TRUE;	
			}

			if(((LPNMHDR)lParam)->code == TVN_BEGINLABELEDIT)
			{
				hEdit=TreeView_GetEditControl(hTree);
			}

			if(((LPNMHDR)lParam)->code == TVN_ENDLABELEDIT)
			{
				char Text[256]="";
				tvi.hItem=Selected;
				SendDlgItemMessage(hWnd,IDC_TREE1,TVM_GETITEM,0,(WPARAM)&tvi);
				GetWindowText(hEdit, Text, sizeof(Text)); 
				tvi.pszText=Text;
				SendDlgItemMessage(hWnd,IDC_TREE1,TVM_SETITEM,0,(WPARAM)&tvi);
			}
		}
		break;

		case WM_MOUSEMOVE:
		{
			if (Dragging) 
			{ 
				Pos = MAKEPOINTS(lParam);
				ImageList_DragMove(Pos.x-32, Pos.y-25); // where to draw the drag from
				ImageList_DragShowNolock(FALSE);
				tvht.pt.x = Pos.x-20; // the highlight items should be as the same points as the drag
				tvht.pt.y = Pos.y-20; //
				if(hitTarget=(HTREEITEM)SendMessage(hTree,TVM_HITTEST,NULL,(LPARAM)&tvht)) // if there is a hit
					SendMessage(hTree,TVM_SELECTITEM,TVGN_DROPHILITE,(LPARAM)hitTarget);   // highlight it
			
			    ImageList_DragShowNolock(TRUE); 
			} 
		}
		break;

        case WM_LBUTTONUP:
        {
            if (Dragging) 
            {
                ImageList_DragLeave(hTree);
                ImageList_EndDrag();
                Selected=(HTREEITEM)SendDlgItemMessage(hWnd,IDC_TREE1,TVM_GETNEXTITEM,TVGN_DROPHILITE,0);
                SendDlgItemMessage(hWnd,IDC_TREE1,TVM_SELECTITEM,TVGN_CARET,(LPARAM)Selected);
                SendDlgItemMessage(hWnd,IDC_TREE1,TVM_SELECTITEM,TVGN_DROPHILITE,0);
                ReleaseCapture();
                ShowCursor(TRUE); 
                Dragging = FALSE;
            }
        }
        break;


		case WM_PAINT: // constantly painting the window
		{
			return 0;
		}
		break;
		
		case WM_COMMAND: // Controling the Buttons
		{
			switch (LOWORD(wParam)) // what we pressed on?
			{ 	 

				case IDC_DELETE: // Generage Button is pressed
				{
					
					if(flagSelected==true)
					{
					 if(tvi.cChildren==0)
					      SendDlgItemMessage(hWnd,IDC_TREE1,TVM_DELETEITEM,TVGN_CARET,(LPARAM)tvi.hItem);
					  flagSelected=false;
					}
					else{ 
						MessageBox(hWnd,"Double Click Item to Delete","Message",MB_OK|MB_ICONINFORMATION);
					}
				} 
				break;

				case IDC_ADDROOT:
				{
					tvinsert.hParent=NULL;			// top most level no need handle
					tvinsert.hInsertAfter=TVI_ROOT; // work as root level
					tvinsert.item.pszText="Parent Added";
					Parent=(HTREEITEM)SendDlgItemMessage(hWnd,IDC_TREE1,TVM_INSERTITEM,0,(LPARAM)&tvinsert);
					UpdateWindow(hWnd);
				}
				break;

				case IDC_CHILD:
				{
                    tvinsert.hParent=Selected;			// top most level no need handle
					tvinsert.hInsertAfter=TVI_LAST; // work as root level
					tvinsert.item.pszText="Child Added";
					Parent=(HTREEITEM)SendDlgItemMessage(hWnd,IDC_TREE1,TVM_INSERTITEM,0,(LPARAM)&tvinsert);
					TreeView_SelectDropTarget(GetDlgItem(hWnd,IDC_TREE1),Parent);
				}
				break;

				case IDC_DELALL:
				{
					int TreeCount=TreeView_GetCount(GetDlgItem(hWnd,IDC_TREE1));
					for(int i=0;i<TreeCount;i++)
						TreeView_DeleteAllItems(GetDlgItem(hWnd,IDC_TREE1));
				}
				break;	
			}
			break;

			case WM_CLOSE: // We colsing the Dialog
			{
				EndDialog(hWnd,0); 
			}
			break;
		}
		break;
	}
	return 0;
}

int addChild(HWND hWnd,HTREEITEM* parent, char* t1,int icon,TreeData* td){
	if (strcmp(t1,"..")!=0 && strcmp(t1,".")!=0){
		debug<<"AddChild: "<<t1<<endl;
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
		*parent = (HTREEITEM)SendDlgItemMessage(hWnd,IDC_TREE1,TVM_INSERTITEM,0,(LPARAM)&tvinsert);
	}
	else debug<<"AddChild: ignoring '.' or '..'"<<endl;
	return 0;
}

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int iCmdShow)
{
	hInst=hInstance;
	DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, (DLGPROC)DialogProc,0);
	debug << "Exit application.\n";
	debug.close();
	return 0;
}
