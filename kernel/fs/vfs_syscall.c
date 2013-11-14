/*
 *  FILE: vfs_syscall.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Wed Apr  8 02:46:19 1998
 *  $Id: vfs_syscall.c,v 1.1 2012/10/10 20:06:46 william Exp $
 */

#include "kernel.h"
#include "errno.h"
#include "globals.h"
#include "fs/vfs.h"
#include "fs/file.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/fcntl.h"
#include "fs/lseek.h"
#include "mm/kmalloc.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/stat.h"
#include "util/debug.h"

/* To read a file:
 *      o fget(fd)
 *      o call its virtual read f_op
 *      o update f_pos
 *      o fput() it
 *      o return the number of bytes read, or an error
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for reading.
 *      o EISDIR
 *        fd refers to a directory.
 *
 * In all cases, be sure you do not leak file refcounts by returning before
 * you fput() a file that you fget()'ed.
 */
int
do_read(int fd, void *buf, size_t nbytes)
{
        /* let the file ponter using the fd */
          file_t *fp = fget(fd);
          int err = 0;

          int bytes_read = 0;
          if(NULL == fp)
          {
                /* not a valid file */

                return -EBADF;
          }
          if((fp->f_mode & 1) != FMODE_READ)
          {
                  fput(fp);
                  return -EBADF;
          }
          if(fp->f_vnode && S_ISDIR(fp->f_vnode->vn_mode))
          {
                err = -EISDIR;
                fput(fp);
                return err;
          }
          /* a valid file */
          bytes_read = fp->f_vnode->vn_ops->read(fp->f_vnode,fp->f_pos,buf,nbytes);
          if(bytes_read == 0)
          {
                /* need to check which error to return */
                return bytes_read;
          }
          if (NULL != fp)
          {
                fp->f_pos=fp->f_pos + bytes_read;
                fput(fp);
          }
          return bytes_read;
}

/* Very similar to do_read.  Check f_mode to be sure the file is writable.  If
 * f_mode & FMODE_APPEND, do_lseek() to the end of the file, call the write
 * f_op, and fput the file.  As always, be mindful of refcount leaks.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for writing.
 */
int
do_write(int fd, const void *buf, size_t nbytes)
{

          file_t *fp = fget(fd);
          int err;
        if(NULL == fp)
        {
                return -EBADF;
        }
        if((fp->f_mode & 2) != FMODE_WRITE)
        {
          fput(fp);
          return -EBADF;
        }
          if(fp->f_vnode && S_ISDIR(fp->f_vnode->vn_mode))
          {
                err = -EISDIR;
                return err;
          }
          int bytes_wrote = 0;
          if((fp->f_mode & FMODE_APPEND) == FMODE_APPEND)
          {
        	  fp->f_pos =  do_lseek(fd, 0, SEEK_END);
        	  	 /* if (0 > fp->f_pos)
                {
                  fput(fp);
                  return -1;  need to check this value 
                }*/

          }

          if((fp->f_mode & FMODE_WRITE) == FMODE_WRITE)
          {

		   bytes_wrote = fp->f_vnode->vn_ops->write(fp->f_vnode,fp->f_pos,buf,nbytes);
		   if(bytes_wrote > 0) {	
		   fp->f_pos = fp->f_pos + bytes_wrote;
		   }
          /*if(0 > bytes_wrote )
          {
                return bytes_wrote;
          }
          else
          {*/
        	  KASSERT((S_ISCHR(fp->f_vnode->vn_mode)) ||
					  (S_ISBLK(fp->f_vnode->vn_mode)) ||
					  ((S_ISREG(fp->f_vnode->vn_mode)) &&
					  (fp->f_pos <= fp->f_vnode->vn_len)));
			  dbg(DBG_PRINT, "(GRADING2A 3.a)It is either a character or block device file or a regular file whose file position is less than the length of the file\n ");





          if (NULL != fp)
          {
                fput(fp);
          }
          }
          else
          {
        	  fput(fp);
        	  return -EBADF;
          }
          return bytes_wrote;

}

/*
 * Zero curproc->p_files[fd], and fput() the file. Return 0 on success
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't a valid open file descriptor.
 */
int
do_close(int fd)
{
        file_t *fp=NULL;
        if(fd < 0) {
                return -EBADF;
        }
        fp = fget(fd);
        if(fp == NULL)
        {
                /*fput(fp);*/
                return -EBADF;
        }
         if(NULL == curproc->p_files[fd])
         {
           return -EBADF;
         }

         curproc->p_files[fd] = NULL;
         fput(fp);
	 fput(fp);
         return 0;
}

/* To dup a file:
 *      o fget(fd) to up fd's refcount
 *      o get_empty_fd()
 *      o point the new fd to the same file_t* as the given fd
 *      o return the new file descriptor
 *
 * Don't fput() the fd unless something goes wrong.  Since we are creating
 * another reference to the file_t*, we want to up the refcount.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't an open file descriptor.
 *      o EMFILE
 *        The process already has the maximum number of file descriptors open
 *        and tried to open a new one.
 */
int
do_dup(int fd)
{
        if(fd < 0){
                return -EBADF;
        }
          file_t *fp = fget(fd);
          int new_fd = 0;
          int err = 0;
          if(NULL == fp)
          {
                /* not a valid file */

                return -EBADF;
          }
          if(fp->f_vnode && S_ISDIR(fp->f_vnode->vn_mode))
          {
                err = -EISDIR;
                return err;
          }
          new_fd = get_empty_fd(curproc);
          if(-EMFILE == new_fd)
          {
                return -EMFILE;
          }
          curproc->p_files[new_fd] = curproc->p_files[fd];
          return new_fd;

}

/* Same as do_dup, but insted of using get_empty_fd() to get the new fd,
 * they give it to us in 'nfd'.  If nfd is in use (and not the same as ofd)
 * do_close() it first.  Then return the new file descriptor.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        ofd isn't an open file descriptor, or nfd is out of the allowed
 *        range for file descriptors.
 */
int
do_dup2(int ofd, int nfd)
{
        if(ofd < 0 || nfd < 0 || ofd >= NFILES || nfd >= NFILES){
                return -EBADF;
        }
          file_t *fp = fget(ofd);
          int err = 0;
          if(NULL == fp)
          {
                /* not a valid file */
                return -EBADF;
          }

          if(nfd == ofd)
          {
                fput(fp);
                return nfd;
          }
          if(fp->f_vnode && S_ISDIR(fp->f_vnode->vn_mode))
          {
                err = -EISDIR;
                return err;
          }
          if(NULL != curproc->p_files[nfd])
          {
                err = do_close(nfd);
                if(0 > err)
                {
                        fput(fp);
                        return err;
                }
          }
          if(-EMFILE == nfd)
          {
                return EMFILE;
          }

          curproc->p_files[nfd] = curproc->p_files[ofd];
          return nfd;

}

/*
 * This routine creates a special file of the type specified by 'mode' at
 * the location specified by 'path'. 'mode' should be one of S_IFCHR or
 * S_IFBLK (you might note that mknod(2) normally allows one to create
 * regular files as well-- for simplicity this is not the case in Weenix).
 * 'devid', as you might expect, is the device identifier of the device
 * that the new special file should represent.
 *
 * You might use a combination of dir_namev, lookup, and the fs-specific
 * mknod (that is, the containing directory's 'mknod' vnode operation).
 * Return the result of the fs-specific mknod, or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        mode requested creation of something other than a device special
 *        file.
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_mknod(const char *path, int mode, unsigned devid)
{

        if(mode != S_IFCHR ||  strlen(path) < 1)
        {
                return -EINVAL;
        }
        if(strlen(path) > MAXPATHLEN)
        {
                return -ENAMETOOLONG;
        }

        const char *name;
        size_t namelength=0;
        vnode_t *res_vnode=NULL;
        int err = dir_namev(path,&namelength,&name,NULL,&res_vnode);
        if ( err < 0 )
        {
                        return err;
        }


        err = lookup(res_vnode,name,namelength,&res_vnode);
        if(err ==0)
        {
                vput(res_vnode);
                return -EEXIST;
        }
        else if(err== -ENOENT)
        {
                KASSERT(NULL != res_vnode->vn_ops->mknod);
                dbg(DBG_PRINT, "(GRADING2A 3.b)mknod operation on vnode is present\n ");
                err = res_vnode->vn_ops->mknod(res_vnode,name,namelength,mode,devid);
                return err;
        }
        else
                return err;


}

/* Use dir_namev() to find the vnode of the dir we want to make the new
 * directory in.  Then use lookup() to make sure it doesn't already exist.
 * Finally call the dir's mkdir vn_ops. Return what it returns.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_mkdir(const char *path)
{
    /*NOT_YET_IMPLEMENTED("VFS: do_mkdir");*/
        int ret = 0;
        size_t namelen=0;
        const char *name=NULL;
        vnode_t *res_vnode=NULL;
        if(strlen(path) > MAXPATHLEN)
        {
                return -ENAMETOOLONG;
        }
        ret = dir_namev(path, &namelen, &name, NULL, &res_vnode );
        if(ret < 0) {
                return ret;
        } else if(namelen > NAME_LEN) {
                vput(res_vnode);
                return -ENAMETOOLONG;
        } else if(res_vnode == NULL) {
                return -ENOTDIR;
        }

        vnode_t *result=NULL;
        ret = lookup(res_vnode, name, namelen, &result);
        if( ret == 0 ) {
                vput(res_vnode);
                vput(result);
                return -EEXIST;
        }
        else if(-ENOENT== ret)
        {
                KASSERT(NULL != (res_vnode)->vn_ops->mkdir);
                dbg(DBG_PRINT, "(GRADING2A 3.c)mkdir operation on vnode is present\n ");
                ret = (res_vnode)->vn_ops->mkdir(res_vnode,name,namelen);
                vput(res_vnode);
                return ret;
        }
        return ret;

}

/* Use dir_namev() to find the vnode of the directory containing the dir to be
 * removed. Then call the containing dir's rmdir v_op.  The rmdir v_op will
 * return an error if the dir to be removed does not exist or is not empty, so
 * you don't need to worry about that here. Return the value of the v_op,
 * or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        path has "." as its final component.
 *      o ENOTEMPTY
 *        path has ".." as its final component.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_rmdir(const char *path)
{
    /*NOT_YET_IMPLEMENTED("VFS: do_rmdir");*/
        int ret=0;
        size_t namelen=0;
        const char *name=NULL;
        vnode_t *res_vnode=NULL;

        ret = dir_namev(path, &namelen, &name, NULL, &res_vnode);
        if (ret < 0) {
                return ret;
        } else if (!strcmp(name, ".")) {
                vput(res_vnode);
                return -EINVAL;
        } else if( !strcmp(name,"..") ) {
                vput(res_vnode);
                return -ENOTEMPTY;
        } else if (res_vnode == NULL) {
                return -ENOTDIR;
        } else if (namelen > NAME_LEN) {
                vput(res_vnode);
                return -ENAMETOOLONG;
        }

        vnode_t *result;

        ret =lookup(res_vnode, name, namelen, &result) ;
        if (ret!= 0)
        {
                vput(res_vnode);
                return ret;
        }
        KASSERT(NULL != res_vnode->vn_ops->rmdir);
        dbg(DBG_PRINT, "(GRADING2A 3.d)rmdir operation on vnode is present\n ");

        ret=(res_vnode)->vn_ops->rmdir(res_vnode,name,namelen);
        vput(res_vnode);
        return ret;
}

/*
 * Same as do_rmdir, but for files.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EISDIR
 *        path refers to a directory.
 *      o ENOENT
 *        A component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_unlink(const char *path)
{
        /* NOT_YET_IMPLEMENTED("VFS: do_unlink"); */
        const char *name;
        size_t len = 0;
        int returnval = 0;
        vnode_t *res_childvnode = NULL;
        vnode_t *res_parentvnode = NULL;
        if (strlen(path) > MAXPATHLEN) {
                return -ENAMETOOLONG;
        }

        returnval = dir_namev(path, &len, &name, NULL, &res_parentvnode);
        if (returnval < 0) {
                vput(res_parentvnode);
                return returnval;
        }

        returnval = lookup(res_parentvnode, name, len, &res_childvnode);
        if (returnval < 0) {
                vput(res_parentvnode);
                return returnval;
        }

        if (S_ISDIR(res_childvnode->vn_mode)) {

                vput(res_parentvnode);
                vput(res_childvnode);
                return -EISDIR;
        }

        KASSERT(NULL != res_parentvnode->vn_ops->unlink);
        dbg(DBG_PRINT, "(GRADING2A 3.e)unlink operation on vnode is present\n ");

        returnval = res_parentvnode->vn_ops->unlink(res_parentvnode, name, len);
        vput(res_childvnode);
        vput(res_parentvnode);
        return returnval;
}

/* To link:
 *      o open_namev(from)
 *      o dir_namev(to)
 *      o call the destination dir's (to) link vn_ops.
 *      o return the result of link, or an error
 *
 * Remember to vput the vnodes returned from open_namev and dir_namev.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        to already exists.
 *      o ENOENT
 *        A directory component in from or to does not exist.
 *      o ENOTDIR
 *        A component used as a directory in from or to is not, in fact, a
 *        directory.
 *      o ENAMETOOLONG
 *        A component of from or to was too long.
 */
int
do_link(const char *from, const char *to)
{
        /* NOT_YET_IMPLEMENTED("VFS: do_link");*/
        const char *name;
        vnode_t *res_parentvnode = NULL;
        vnode_t *res_childvnode = NULL;
        size_t len = 0;
        int returnval = 0;
        int flag = 0;
        if (strlen(from) > MAXPATHLEN) {
                return -ENAMETOOLONG;
        }
        if (strlen(to) > MAXPATHLEN) {
                return -ENAMETOOLONG;
        }
        returnval = open_namev(from, flag, &res_parentvnode, NULL);
        if (returnval < 0) {
                return returnval;
        }

        returnval = dir_namev(to, &len, &name, NULL, &res_childvnode);
        if (returnval < 0) {
                vput(res_parentvnode);
                return returnval;
        }
        vput(res_childvnode);
        returnval = lookup(res_childvnode, name, len, &res_childvnode);
        if (!S_ISDIR(res_childvnode->vn_mode)) {
                vput(res_childvnode);
                return -EISDIR;
        }

        if(0==returnval)
        {
                vput(res_childvnode);
                vput(res_parentvnode);
                return -EEXIST;
        }
        else if(-ENOENT==returnval)
        {
                returnval = res_childvnode->vn_ops->link(res_parentvnode,
                res_childvnode, name, len);
                vput(res_parentvnode);
                return returnval;
        }
        else
        {
                vput(res_parentvnode);
                return returnval;
        }
        return -1;
}

/*      o link newname to oldname
 *      o unlink oldname
 *      o return the value of unlink, or an error
 *
 * Note that this does not provide the same behavior as the
 * Linux system call (if unlink fails then two links to the
 * file could exist).
 */
int
do_rename(const char *oldname, const char *newname)
{
        if(strlen(oldname) > 1 || strlen(newname) > 1)
        {
                  do_link(oldname, newname);
              return do_unlink(oldname);

        }
        return -EINVAL;

}

/* Make the named directory the current process's cwd (current working
 * directory).  Don't forget to down the refcount to the old cwd (vput()) and
 * up the refcount to the new cwd (open_namev() or vget()). Return 0 on
 * success.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        path does not exist.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 *      o ENOTDIR
 *        A component of path is not a directory.
 */
int
do_chdir(const char *path)
{
                vnode_t *res_vnode;
                if(strlen(path)<1)
                {
                        return -EINVAL;

                }
                int err=open_namev(path,0,&res_vnode,NULL);
                if(0 > err )
                 {
                         return err;
                 }

                if(!S_ISDIR(res_vnode->vn_mode))
                {
                                vput(res_vnode);

                                return -ENOTDIR;
                }

                vput(curproc->p_cwd);


                curproc->p_cwd = res_vnode;
                /*vref(res_vnode);*/
                return 0;

}

/* Call the readdir f_op on the given fd, filling in the given dirent_t*.
 * If the readdir f_op is successful, it will return a positive value which
 * is the number of bytes copied to the dirent_t.  You need to increment the
 * file_t's f_pos by this amount.  As always, be aware of refcounts, check
 * the return value of the fget and the virtual function, and be sure the
 * virtual function exists (is not null) before calling it.
 *
 * Return either 0 or sizeof(dirent_t), or -errno.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        Invalid file descriptor fd.
 *      o ENOTDIR
 *        File descriptor does not refer to a directory.
 */
int
do_getdent(int fd, struct dirent *dirp)
{
        if (fd < 0 || fd > NFILES)
                return -EBADF;
        file_t *fileDescriptor = fget(fd);
        if (fileDescriptor != NULL) {
                if (!S_ISDIR(fileDescriptor->f_vnode->vn_mode)) {
                        fput(fileDescriptor);
                        return -ENOTDIR;
                }
                else {
                        int num_bytes = fileDescriptor->f_vnode->vn_ops->readdir(
                                        fileDescriptor->f_vnode, fileDescriptor->f_pos, dirp);
                        if (num_bytes == 0) {
                                return 0;
                        }
                        fileDescriptor->f_pos += num_bytes;
                        fput(fileDescriptor);
                        return sizeof(*dirp);
                }

        }
        return -EBADF;

}

/*
 * Modify f_pos according to offset and whence.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not an open file descriptor.
 *      o EINVAL
 *        whence is not one of SEEK_SET, SEEK_CUR, SEEK_END; or the resulting
 *        file offset would be negative.
 */
int
do_lseek(int fd, int offset, int whence)
{
         /*NOT_YET_IMPLEMENTED("VFS: do_lseek");*/
                if(fd < 0) {
                        return -EBADF;
                }
                file_t *filepointer=NULL;
                filepointer=fget(fd);
		if(filepointer == NULL)
		{
		   return -EBADF;
		}

                if(whence!=SEEK_SET && whence !=SEEK_CUR && whence!=SEEK_END)
                {
                        return -EINVAL;
                }
                if(whence==SEEK_SET)
                {
                        filepointer->f_pos = offset;
                }
                else if(whence==SEEK_END)
                {
                   filepointer->f_pos = filepointer->f_vnode->vn_len + offset;
                }
                else if(whence == SEEK_CUR) 
		{
                        filepointer->f_pos += offset;
                }


		  if(0 > filepointer->f_pos)
                  {
                       filepointer->f_pos = 0;
                       fput(filepointer);
                       return -EINVAL;
                  }
                  fput(filepointer);
		return filepointer->f_pos;
                /*fput(filepointer);*/


}

/*
 * Find the vnode associated with the path, and call the stat() vnode operation.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        A component of path does not exist.
 *      o ENOTDIR
 *        A component of the path prefix of path is not a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int do_stat(const char *path, struct stat *buf)
{
        /* NOT_YET_IMPLEMENTED("VFS: do_stat");*/
        int returnval = 0;
        size_t namelength = 0;
        const char *name = NULL;
        vnode_t *res_vnode = NULL;
        if (strlen(path) > MAXPATHLEN) {
                return -ENAMETOOLONG;
        }
        if( strlen(path) == 0) {
                return -EINVAL;
        }
        returnval = dir_namev(path, &namelength, &name, NULL, &res_vnode);

        if (returnval < 0) {
                return returnval;
        }
        vput(res_vnode);

        returnval = lookup(res_vnode, name, namelength, &res_vnode);
        if (returnval == -ENOENT) {
                return returnval;
        }
        else if(returnval == 0)
        {
                KASSERT(NULL != res_vnode->vn_ops->stat);
                dbg(DBG_PRINT, "(GRADING2A 3.f)stat operation on vnode is present\n ");
                returnval = res_vnode->vn_ops->stat(res_vnode, buf);
                vput(res_vnode);
                return returnval;
        }
        return returnval;
}


#ifdef __MOUNTING__
/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutely sure your Weenix is perfect.
 *
 * This is the syscall entry point into vfs for mounting. You will need to
 * create the fs_t struct and populate its fs_dev and fs_type fields before
 * calling vfs's mountfunc(). mountfunc() will use the fields you populated
 * in order to determine which underlying filesystem's mount function should
 * be run, then it will finish setting up the fs_t struct. At this point you
 * have a fully functioning file system, however it is not mounted on the
 * virtual file system, you will need to call vfs_mount to do this.
 *
 * There are lots of things which can go wrong here. Make sure you have good
 * error handling. Remember the fs_dev and fs_type buffers have limited size
 * so you should not write arbitrary length strings to them.
 */
int
do_mount(const char *source, const char *target, const char *type)
{
        NOT_YET_IMPLEMENTED("MOUNTING: do_mount");
        return -EINVAL;
}

/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutley sure your Weenix is perfect.
 *
 * This function delegates all of the real work to vfs_umount. You should not worry
 * about freeing the fs_t struct here, that is done in vfs_umount. All this function
 * does is figure out which file system to pass to vfs_umount and do good error
 */
int
do_umount(const char *target)
{
        NOT_YET_IMPLEMENTED("MOUNTING: do_umount");
        return -EINVAL;
}
#endif
