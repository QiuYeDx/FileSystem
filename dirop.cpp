#include "dirop.h"
#include "fileop.h"
#include "blockop.h"

/*
*	Create a new drirectory only with "." and ".." items.
*
*	return: the function returns true only when the new directory is
*			created successfully.
*/
bool MakeDir(const char* dirname)
{
	// 检查输入的合法性
	if (dirname == NULL || strlen(dirname) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal directory name.\n");
		return false;
	}

	if (superBlock.s_num_fblock <= 0 || superBlock.s_num_finode <= 0)
	{
		printf("File creation error: No valid spaces.\n");
		return false;
	}

	// 查找可用的inode和block
	int new_ino = 0;
	unsigned int new_block_addr = 0;
	for (; new_ino < INODE_NUM; new_ino++)
	{
		if (inode_bitmap[new_ino] == 0)
		{
			break;
		}
	}
	find_free_block(new_block_addr);
	if (new_block_addr == -1) return false;
	if (new_ino == INODE_NUM || new_block_addr == BLOCK_NUM)
	{
		printf("File creation error: No valid spaces.\n");
		return false;
	}

	// 检查目录名是否已经存在
	for (int i = 0; i < DIRECTORY_NUM; i++)
	{
		if (strcmp(currentDirectory.fileName[i], dirname) == 0)
		{
			// 目录名已存在，检查对应的inode类型
			inode* tmp_file_inode = new inode;
			int tmp_file_ino = currentDirectory.inodeID[i];
			fseek(fd, INODE_START + tmp_file_ino * INODE_SIZE, SEEK_SET);
			fread(tmp_file_inode, sizeof(inode), 1, fd);
			if (tmp_file_inode->di_mode == 1) 
				continue;	// 目录名已被使用且对应的inode类型是文件
			else {
				printf("File creation error: Directory name '%s' has been used.\n", currentDirectory.fileName[i]);
				return false;
			}
		}
	}

	// 检查当前路径中的文件或目录数量是否超过限制
	int itemCounter = 0;
	for (int i = 0; i < DIRECTORY_NUM; i++)
	{
		if (strlen(currentDirectory.fileName[i]) > 0)
		{
			itemCounter++;
		}
	}
	if (itemCounter >= DIRECTORY_NUM)
	{
		printf("File creation error: Too many files or directories in current path.\n");
		return false;
	}

	// 创建新目录的inode
	inode idir_tmp;
	idir_tmp.i_ino = new_ino;				//Identification
	idir_tmp.di_number = 1;					//Associations
	idir_tmp.di_mode = 0;					//0 stands for directory
	idir_tmp.di_size = sizeof(directory);	//"For directories, the value is 0." ***
	memset(idir_tmp.di_addr, -1, sizeof(unsigned int) * NADDR);
	idir_tmp.di_addr[0] = new_block_addr;
	idir_tmp.di_uid = userID;				
	idir_tmp.di_grp = users.groupID[userID];
	time_t t = time(0);
	strftime(idir_tmp.time, sizeof(idir_tmp.time), "%Y/%m/%d %X %A %jday %z", localtime(&t));
	idir_tmp.time[64] = 0;
	idir_tmp.icount = 0;
	idir_tmp.permission = MAX_PERMISSION;
	fseek(fd, INODE_START + new_ino * INODE_SIZE, SEEK_SET);
	fwrite(&idir_tmp, sizeof(inode), 1, fd);

	// 创建新目录的数据块，并写入 "." 和 ".." 项
	directory tmp_dir;
	memset(tmp_dir.fileName, 0, sizeof(char) * DIRECTORY_NUM * FILE_NAME_LENGTH);
	memset(tmp_dir.inodeID, -1, sizeof(unsigned int) * DIRECTORY_NUM);
	strcpy(tmp_dir.fileName[0], ".");
	tmp_dir.inodeID[0] = new_ino;
	strcpy(tmp_dir.fileName[1], "..");
	tmp_dir.inodeID[1] = currentDirectory.inodeID[0];
	fseek(fd, DATA_START + new_block_addr * BLOCK_SIZE, SEEK_SET);
	fwrite(&tmp_dir, sizeof(directory), 1, fd);

	// 更新inode位图
	inode_bitmap[new_ino] = 1;
	fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
	fwrite(inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);

	// 在父目录中添加新目录项
	int pos_directory_inode = 0;
	pos_directory_inode = currentDirectory.inodeID[0]; // 至于当前的"." 「当前目录，新目录的父目录，是新目录的".."」
	inode tmp_directory_inode;	// 新目录的父目录的inode
	fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
	fread(&tmp_directory_inode, sizeof(inode), 1, fd);

	// ----查找空闲的目录项，并添加新目录项
	for (int i = 2; i < DIRECTORY_NUM; i++)
	{
		if (strlen(currentDirectory.fileName[i]) == 0)
		{
			strcat(currentDirectory.fileName[i], dirname);
			currentDirectory.inodeID[i] = new_ino;
			break;
		}
	}
	fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fwrite(&currentDirectory, sizeof(directory), 1, fd);

	// 更新目录项的关联计数
	directory tmp_directory = currentDirectory;
	int tmp_pos_directory_inode = pos_directory_inode;
	while (true)
	{
		tmp_directory_inode.di_number++;
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
		if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
		{
			break;
		}
		tmp_pos_directory_inode = tmp_directory.inodeID[1];		//".."
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fread(&tmp_directory_inode, sizeof(inode), 1, fd);
		fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);	// ***
		fread(&tmp_directory, sizeof(directory), 1, fd);
	}

	// 更新超级块中的空闲inode数量
	superBlock.s_num_finode--;
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(filsys), 1, fd);
	printf("Create a directory successfully!\n");
	return true;
};

/*
*	Delete a drirectory as well as all files and sub-directories in it.
*
*	return: the function returns true only when the directory as well
*			as all files and sub-directories in it is deleted successfully.
*/
bool RemoveDir(const char* dirname)
{
	// 检查输入的合法性
	if (dirname == NULL || strlen(dirname) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal directory name.\n");
		return false;
	}

	int pos_in_directory = 0;
	int tmp_dir_ino;
	inode tmp_dir_inode;

	// 查找目录并获取目录的inode
	do {
		pos_in_directory++;
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], dirname) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Delete error: Directory not found.\n");
			return false;
		}

		tmp_dir_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_dir_ino * INODE_SIZE, SEEK_SET);
		fread(&tmp_dir_inode, sizeof(inode), 1, fd);
	} while (tmp_dir_inode.di_mode == 1);

	// 检查用户权限
	if (userID == tmp_dir_inode.di_uid)
	{
		if (!(tmp_dir_inode.permission & OWN_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	else if (users.groupID[userID] == tmp_dir_inode.di_grp) {
		if (!(tmp_dir_inode.permission & GRP_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}
	else {
		if (!(tmp_dir_inode.permission & ELSE_X)) {
			printf("Delete error: Access deny.\n");
			return false;
		}
	}

	if (tmp_dir_inode.icount > 0) {
		tmp_dir_inode.icount--;
		fseek(fd, INODE_START + tmp_dir_inode.i_ino * INODE_SIZE, SEEK_SET);
		fwrite(&tmp_dir_inode, sizeof(inode), 1, fd);

		int pos_directory_inode = currentDirectory.inodeID[0];	//"."
		inode tmp_directory_inode;
		fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
		fread(&tmp_directory_inode, sizeof(inode), 1, fd);

		memset(currentDirectory.fileName[pos_in_directory], 0, FILE_NAME_LENGTH);
		currentDirectory.inodeID[pos_in_directory] = -1;
		fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
		fwrite(&currentDirectory, sizeof(directory), 1, fd);

		directory tmp_directory = currentDirectory;
		int tmp_pos_directory_inode = pos_directory_inode;
		while (true)
		{
			tmp_directory_inode.di_number--;
			fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
			fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
			if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
			{
				break;
			}
			tmp_pos_directory_inode = tmp_directory.inodeID[1];		//".."
			fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
			fread(&tmp_directory_inode, sizeof(inode), 1, fd);
			fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
			fread(&tmp_directory, sizeof(directory), 1, fd);
		}
		return true;
	}

	// 递归删除目录下的文件和子目录
	directory tmp_dir;
	fseek(fd, DATA_START + tmp_dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fread(&tmp_dir, sizeof(directory), 1, fd);

	inode tmp_sub_inode;
	char tmp_sub_filename[FILE_NAME_LENGTH];
	memset(tmp_sub_filename, 0, FILE_NAME_LENGTH);
	for (int i = 2; i < DIRECTORY_NUM; i++)
	{
		if (strlen(tmp_dir.fileName[i]) > 0)
		{
			strcpy(tmp_sub_filename, tmp_dir.fileName[i]);
			fseek(fd, INODE_START + tmp_dir.inodeID[i] * INODE_SIZE, SEEK_SET);
			fread(&tmp_sub_inode, sizeof(inode), 1, fd);
			directory tmp_swp;
			tmp_swp = currentDirectory;
			currentDirectory = tmp_dir;
			tmp_dir = tmp_swp;
			if (tmp_sub_inode.di_mode == 1)
			{
				DeleteFile(tmp_sub_filename);
			}
			else if (tmp_sub_inode.di_mode == 0)
			{
				RemoveDir(tmp_sub_filename);
			}
			tmp_swp = currentDirectory;
			currentDirectory = tmp_dir;
			tmp_dir = tmp_swp;
		}
	}

	// 清空目录的inode，并释放占用的块和inode
	int tmp_fill[sizeof(inode)];
	memset(tmp_fill, 0, sizeof(inode));
	fseek(fd, INODE_START + tmp_dir_ino * INODE_SIZE, SEEK_SET);
	fwrite(&tmp_fill, sizeof(inode), 1, fd);

	inode_bitmap[tmp_dir_ino] = 0;
	fseek(fd, 2 * BLOCK_SIZE, SEEK_SET);
	fwrite(&inode_bitmap, sizeof(unsigned short) * INODE_NUM, 1, fd);
	//block bitmap
	for (int i = 0; i < (tmp_dir_inode.di_size / BLOCK_SIZE + 1); i++)
	{
		recycle_block(tmp_dir_inode.di_addr[i]);
	}

	// 更新父目录信息（关联计数）
	int pos_directory_inode = currentDirectory.inodeID[0];	//"."
	inode tmp_directory_inode;
	fseek(fd, INODE_START + pos_directory_inode * INODE_SIZE, SEEK_SET);
	fread(&tmp_directory_inode, sizeof(inode), 1, fd);

	memset(currentDirectory.fileName[pos_in_directory], 0, FILE_NAME_LENGTH);
	currentDirectory.inodeID[pos_in_directory] = -1;
	fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * INODE_SIZE, SEEK_SET);
	fwrite(&currentDirectory, sizeof(directory), 1, fd);

	directory tmp_directory = currentDirectory;
	int tmp_pos_directory_inode = pos_directory_inode;
	while (true)
	{
		tmp_directory_inode.di_number--;
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fwrite(&tmp_directory_inode, sizeof(inode), 1, fd);
		if (tmp_directory.inodeID[1] == tmp_directory.inodeID[0])
		{
			break;
		}
		tmp_pos_directory_inode = tmp_directory.inodeID[1];		//".."
		fseek(fd, INODE_START + tmp_pos_directory_inode * INODE_SIZE, SEEK_SET);
		fread(&tmp_directory_inode, sizeof(inode), 1, fd);
		fseek(fd, DATA_START + tmp_directory_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
		fread(&tmp_directory, sizeof(directory), 1, fd);
	}

	superBlock.s_num_finode++;
	fseek(fd, BLOCK_SIZE, SEEK_SET);
	fwrite(&superBlock, sizeof(filsys), 1, fd);
	printf("Remove a directory successfully!\n");
	return true;
};

/*
*	Open a directory.
*
*	return: the function returns true only when the directory is
*			opened successfully.
*/
bool OpenDir(const char* dirname)
{
	if (dirname == NULL || strlen(dirname) > FILE_NAME_LENGTH)
	{
		printf("Error: Illegal directory name.\n");
		return false;
	}

	int pos_in_directory = 0;
	inode tmp_dir_inode;
	int tmp_dir_ino;
	bool dir_flag = false;
	do {
		for (; pos_in_directory < DIRECTORY_NUM; pos_in_directory++)
		{
			if (strcmp(currentDirectory.fileName[pos_in_directory], dirname) == 0)
			{
				break;
			}
		}
		if (pos_in_directory == DIRECTORY_NUM)
		{
			printf("Open dir error: Directory not found.\n");
			return false;
		}
		tmp_dir_ino = currentDirectory.inodeID[pos_in_directory];
		fseek(fd, INODE_START + tmp_dir_ino * INODE_SIZE, SEEK_SET);
		fread(&tmp_dir_inode, sizeof(inode), 1, fd);
		if(tmp_dir_inode.di_mode == 0) dir_flag = true; 
		else pos_in_directory++;
	} while (!dir_flag);
	if (userID == tmp_dir_inode.di_uid)
	{
		if (tmp_dir_inode.permission & OWN_X != OWN_X) {
			printf("Open dir error: Access deny.\n");
			return NULL;
		}
	}
	else if (users.groupID[userID] == tmp_dir_inode.di_grp) {
		if (tmp_dir_inode.permission & GRP_X != GRP_X) {
			printf("Open dir error: Access deny.\n");
			return NULL;
		}
	}
	else {
		if (tmp_dir_inode.permission & ELSE_X != ELSE_X) {
			printf("Open dir error: Access deny.\n");
			return NULL;
		}
	}

	directory new_current_dir;
	fseek(fd, DATA_START + tmp_dir_inode.di_addr[0] * BLOCK_SIZE, SEEK_SET);
	fread(&new_current_dir, sizeof(directory), 1, fd);
	currentDirectory = new_current_dir;
	if (dirname[0] == '.' && dirname[1] == 0) {
		dir_pointer;
	}
	else if (dirname[0] == '.' && dirname[1] == '.' && dirname[2] == 0) {
		if (dir_pointer != 0) dir_pointer--;
	}
	else {
		for (int i = 0; i < 14; i++) {
			ab_dir[dir_pointer][i] = dirname[i];
		}
		dir_pointer++;
	}
	printf("Open a directory successfully!\n");
	return true;
};

/*
*	Print file and directory information under current directory.
*/
void List()
{
	printf("\n     name\tuser\tgroup\tinodeID\tIcount\tsize\tpermission\ttime\n");
	for (int i = 0; i < DIRECTORY_NUM; i++)
	{
		if (strlen(currentDirectory.fileName[i]) > 0)
		{
			inode tmp_inode;
			fseek(fd, INODE_START + currentDirectory.inodeID[i] * INODE_SIZE, SEEK_SET);
			fread(&tmp_inode, sizeof(inode), 1, fd);

			const char* tmp_type = tmp_inode.di_mode == 0 ? "d" : "-";
			const char* tmp_user = users.userName[tmp_inode.di_uid];
			const int tmp_grpID = tmp_inode.di_grp;

			printf("%10s\t%s\t%d\t%d\t%d\t%u\t%s", currentDirectory.fileName[i], tmp_user, tmp_grpID, tmp_inode.i_ino, tmp_inode.icount, tmp_inode.di_size, tmp_type);
			for (int x = 8; x > 0; x--) {
				if (tmp_inode.permission & (1 << x)) {
					if ((x + 1) % 3 == 0) printf("r");
					else if ((x + 1) % 3 == 2) printf("w");
					else printf("x");
				}
				else printf("-");
			}
			if(tmp_inode.permission & 1) printf("x\t");
			else printf("-\t");
			printf("%s\n", tmp_inode.time);
		}
	}

	printf("\n\n");
}

/*
*	Print absolute directory.
*/
void Ab_dir() {
	for (int i = 0; i < dir_pointer; i++)
		printf("%s/", ab_dir[i]);
	printf("\n");
}