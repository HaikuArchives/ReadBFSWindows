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