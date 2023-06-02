#include "struct_def.h"


/*
*	Create a new empty file with the specific file name.
*
*	return: the function return true only when the new file is successfully
*			created.
*/
bool CreateFile(const char* filename);

/*
*	Delete a file.
*
*	return: the function returns true only delete the file successfully.
*/
bool DeleteFile(const char* filename);

/*
*	Open the specific file under current directory.
*
*	return: the function returns a pointer of the file's inode if the file is
*			successfully opened and NULL otherwise.
*/
inode* OpenFile(const char* filename);

/*
*	Append a string "content" to the specific file.
*
*	return: the function returns the number of bytes it has writen or -1 when
*			some error occur.
*/
int Write(inode& ifile, const char* content);

/*
*	Print the string "content" in the specific file.
*
*	return: none
*
*/
void PrintFile(inode& ifile);
