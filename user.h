#include "struct_def.h"

/*
*	Log in function. Update user information by checking log in inputs.
*
*	return: the function return true only when log in process succeed.
*/
bool Login(const char* user, const char* password);
/*
*	Log out function. Remove user's states.
*/
void Logout();

/*
*	Change file permission.
*/
void Chmod(char* filename);

/*
*	Change file' owner.
*/
void Chown(char* filename);

/*
*	Change file' group.
*/
void Chgrp(char* filename);

/*
*	Change password.
*/
void Passwd();

/*
*	User Management.
*/
void User_management();