/*
 *  FILE: open.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Mon Apr  6 19:27:49 1998
 */

#include "globals.h"
#include "errno.h"
#include "fs/fcntl.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/vfs.h"
#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/stat.h"
#include "util/debug.h"

/* find empty index in p->p_files[] */
int
get_empty_fd(proc_t *p)
{
        int fd;

        for (fd = 0; fd < NFILES; fd++) {
                if (!p->p_files[fd])
                        return fd;
        }

        dbg(DBG_ERROR | DBG_VFS, "ERROR: get_empty_fd: out of file descriptors "
            "for pid %d\n", curproc->p_pid);
        return -EMFILE;
}

/*
 * There a number of steps to opening a file:
 *      1. Get the next empty file descriptor.
 *      2. Call fget to get a fresh file_t.
 *      3. Save the file_t in curproc's file descriptor table.
 *      4. Set file_t->f_mode to OR of FMODE_(READ|WRITE|APPEND) based on
 *         oflags, which can be O_RDONLY, O_WRONLY or O_RDWR, possibly OR'd with
 *         O_APPEND.
 *      5. Use open_namev() to get the vnode for the file_t.
 *      6. Fill in the fields of the file_t.
 *      7. Return new fd.
 *
 * If anything goes wrong at any point (specifically if the call to open_namev
 * fails), be sure to remove the fd from curproc, fput the file_t and return an
 * error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        oflags is not valid.
 *      o EMFILE
 *        The process already has the maximum number of files open.
 *      o ENOMEM
 *        Insufficient kernel memory was available.
 *      o ENAMETOOLONG
 *        A component of filename was too long.
 *      o ENOENT
 *        O_CREAT is not set and the named file does not exist.  Or, a
 *        directory component in pathname does not exist.
 *      o EISDIR
 *        pathname refers to a directory and the access requested involved
 *        writing (that is, O_WRONLY or O_RDWR is set).
 *      o ENXIO
 *        pathname refers to a device special file and no corresponding device
 *        exists.
 */

int
do_open(const char *filename, int oflags)
{
	int next_avail_fd = 0;
	file_t *fp = NULL;
	int flag = 0;
	vnode_t *res_vnode = NULL;
	switch (oflags & 3) {
	case O_RDONLY:
	case O_WRONLY:
	case O_RDWR:
		/*flag= oflags & 3;*/
		break;
	default:
		return -EINVAL;
	}
	switch (oflags & 0x700) {
	case O_CREAT:
	case O_TRUNC:
	case O_APPEND:
		flag = (oflags & 0x700);
		break;
	/* default:
		return -EINVAL; */

	}
	if (MAXPATHLEN < strlen(filename)) {
		return -ENAMETOOLONG;
	}
	next_avail_fd = get_empty_fd(curproc);
	if (-EMFILE == next_avail_fd) {
		return -EMFILE;
	}

	/* to get a fresh file */
	fp = fget(-1);
	if (NULL == fp) {
		/* not a valid file */
		return -EBADF;
	}
	int returnerr = open_namev(filename, flag, &fp->f_vnode, NULL);
	if (0 > returnerr) {
		/*fput(curproc->p_files[next_avail_fd]);*/
		return returnerr;
	}

	if (S_ISDIR(fp->f_vnode->vn_mode) &&
            (((oflags & 3) == O_WRONLY) ||
             ((oflags & 3) == O_RDWR))) {
		vput(fp->f_vnode);
		/*fput(curproc->p_files[next_avail_fd]);*/

		return -EISDIR;
	}
	if ((returnerr == 0) &&
	    (flag & O_APPEND) != O_APPEND &&
            (oflags & 3) == O_WRONLY)
	{
	                vput(fp->f_vnode);
	                returnerr = do_unlink(filename);
	                if ( returnerr < 0 )
	                {
	                        fput(curproc->p_files[next_avail_fd]);
	                        return returnerr;
	                }
	                returnerr = open_namev(filename,O_CREAT,&res_vnode,NULL);
	                if ( returnerr < 0 )
	                {
	                        fput(curproc->p_files[next_avail_fd]);
	                        return returnerr;
	                }
	}


	curproc->p_files[next_avail_fd] = fp;

	if ( (oflags & 0x700) ==O_APPEND)
	{

	   curproc->p_files[next_avail_fd]->f_mode = FMODE_APPEND;
	   /* curproc->p_files[next_avail_fd]->f_pos = fp->f_vnode->vn_len;  */
	}
	else{
	  curproc->p_files[next_avail_fd]->f_mode = 0;
	  curproc->p_files[next_avail_fd]->f_pos = 0;

	}
	if ( (oflags & 3) == O_RDONLY )
	{
   	  curproc->p_files[next_avail_fd]->f_mode |= FMODE_READ;
	}
	else if ( (oflags & 3) == O_WRONLY )
	{
	curproc->p_files[next_avail_fd]->f_mode |= FMODE_WRITE;
	}
	else
	{
	  curproc->p_files[next_avail_fd]->f_mode |= FMODE_WRITE;
	  curproc->p_files[next_avail_fd]->f_mode |= FMODE_READ;
	}
	return next_avail_fd;
}
