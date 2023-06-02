#include "struct_def.h"

/*
*	Formatting function of the file system, including the establishment
*	of superblock, inode chain, root directory, password file and so on.
*
*	return: the function return true only the file system is initialized
*			successfully.
*/
bool Format();

/*
*	Initialization function of the file system. Open an existing file system
*	from 'fs.han'.
*
*	return: the function return true only when the file system has been
*			formatted and is complete.
*/
bool Mount();