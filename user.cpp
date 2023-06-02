#include "struct_def.h"
#include "os.h"

/*
*	Log in function. Update user information by checking log in inputs.
*
*	return: the function return true only when log in process succeed.
*/
bool Login(const char* user, const char* password)
{
	//parameters check
	if (user == NULL || password == NULL)
	{
		printf("Error: User name or password illegal!\n\n");
		return false;
	}
	if (strlen(user) > USER_NAME_LENGTH || strlen(password) > USER_PASSWORD_LENGTH)
	{
		printf("Error: User name or password illegal!\n");
		return false;
	}

	//have logged in?
	if (userID != ACCOUNT_NUM)
	{
		printf("Login failed: User has been logged in. Please log out first.\n");
		return false;
	}

	//search the user in accouting file
	for (int i = 0; i < ACCOUNT_NUM; i++)
	{
		if (strcmp(users.userName[i], user) == 0)
		{
			//find the user and check password
			if (strcmp(users.password[i], password) == 0)
			{
				//Login successfully
				printf("Login successfully.\n");
				userID = users.userID[i];
				//make user's name, root user is special
				memset(userName, 0, USER_NAME_LENGTH + 6);
				if (userID == 0)
				{
					strcat(userName, "root ");
					strcat(userName, users.userName[i]);
					strcat(userName, "$");
				}
				else
				{
					strcat(userName, users.userName[i]);
					strcat(userName, "#");
				}

				return true;
			}
			else
			{
				//Password wrong
				printf("Login failed: Wrong password.\n");
				return false;
			}
		}
	}

	//User not found
	printf("Login failed: User not found.\n");
	return false;

};

/*
*	Log out function. Remove user's states.
*/
void Logout()
{
	//remove user's states
	userID = ACCOUNT_NUM;
	memset(&users, 0, sizeof(users));
	memset(userName, 0, 6 + USER_NAME_LENGTH);
	Mount();
};

/*
*	Change file permission.
*/
void Chmod(char* filename) {
	printf("File or Dir?(0 stands for file, 1 for dir):");
	int tt;
	scanf("%d", &tt);
	//parameter check
	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal file name.\n");
		return;
	}

	/*
	*	1. Check whether the file exists in current directory.
	*/
	int pos_in_directory = -1;
	inode* tmp_file_inode = new inode;
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Not found.\n");
			return;
		}

		/*
		*	2. Fetch inode and check whether it's a directory.
		*/
		//Fetch inode
		int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
		fread(tmp_file_inode, sizeof(inode), 1, fd);
		//Directory check
	} while (tmp_file_inode->di_mode == tt);
	if (userID == tmp_file_inode->di_uid)
	{
		if (!(tmp_file_inode->permission & OWN_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}
	else if (users.groupID[userID] == tmp_file_inode->di_grp) {
		if (!(tmp_file_inode->permission & GRP_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}
	else {
		if (!(tmp_file_inode->permission & ELSE_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}
	printf("Please input 0&1 string(1 stands for having, 0 for not)\n");
	printf("Format: rwxrwxrwx\n");
	char str[10];
	scanf("%s", str);
	int temp = 0;
	for (int i = 0; i < 8; i++) {
		if (str[i] == '1')
			temp += 1 << (8 - i);
	}
	if (str[8] == '1') temp += 1;
	tmp_file_inode->permission = temp;
	fseek(fd, INODE_START + tmp_file_inode->i_ino * INODE_SIZE, SEEK_SET);
	fwrite(tmp_file_inode, sizeof(inode), 1, fd);
	printf("Change file permission successfully!\n");
	return;
}

/*
*	Change file' owner.
*/
void Chown(char* filename) {
	printf("File or Dir?(0 stands for file, 1 for dir):");
	int tt;
	scanf("%d", &tt);
	//parameter check
	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal file name.\n");
		return;
	}

	int pos_in_directory = -1;
	inode* tmp_file_inode = new inode;
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Not found.\n");
			return;
		}

		//Fetch inode
		int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
		fread(tmp_file_inode, sizeof(inode), 1, fd);
		//Directory check
	} while (tmp_file_inode->di_mode == tt);

	if (userID == tmp_file_inode->di_uid)
	{
		if (!(tmp_file_inode->permission & OWN_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}
	else if (users.groupID[userID] == tmp_file_inode->di_grp) {
		if (!(tmp_file_inode->permission & GRP_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}
	else {
		if (!(tmp_file_inode->permission & ELSE_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}

	printf("Please input user name:");
	char str[USER_NAME_LENGTH];
	int i;
	scanf("%s", str);
	for (i = 0; i < ACCOUNT_NUM; i++) {
		if (strcmp(users.userName[i], str) == 0) break;
	}
	if (i == ACCOUNT_NUM) {
		printf("Error user!\n");
		return;
	}
	tmp_file_inode->di_uid = i;
	fseek(fd, INODE_START + tmp_file_inode->i_ino * INODE_SIZE, SEEK_SET);
	fwrite(tmp_file_inode, sizeof(inode), 1, fd);
	printf("Change file's owner successfully!\n");
	return;
}

/*
*	Change file' group.
*/
void Chgrp(char* filename) {
	printf("File or Dir?(0 stands for file, 1 for dir):");
	int tt;
	scanf("%d", &tt);
	//parameter check
	if (filename == NULL || strlen(filename) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal file name.\n");
		return;
	}

	int pos_in_directory = -1;
	inode* tmp_file_inode = new inode;
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], filename) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Not found.\n");
			return;
		}

		//Fetch inode
		int tmp_file_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
		fread(tmp_file_inode, sizeof(inode), 1, fd);
		//Directory check
	} while (tmp_file_inode->di_mode == tt);

	if (userID == tmp_file_inode->di_uid)
	{
		if (!(tmp_file_inode->permission & OWN_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}
	else if (users.groupID[userID] == tmp_file_inode->di_grp) {
		if (!(tmp_file_inode->permission & GRP_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}
	else {
		if (!(tmp_file_inode->permission & ELSE_X)) {
			printf("Open file error: Access deny.\n");
			return;
		}
	}

	printf("Please input group number:");
	int ttt, i;
	scanf("%d", &ttt);
	for (i = 0; i < ACCOUNT_NUM; i++) {
		if (users.groupID[i] == ttt) break;
	}
	if (i == ACCOUNT_NUM) {
		printf("Error user!\n");
		return;
	}
	tmp_file_inode->di_grp = ttt;
	fseek(fd, INODE_START + tmp_file_inode->i_ino * INODE_SIZE, SEEK_SET);
	fwrite(tmp_file_inode, sizeof(inode), 1, fd);
	printf("Change file's group successfully!\n");
	return;
}

/*
*	Change password.
*/
void Passwd() {
	printf("Please input old password:");
	char str[USER_PASSWORD_LENGTH+1];
	scanf("%s", str);
	str[USER_PASSWORD_LENGTH] = 0;
	if (strcmp(users.password[userID], str) == 0) {
		printf("Please input a new password:");
		scanf("%s", str);
		if (strcmp(users.password[userID], str) == 0) {
			printf("Password doesn't change!\n");
		}
		else {
			strcpy(users.password[userID], str);
			//Write
			fseek(fd, DATA_START + BLOCK_SIZE, SEEK_SET);
			fwrite(&users, sizeof(users), 1, fd);
			printf("Success\n");
		}
	}
	else {
		printf("Password Error!!!\n");
	}
	printf("Change password successfully!\n");
}

/*
*	User Management.
*/
void User_management() {
	if (userID != 0) {
		printf("Only administrator can manage users!\n");
		return;
	}
	printf("Welcome to user management!\n");
	char c, d;
	scanf("%c", &c);
	while (c != '0') {
		printf("\n1.View 2.Create 3.Remove 0.Save & Exit\n");
		scanf("%c", &c);
		switch (c) {
		case '1':
			for (int i = 0; i < ACCOUNT_NUM; i++) {
				if (users.userName[i][0] != 0)
					printf("%d\t%s\t%d\n", users.userID[i], users.userName[i], users.groupID[i]);
				else break;
			}
			scanf("%c", &c);
			break;
		case '2':
			int i;
			for (i = 0; i < ACCOUNT_NUM; i++) {
				if (users.userName[i][0] == 0) break;
			}
			if (i == ACCOUNT_NUM) {
				printf("Users Full!\n");
				break;
			}
			printf("Please input username:");
			char str[USER_NAME_LENGTH];
			scanf("%s", str);
			for (i = 0; i < ACCOUNT_NUM; i++) {
				if (strcmp(users.userName[i], str) == 0) {
					printf("Already has same name user!\n");
				}
				if (users.userName[i][0] == 0) {
					strcpy(users.userName[i], str);
					printf("Please input password:");
					scanf("%s", str);
					strcpy(users.password[i], str);
					printf("Please input group ID:");
					int t;
					scanf("%d", &t);
					scanf("%c", &c);
					if (t > 0) {
						users.groupID[i] = t;
						printf("Success!\n");
					}
					else {
						printf("Insert Error!\n");
						//strcpy(users.userName[i], 0);
						memset(users.userName[i], 0, 14);
						//strcpy(users.password[i], 0);
						memset(users.password[i], 0, 14);
					}
					break;
				}
			}
			break;
		case '3':
			printf("Please input userID:");
			int tmp;
			scanf("%d", &tmp);scanf("%c", &c);
			if (strcmp(users.userName[tmp], "") == 0) {
				printf("Remove Error!\n");
			}
			else {
				for (int j = tmp; j < ACCOUNT_NUM - 1; j++) {
					strcpy(users.userName[j], users.userName[j + 1]);
					strcpy(users.password[j], users.password[j + 1]);
					users.groupID[j] = users.groupID[j + 1];
				}
				printf("Success!\n");
			}
			break;
		}
	}
	fseek(fd, DATA_START + BLOCK_SIZE, SEEK_SET);
	fwrite(&users, sizeof(users), 1, fd);
}