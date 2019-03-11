// File-system system calls.

#include <kern/lib/types.h>
#include <kern/lib/debug.h>
#include <kern/lib/pmap.h>
#include <kern/lib/string.h>
#include <kern/lib/trap.h>
#include <kern/lib/spinlock.h>
#include <kern/lib/syscall.h>
#include <kern/thread/PTCBIntro/export.h>
#include <kern/thread/PCurID/export.h>
#include <kern/trap/TSyscallArg/export.h>
#include <kern/lib/spinlock.h>

#include <kern/dev/console.h>

#include "dir.h"
#include "path.h"
#include "file.h"
#include "fcntl.h"
#include "log.h"

#define BUFLEN_MAX 10000
char fsbuf[BUFLEN_MAX];
spinlock_t fsbuf_lk;

void fs_init(void) {
    spinlock_init(&fsbuf_lk);
}

static bool check_buf(tf_t *tf, uintptr_t buf, size_t len, size_t maxlen) {
    if (!(VM_USERLO <= buf && buf + len <= VM_USERHI)
        || (0 < maxlen && maxlen <= len)) {
        syscall_set_errno(tf, E_INVAL_ADDR);
        syscall_set_retval1(tf, -1);
        return FALSE;
    }
    return TRUE;
}

/**
 * This function is not a system call handler, but an auxiliary function
 * used by sys_open.
 * Allocate a file descriptor for the given file.
 * You should scan the list of open files for the current thread
 * and find the first file descriptor that is available.
 * Return the found descriptor or -1 if none of them is free.
 */
static int fdalloc(struct file *f)
{
    int fd;
    int pid = get_curid();
    struct file **openfiles = tcb_get_openfiles(pid);

    for (fd = 0; fd < NOFILE; fd++) {
        if (openfiles[fd] == NULL) {
            tcb_set_openfiles(pid, fd, f);
            return fd;
        }
    }

    return -1;
}

/**
 * From the file indexed by the given file descriptor, read n bytes and save them
 * into the buffer in the user. As explained in the assignment specification,
 * you should first write to a kernel buffer then copy the data into user buffer
 * with pt_copyout.
 * Return Value: Upon successful completion, read() shall return a non-negative
 * integer indicating the number of bytes actually read. Otherwise, the
 * functions shall return -1 and set errno E_BADF to indicate the error.
 */
void sys_read(tf_t *tf)
{

    struct file *file;
    int read;

    int pid = get_curid();
    int fd = syscall_get_arg2(tf);
    uintptr_t buf = syscall_get_arg3(tf);
    size_t buflen = syscall_get_arg4(tf);

    if (!check_buf(tf, buf, buflen, BUFLEN_MAX)) {
        return;
    }
    if (!(0 <= fd && fd < NOFILE)) {
        syscall_set_errno(tf, E_BADF);
        syscall_set_retval1(tf, -1);
        return;
    }

    file = tcb_get_openfiles(pid)[fd];
    if (file == NULL || file->type != FD_INODE) {
        syscall_set_errno(tf, E_BADF);
        syscall_set_retval1(tf, -1);
        return;
    }

    spinlock_acquire(&fsbuf_lk);
    read = file_read(file, fsbuf, buflen);
    syscall_set_retval1(tf, read);
    if (0 <= read) {
        pt_copyout(fsbuf, pid, buf, buflen);
        syscall_set_errno(tf, E_SUCC);
    }
    else {
        syscall_set_errno(tf, E_BADF);
    }
    spinlock_release(&fsbuf_lk);
}

/**
 * Write n bytes of data in the user's buffer into the file indexed by the file descriptor.
 * You should first copy the data info an in-kernel buffer with pt_copyin and then
 * pass this buffer to appropriate file manipulation function.
 * Upon successful completion, write() shall return the number of bytes actually
 * written to the file associated with f. This number shall never be greater
 * than nbyte. Otherwise, -1 shall be returned and errno E_BADF set to indicate the
 * error.
 */
void sys_write(tf_t *tf)
{

    struct file *file;
    int written;

    int pid = get_curid();
    int fd = syscall_get_arg2(tf);
    uintptr_t buf = syscall_get_arg3(tf);
    size_t buflen = syscall_get_arg4(tf);

    if (!check_buf(tf, buf, buflen, BUFLEN_MAX)) {
        return;
    }
    if (!(0 <= fd && fd < NOFILE)) {
        syscall_set_errno(tf, E_BADF);
        syscall_set_retval1(tf, -1);
        return;
    }

    file = tcb_get_openfiles(pid)[fd];
    if (file == NULL || file->type != FD_INODE) {
        syscall_set_errno(tf, E_BADF);
        syscall_set_retval1(tf, -1);
        return;
    }

    spinlock_acquire(&fsbuf_lk);
    pt_copyin(pid, buf, fsbuf, buflen);
    written = file_write(file, fsbuf, buflen);
    spinlock_release(&fsbuf_lk);

    syscall_set_retval1(tf, written);
    if (0 <= written) {
        syscall_set_errno(tf, E_SUCC);
    }
    else {
        syscall_set_errno(tf, E_BADF);
    }
}

/**
 * Return Value: Upon successful completion, 0 shall be returned; otherwise, -1
 * shall be returned and errno E_BADF set to indicate the error.
 */
void sys_close(tf_t *tf)
{

    struct file *file;

    int pid = get_curid();
    int fd = syscall_get_arg2(tf);

    if (!(0 <= fd && fd < NOFILE)) {
        syscall_set_errno(tf, E_BADF);
        syscall_set_retval1(tf, -1);
        return;
    }

    file = tcb_get_openfiles(pid)[fd];
    if (file == NULL || file->ref < 1) {
        syscall_set_errno(tf, E_BADF);
        syscall_set_retval1(tf, -1);
        return;
    }

    file_close(file);
    tcb_set_openfiles(pid, fd, NULL);

    syscall_set_errno(tf, E_SUCC);
    syscall_set_retval1(tf, 0);
}

/**
 * Return Value: Upon successful completion, 0 shall be returned. Otherwise, -1
 * shall be returned and errno E_BADF set to indicate the error.
 */
void sys_fstat(tf_t *tf)
{

    struct file *file;
    struct file_stat fs_stat;

    int pid = get_curid();
    int fd = syscall_get_arg2(tf);
    uintptr_t stat = syscall_get_arg3(tf);

    if (!check_buf(tf, stat, sizeof(struct file_stat), 0)) {
        return;
    }
    if (!(0 <= fd && fd < NOFILE)) {
        syscall_set_errno(tf, E_BADF);
        syscall_set_retval1(tf, -1);
        return;
    }

    file = tcb_get_openfiles(pid)[fd];
    if (file == NULL || file->type != FD_INODE) {
        syscall_set_errno(tf, E_BADF);
        syscall_set_retval1(tf, -1);
        return;
    }

    if (file_stat(file, &fs_stat) == 0) {
        pt_copyout(&fs_stat, pid, stat, sizeof(struct file_stat));
        syscall_set_errno(tf, E_SUCC);
        syscall_set_retval1(tf, 0);
    }
    else {
        syscall_set_errno(tf, E_BADF);
        syscall_set_retval1(tf, -1);
    }
}

/**
 * Create the path new as a link to the same inode as old.
 */
void sys_link(tf_t * tf)
{
    char name[DIRSIZ], new[128], old[128];
    struct inode *dp, *ip;

    uintptr_t uold = syscall_get_arg2(tf);
    uintptr_t unew = syscall_get_arg3(tf);
    uintptr_t oldlen = syscall_get_arg4(tf);
    uintptr_t newlen = syscall_get_arg5(tf);

    if (!check_buf(tf, uold, oldlen, 128) || !check_buf(tf, unew, newlen, 128)) {
        return;
    }

    pt_copyin(get_curid(), uold, old, oldlen);
    pt_copyin(get_curid(), unew, new, newlen);

    if ((ip = namei(old)) == 0) {
        syscall_set_errno(tf, E_NEXIST);
        return;
    }

    begin_trans();

    inode_lock(ip);
    if (ip->type == T_DIR) {
        inode_unlockput(ip);
        commit_trans();
        syscall_set_errno(tf, E_DISK_OP);
        return;
    }

    ip->nlink++;
    inode_update(ip);
    inode_unlock(ip);

    if ((dp = nameiparent(new, name)) == 0)
        goto bad;
    inode_lock(dp);
    if (dp->dev != ip->dev || dir_link(dp, name, ip->inum) < 0) {
        inode_unlockput(dp);
        goto bad;
    }
    inode_unlockput(dp);
    inode_put(ip);

    commit_trans();

    syscall_set_errno(tf, E_SUCC);
    return;

bad:
    inode_lock(ip);
    ip->nlink--;
    inode_update(ip);
    inode_unlockput(ip);
    commit_trans();
    syscall_set_errno(tf, E_DISK_OP);
    return;
}

/**
 * Is the directory dp empty except for "." and ".." ?
 */
static int isdirempty(struct inode *dp)
{
    int off;
    struct dirent de;

    for (off = 2 * sizeof(de); off < dp->size; off += sizeof(de)) {
        if (inode_read(dp, (char *) &de, off, sizeof(de)) != sizeof(de))
            KERN_PANIC("isdirempty: readi");
        if (de.inum != 0)
            return 0;
    }
    return 1;
}

void sys_unlink(tf_t *tf)
{
    struct inode *ip, *dp;
    struct dirent de;
    char name[DIRSIZ], path[128];
    uint32_t off;

    uintptr_t buf = syscall_get_arg2(tf);
    size_t buflen = syscall_get_arg3(tf);

    if (!check_buf(tf, buf, buflen, 128)) {
        return;
    }

    pt_copyin(get_curid(), buf, path, buflen);

    if ((dp = nameiparent(path, name)) == 0) {
        syscall_set_errno(tf, E_DISK_OP);
        return;
    }

    begin_trans();

    inode_lock(dp);

    // Cannot unlink "." or "..".
    if (dir_namecmp(name, ".") == 0 || dir_namecmp(name, "..") == 0)
        goto bad;

    if ((ip = dir_lookup(dp, name, &off)) == 0)
        goto bad;
    inode_lock(ip);

    if (ip->nlink < 1)
        KERN_PANIC("unlink: nlink < 1");
    if (ip->type == T_DIR && !isdirempty(ip)) {
        inode_unlockput(ip);
        goto bad;
    }

    memset(&de, 0, sizeof(de));
    if (inode_write(dp, (char *) &de, off, sizeof(de)) != sizeof(de))
        KERN_PANIC("unlink: writei");
    if (ip->type == T_DIR) {
        dp->nlink--;
        inode_update(dp);
    }
    inode_unlockput(dp);

    ip->nlink--;
    inode_update(ip);
    inode_unlockput(ip);

    commit_trans();

    syscall_set_errno(tf, E_SUCC);
    return;

bad:
    inode_unlockput(dp);
    commit_trans();
    syscall_set_errno(tf, E_DISK_OP);
    return;
}

static struct inode *create(char *path, short type, short major, short minor)
{
    uint32_t off;
    struct inode *ip, *dp;
    char name[DIRSIZ];

    if ((dp = nameiparent(path, name)) == 0)
        return 0;
    inode_lock(dp);

    if ((ip = dir_lookup(dp, name, &off)) != 0) {
        inode_unlockput(dp);
        inode_lock(ip);
        if (type == T_FILE && ip->type == T_FILE)
            return ip;
        inode_unlockput(ip);
        return 0;
    }

    if ((ip = inode_alloc(dp->dev, type)) == 0)
        KERN_PANIC("create: ialloc");

    inode_lock(ip);
    ip->major = major;
    ip->minor = minor;
    ip->nlink = 1;
    inode_update(ip);

    if (type == T_DIR) {  // Create . and .. entries.
        dp->nlink++;      // for ".."
        inode_update(dp);
        // No ip->nlink++ for ".": avoid cyclic ref count.
        if (dir_link(ip, ".", ip->inum) < 0
            || dir_link(ip, "..", dp->inum) < 0)
            KERN_PANIC("create dots");
    }

    if (dir_link(dp, name, ip->inum) < 0)
        KERN_PANIC("create: dir_link");

    inode_unlockput(dp);
    return ip;
}

void sys_open(tf_t *tf)
{
    char path[128];
    int fd, omode;
    struct file *f;
    struct inode *ip;

    uintptr_t buf = syscall_get_arg2(tf);
    size_t buflen = syscall_get_arg4(tf);

    if (!check_buf(tf, buf, buflen, 128)) {
        return;
    }

    static int first = TRUE;
    if (first) {
        first = FALSE;
        log_init();
    }

    pt_copyin(get_curid(), buf, path, buflen);
    omode = syscall_get_arg3(tf);

    if (omode & O_CREATE) {
        begin_trans();
        ip = create(path, T_FILE, 0, 0);
        commit_trans();
        if (ip == 0) {
            syscall_set_retval1(tf, -1);
            syscall_set_errno(tf, E_CREATE);
            return;
        }
    } else {
        if ((ip = namei(path)) == 0) {
            syscall_set_retval1(tf, -1);
            syscall_set_errno(tf, E_NEXIST);
            return;
        }
        inode_lock(ip);
        if (ip->type == T_DIR && omode != O_RDONLY) {
            inode_unlockput(ip);
            syscall_set_retval1(tf, -1);
            syscall_set_errno(tf, E_DISK_OP);
            return;
        }
    }

    if ((f = file_alloc()) == 0 || 
	(fd = fdalloc(f)) < 0) {
        if (f)
            file_close(f);
        inode_unlockput(ip);
        syscall_set_retval1(tf, -1);
        syscall_set_errno(tf, E_DISK_OP);
        return;
    }
    inode_unlock(ip);

    f->type = FD_INODE;
    f->ip = ip;
    f->off = 0;
    f->readable = !(omode & O_WRONLY);
    f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
    syscall_set_retval1(tf, fd);
    syscall_set_errno(tf, E_SUCC);
}

void sys_mkdir(tf_t *tf)
{
    char path[128];
    struct inode *ip;
    int pathSize = syscall_get_arg3(tf) + 1;
	//dprintf("path is %s", path);

    if (pathSize>128) {syscall_set_errno(tf,E_EXCEED_BUF);return;}

    uintptr_t buf = syscall_get_arg2(tf);
    size_t buflen = syscall_get_arg3(tf);

    if (!check_buf(tf, buf, buflen, 128)) {
        return;
    }

    pt_copyin(get_curid(), buf, path, buflen);

    begin_trans();
    if ((ip = (struct inode *) create(path, T_DIR, 0, 0)) == 0) {
        commit_trans();
        syscall_set_errno(tf, E_DISK_OP);
        return;
    }
    inode_unlockput(ip);
    commit_trans();
    syscall_set_errno(tf, E_SUCC);
}

void sys_chdir(tf_t *tf)
{
    char path[128];
    struct inode *ip;
    int pid = get_curid();
    int pathSize = syscall_get_arg3(tf) + 1;


    uintptr_t buf = syscall_get_arg2(tf);
    size_t buflen = syscall_get_arg3(tf);

    if (!check_buf(tf, buf, buflen, 128)) {
        return;
    }

    pt_copyin(get_curid(), buf, path, buflen);

    if ((ip = namei(path)) == 0) {
        syscall_set_errno(tf, E_DISK_OP);
        return;
    }
    inode_lock(ip);
    if (ip->type != T_DIR) {
        inode_unlockput(ip);
        syscall_set_errno(tf, E_DISK_OP);
        return;
    }
    inode_unlock(ip);
    inode_put(tcb_get_cwd(pid));
    tcb_set_cwd(pid, ip);
    syscall_set_errno(tf, E_SUCC);
}

void sys_getc(tf_t *tf)
{
	syscall_set_retval1(tf, cons_getc());
	syscall_set_errno(tf, E_SUCC);
}

void sys_putc(tf_t *tf)
{
	char charc = syscall_get_arg2(tf);// or whichever is char to put 
	
	if (charc!=0)
	cons_putc(charc);	

	syscall_set_retval1(tf, charc); //tryint to make it return the val

	syscall_set_errno(tf, E_SUCC);

}



/*Notes: first entry of inode is '.' and second is ".."*/
void sys_pwd(tf_t *tf) 
{
	//stack variables
	struct dirent de;
	uint16_t childiNum;
	
	char revWorkingDir[256];
	char workingDir[256];

	uint16_t parentiNum;
	uint32_t parentOff=sizeof(de);
	uint32_t off;
	unsigned int rwdPtr=0; //pointing to reversed result string
	unsigned int breakWhile=0;

	//get cur inode
	int cur_pid = get_curid();

	//correct?
	struct inode* temp= tcb_get_cwd(cur_pid);
	

	//read current inode
	inode_lock(temp);
	inode_read(temp, (char *) &de, 0, sizeof(de)); //fills de w . itself
	inode_unlock(temp); //does this need to be ulput because i used tcb get cwd?

	//current node is root, simplest case 
	if (de.inum==ROOTINO) {dprintf("/\n"); syscall_set_errno(tf, E_SUCC);return;}

	//current rode is not root need to cycle up the tree and build print string
	while (de.inum!=ROOTINO)
	{	
		//get child's inode num while we have it
		childiNum = de.inum;

		//FILL de w PARENT (one de offset into iNode)
		inode_lock(temp);
		inode_read(temp, (char *) &de, parentOff, sizeof(de));
		inode_unlock(temp);
			
		//de now has parent's inum 
		parentiNum = de.inum;
	
		//get parent inode and read thru it with inode_read
		struct inode* tempParent=inode_get(temp->dev,parentiNum); //remember get does ++
		
		//now we need to read the parent's inodes to find the name of the current inode temp
		for (off = 2*sizeof(de); off < tempParent->size; off +=sizeof(de))
		{
			inode_read(tempParent, (char*) &de, off,sizeof(de)); //to fill de

			//found child entry in parent inode
			if (de.inum==childiNum)
			{
				//dprintf("FOUND CUR INODE'S ENTRY IN ITS PARENT'S ENTRIES \n");
				//here de has child entry in parent inode
				//add de.name[DIRSIZ] to revWorkingdir backwards
				
				//get to end of de.name first
				unsigned int nameEnd=0;

				while (de.name[nameEnd]!=0) {nameEnd++;}
				//nameEnd is the number of letters in name w/o nullterm
				
				unsigned int ptr;
				for (ptr=0;ptr<nameEnd;ptr++)
				{
					revWorkingDir[rwdPtr]= de.name[nameEnd-ptr-1];
					rwdPtr++;
				}
				//rwdptr at char after last char in reverse string

				//append slash between names 
				revWorkingDir[rwdPtr]='/';
				//now rwdPtr at final slash
				rwdPtr++; //at next empty space 

				//now that current inode's child's name written,make temp it's parent n repeat
				temp=tempParent;

				//at this point we have given temp the parent root
				//if temp is root at this point
				// we do not need to read its parent for its name. break loop and append / , done
				if (temp->inum==ROOTINO) {breakWhile=1; break;}
			}
		}	//done searching the new parent

		//breakwhile takes us here
		if (breakWhile==1) break;

	} //end of while loop
	
	//temp is root by here
	//revWorkingDir[rwdPtr]='/'; rwdPtr++;

	//reverse string, start at rwdPtr which at next empty space 
	unsigned int wdPtr=0;
	
	//dprintf("before while loop rwdPtr is %d\n", rwdPtr);
	while (rwdPtr>=0) 
	{
		//dprintf("current output string is %s\n", workingDir);
		rwdPtr--; //its at an empty space
		workingDir[wdPtr] = revWorkingDir[rwdPtr];
		wdPtr++;
		
		if (rwdPtr==0) {break;} //done here 		 
	}

	//wdptr right at next empty spot, append null term
	//add nullterminal to  regular workingDir here
	workingDir[wdPtr]=0;
	syscall_set_errno(tf, E_SUCC);
	dprintf("%s\n", workingDir); 
}



//system call for ls
void sys_ls(tf_t *tf)
{
    //list names of all the dirs + files in the cur inode
	struct dirent de;	
	unsigned int off;
	//unsigned int numArgs= syscall_get_arg2(tf);

	//get cur inode
	int cur_pid = get_curid();
	struct inode* ls_node= tcb_get_cwd(cur_pid);
	
	//dprintf("numInputs is %d\n", numArgs);
	dprintf("\n");
	for (int i=0;i<NDIRECT+1;i++)
	{
	    uint32_t curDirectPointer= ls_node->addrs[i];
	    //dprintf("curdp num %d is %d\n",i,curDirectPointer);

		if (curDirectPointer!=0) 
		{
			//search the lsnode for this inum
			for (off = 2*sizeof(de); off < ls_node->size; off +=sizeof(de))			
			{
				//fill de
				inode_lock(ls_node);
				inode_read(ls_node, (char*) &de, off,sizeof(de)); 
				inode_unlock(ls_node);			

				//print de.name
				if (*de.name!=0) dprintf("%s\n", de.name);

				//if (curDirectPointer==de.inum) {dprintf("%s\n", de.name);}
		
			}
		}
			
	}

	syscall_set_errno(tf, E_SUCC);
	//maybe return;
}

/*
void sys_cat(tf_t *tf)
{
	unsigned int fd = syscall_get_arg2(tf);
	int cur_pid = get_curid();
	struct file ** openFiles = tcb_get_openfiles(cur_pid);
    struct file* toPrintFile= openFiles[fd];

	//print the contents of toPrintFile
	char* printBuf;
	//int n is last arg if inode re
	file_read(struct file *f, printBuf, int n);
	dprintf("file contents are:\n %s",printBuf);
}*/

void sys_rm(tf_t *tf)
{
	char* toRm = syscall_get_arg2(tf);
	unsigned int rflag = syscall_get_arg3(tf);

	//can figure out how to do this later..
	if (rflag)
	{
		//recursive remove;
	}
	else
	{
		//regular nonrecursive remove
		;
	}

}


void sys_cp(tf_t *tf)
{
	char* dir1 = syscall_get_arg2(tf);
	char* dir2 = syscall_get_arg3(tf);
	unsigned int rflag = syscall_get_arg4(tf);

}
