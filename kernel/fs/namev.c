#include "kernel.h"
#include "globals.h"
#include "types.h"
#include "errno.h"

#include "util/string.h"
#include "util/printf.h"
#include "util/debug.h"

#include "fs/dirent.h"
#include "fs/fcntl.h"
#include "fs/stat.h"
#include "fs/vfs.h"
#include "fs/vnode.h"

/* This takes a base 'dir', a 'name', its 'len', and a result vnode.
 * Most of the work should be done by the vnode's implementation
 * specific lookup() function, but you may want to special case
 * "." and/or ".." here depnding on your implementation.
 *
 * If dir has no lookup(), return -ENOTDIR.
 *
 * Note: returns with the vnode refcount on *result incremented.
 */
int
lookup(vnode_t *dir, const char *name, size_t len, vnode_t **result)
{
    /*NOT_YET_IMPLEMENTED("VFS: lookup");l*/
	KASSERT(NULL != dir);
	dbg(DBG_PRINT, "(GRADING2A 2.a)Input parameter vnode is not null\n ");
	KASSERT(NULL != name);
	dbg(DBG_PRINT, "(GRADING2A 2.a)Input parameter name not null\n ");
	KASSERT(NULL != result);
	dbg(DBG_PRINT, "(GRADING2A 2.a)Result vnode pointer not null\n ");
	int ret=0;

	if(!S_ISDIR(dir->vn_mode))
	{
		return -ENOTDIR;
	}
	if(strlen(name) == 0)
	{
	  *result = dir;
	  vref(*result);
	  return 0;
	}
	/*special case for "." and ".."*/
	/*if(!strncmp(name,".",len)) {
		(*result) = dir;
		return 0;
	} else if(!strncmp(name,"..",len)) {
		set result with its parent
		return 0;
	}*/

	if(dir->vn_ops->lookup == NULL) {
		return -ENOTDIR;
	}
    	/*vfs_is_in_use(vfs_root_vn->vn_fs);*/
	ret = dir->vn_ops->lookup(dir,name,len,result);


	/*if(ret < 0){
		return ret;
	}
	if(len == 0) {
	 *result = vget(dir->vn_fs,dir->vn_vno);
	}*/
	/*vfs_is_in_use(vfs_root_vn->vn_fs);*/
	/*vref(result);*/
	return ret;
}


/* When successful this function returns data in the following "out"-arguments:
 *  o res_vnode: the vnode of the parent directory of "name"
 *  o name: the `basename' (the element of the pathname)
 *  o namelen: the length of the basename
 *
 * For example: dir_namev("/s5fs/bin/ls", &namelen, &name, NULL,
 * &res_vnode) would put 2 in namelen, "ls" in name, and a pointer to the
 * vnode corresponding to "/s5fs/bin" in res_vnode.
 *
 * The "base" argument defines where we start resolving the path from:
 * A base value of NULL means to use the process's current working directory,
 * curproc->p_cwd.  If pathname[0] == '/', ignore base and start with
 * vfs_root_vn.  dir_namev() should call lookup() to take care of resolving each
 * piece of the pathname.
 *
 * Note: A successful call to this causes vnode refcount on *res_vnode to
 * be incremented.
 */
int
dir_namev(const char *pathname, size_t *namelen, const char **name,
          vnode_t *base, vnode_t **res_vnode)
{
	/*NOT_YET_IMPLEMENTED("VFS: dir_namev");*/
	KASSERT(NULL != pathname);
	dbg(DBG_PRINT, "(GRADING2A 2.b)Input parameter pathname not null\n ");
	KASSERT(NULL != namelen);
	dbg(DBG_PRINT, "(GRADING2A 2.b)namelen pointer not null\n ");
	KASSERT(NULL != name);
	dbg(DBG_PRINT, "(GRADING2A 2.b)name pointer not null\n ");
	KASSERT(NULL != res_vnode);
	dbg(DBG_PRINT, "(GRADING2A 2.b)resultant pointer not null\n ");


	int ret = 0;
	vnode_t *result;
	vnode_t* dir;
	char* startPtr = (char*) pathname;
	char* breakPtr = (char*) pathname;
	char* endPtr = (char*) pathname + strlen(pathname);
	char* extraPtr = NULL;

	/*check if length check needed if not you can remove it
	if(strlen(pathname) == 0) {
		return -EINVAL;
	} else if(strlen(pathname) > MAXPATHLEN) {
		return -ENAMETOOLONG;
	} */

	if (pathname[0] == '/') {
		/*curproc->p_cwd = vfs_root_vn;*/
		dir = vfs_root_vn;

		startPtr++;
		/*breakPtr++;*/
	} else if (base == NULL) {
		dir = curproc->p_cwd;
	} else {
		dir = base;
	}

	if(dir != NULL) {
		vget(dir->vn_fs,dir->vn_vno);
	}
	while (breakPtr != endPtr) {
                breakPtr = strchr(startPtr, '/');
                extraPtr = breakPtr;
                if( extraPtr != NULL) {
                        while(*extraPtr == '/') {
                                extraPtr++;
                        }
                }
                if(breakPtr != NULL && breakPtr+1 != endPtr && extraPtr!=endPtr ) {
                        if(dir == NULL) {
                                vput(dir);
                                return -ENOENT;
                        } else if ( !S_ISDIR(dir->vn_mode) ) {
                                vput(dir);
                                return -ENOTDIR;
                        }
			KASSERT(NULL != dir);
			dbg(DBG_PRINT,"(GRADING2A 2.b)Directory vnode is not NULL\n");
                        int ret = lookup(dir, startPtr, breakPtr-startPtr, &result);

                        vput(dir);
                        if(ret < 0) {

                                return ret;
                        }
                        if ( !S_ISDIR(result->vn_mode) )
                        {
                                return -ENOTDIR;
                        }
                        dir = result;
                        if( extraPtr == NULL) {
                                breakPtr++;
                        } else {
                                breakPtr = extraPtr;
                        }
			startPtr = breakPtr;
                } else {
                        if(breakPtr+1 == endPtr) {
                                endPtr--;
                        }
                        break;
                }
        }
        *res_vnode = dir;
        *name = (const char*) startPtr;
        if( endPtr == extraPtr ) {
                *namelen = breakPtr-startPtr;
        } else {
                *namelen = endPtr-startPtr;
        }
        return 0;
}

/* This returns in res_vnode the vnode requested by the other parameters.
 * It makes use of dir_namev and lookup to find the specified vnode (if it
 * exists).  flag is right out of the parameters to open(2); see
 * <weenix/fnctl.h>.  If the O_CREAT flag is specified, and the file does
 * not exist call create() in the parent directory vnode.
 *
 * Note: Increments vnode refcount on *res_vnode.
 */
int
open_namev(const char *pathname, int flag, vnode_t **res_vnode, vnode_t *base)
{
	const char *name = NULL;
	size_t len;

	int errno= dir_namev(pathname,&len,&name,base,res_vnode);
	if ( errno >= 0) {
		vput(*res_vnode);
		if (len > NAME_LEN) {
			return - ENAMETOOLONG;
		}
		errno = lookup(*res_vnode, name, len, res_vnode);
		if ( errno == -ENOENT && (flag & O_CREAT) == O_CREAT) {
			KASSERT(NULL != (*res_vnode)->vn_ops->create);
			dbg(DBG_PRINT,
					"(GRADING2A 2.c)create operation on vnode is present\n ");
			errno = (*res_vnode)->vn_ops->create(*res_vnode, name, len,
					res_vnode);

		}
	}
	return errno;
}

#ifdef __GETCWD__
/* Finds the name of 'entry' in the directory 'dir'. The name is writen
 * to the given buffer. On success 0 is returned. If 'dir' does not
 * contain 'entry' then -ENOENT is returned. If the given buffer cannot
 * hold the result then it is filled with as many characters as possible
 * and a null terminator, -ERANGE is returned.
 *
 * Files can be uniquely identified within a file system by their
 * inode numbers. */
int
lookup_name(vnode_t *dir, vnode_t *entry, char *buf, size_t size)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_name");
        return -ENOENT;
}


/* Used to find the absolute path of the directory 'dir'. Since
 * directories cannot have more than one link there is always
 * a unique solution. The path is writen to the given buffer.
 * On success 0 is returned. On error this function returns a
 * negative error code. See the man page for getcwd(3) for
 * possible errors. Even if an error code is returned the buffer
 * will be filled with a valid string which has some partial
 * information about the wanted path. */
ssize_t
lookup_dirpath(vnode_t *dir, char *buf, size_t osize)
{
        NOT_YET_IMPLEMENTED("GETCWD: lookup_dirpath");

        return -ENOENT;
}
#endif /* __GETCWD__ */
