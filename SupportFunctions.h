/*
CurrentItem = TreeView_GetRoot();

while(CurrentItem)
{   //do something or check something
    //or make something like i++
    CurrentItem = GetNext(CurrentItem);
}

now the question is on the GetNext() function

GetNext(Item)
{   TempItem = TreeView_GetChild(Item);
   
    if(TempItem)
        return TempItem;

    TempItem = TreeView_GetNextItem( , Item, TVGN_NEXT);
    if(TempItem)
        return TempItem;

    return GetNextUpItem(Item);
}

and the function GetNextUpItem()

GetNextUpItem(Item)
{    Item = TreeView_GetNextItem( , Item, TVGN_PARENT);
     if(!Item)
         return NULL;

     TempItem = TreeView_GetNextItem( , Item, TVGN_NEXT);
     
     if(TempItem)
         return TempItem;

     return GetNextUpItem(Item);
}

*/
#include <windows.h>
#include <commctrl.h>
//tree traversal functions

HTREEITEM GetNext(HWND hTree, HTREEITEM Item);
HTREEITEM GetNextUpItem(HWND hTree, HTREEITEM Item);
