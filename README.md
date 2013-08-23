Read BeFS from Windows
==========================
Original author: Peter Speybrouck

The code here appears as it was given to me by Peter, after I contacted him about [this forum post](https://www.haiku-os.org/community/forum/bfs_access_in_windows). I didn't touch it, but I made a copy and attempted to make it compine and ran into a lot of MSVC errors that I hate.

Some things that could be done:
* make it compile on MinGW
* update the BFS code with the equivalent files from the Haiku repository
* general cleanup
* turn this into a Windows filesystem driver

Regarding a license, he said: 
> The haiku code obviously keeps its licensing. You can do with my code whatever you want if you ask me. 

## More information from Peter about the source
A lot of the BFS code is from the Haiku repository, adapted to make it compile and with some added code to get it to work in windows XP (never tested Vista or 7). That code has changed since then and the parts for writinng have been commented out because that required some more implementation of windows specific code and of course testing.

*Small guide through the code*
* most files are basically the original code from haiku with here and there some code commented out or roughly converted to make it compile on windows. I am not claiming that this will always be the best or correct way, but it made it work :)
* System.cpp contains code that implements a number of functions that are normally provided by the operating system but I didn't know of an existing equivalent, so I implemented these myself (read_pos functions). This file also contains a number of functions that are not implemented but were required to prevent massive changes to the  haiku code. Some of these would need to be implemented to get write support.
* DiskFunc.h/cpp contains custom code to read the partition table and to enumerate the files in a directory.
* TreeControl.cpp contains the GUI interface, but there are a few functions scattered over DiscFunc.cpp and SupportFunctions.cpp
* Volume.h also contains a list with partition code definitions that are used to determine the partition type. Note that when I tested the application, I ran into a partition type that did not match what I would have expected, but it had the same structure, so it might have been some customized installation, don't remember. This is why you will see the PART_BFS1 definition.
* supportDefs.h contains a bunch of code that is sometimes copy/pasted from various header files to avoid having to include the full header files which was causing more issues.
* I'm guessing that there have been updates to the relevant haiku code, these have not been merged, but might improve performance.
