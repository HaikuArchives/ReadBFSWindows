/* Author: Peter Speybrouck - peter.speybrouck@gmail.com
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.ORG> wrote this file. As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return. Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

#include "SupportFunctions.h"

//tree traversal functions

HTREEITEM GetNext(HWND hTree, HTREEITEM Item)
{   
	HTREEITEM TempItem = TreeView_GetChild(hTree, Item);
   
    if(TempItem)
        return TempItem;

    TempItem = TreeView_GetNextItem(hTree, Item, TVGN_NEXT);
    if(TempItem)
        return TempItem;

    return GetNextUpItem(hTree, Item);
}


HTREEITEM GetNextUpItem(HWND hTree, HTREEITEM Item)
{    
	Item = TreeView_GetNextItem(hTree, Item, TVGN_PARENT);
     if(!Item)
         return NULL;

     HTREEITEM TempItem = TreeView_GetNextItem(hTree, Item, TVGN_NEXT);
     
     if(TempItem)
         return TempItem;

     return GetNextUpItem(hTree, Item);
}