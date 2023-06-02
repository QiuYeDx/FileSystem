#include "struct_def.h"


/*
*	Create a new drirectory only with "." and ".." items.
*
*	return: the function returns true only when the new directory is
*			created successfully.
*/
bool MakeDir(const char* dirname);

/*
*	Delete a drirectory as well as all files and sub-directories in it.
*
*	return: the function returns true only when the directory as well
*			as all files and sub-directories in it is deleted successfully.
*/
bool RemoveDir(const char* dirname);

/*
*	Open a directory.
*
*	return: the function returns true only when the directory is
*			opened successfully.
*/
bool OpenDir(const char* dirname);

/*
*	Print file and directory information under current directory.
*/
void List();

/*
*	Print absolute directory.
*/
void Ab_dir();
