/******************************************************************************/
/* Important Spring 2023 CSCI 402 usage information:                          */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/*         53616c7465645f5fd1e93dbf35cbffa3aef28f8c01d8cf2ffc51ef62b26a       */
/*         f9bda5a68e5ed8c972b17bab0f42e24b19daa7bd408305b1f7bd6c7208c1       */
/*         0e36230e913039b3046dd5fd0ba706a624d33dbaa4d6aab02c82fe09f561       */
/*         01b0fd977b0051f0b0ce0c69f7db857b1b5e007be2db6d42894bf93de848       */
/*         806d9152bd5715e9                                                   */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/* If this comment block is removed or altered in a submitted file, 20 points */
/*         will be deducted.                                                  */
/******************************************************************************/

/*
 *  FILE: vfs_syscall.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Wed Apr  8 02:46:19 1998
 *  $Id: vfs_syscall.c,v 1.2 2018/05/27 03:57:26 cvsps Exp $
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

/*
 * Syscalls for vfs. Refer to comments or man pages for implementation.
 * Do note that you don't need to set errno, you should just return the
 * negative error code.
 */

/* To read a file:
 *      o fget(fd)
 *      o call its virtual read vn_op
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
    // NOT_YET_IMPLEMENTED("VFS: do_read");
    file_t* file_ptr = NULL;        // file pointer
    size_t  bytes_read = 0;         // number of bytes
    
    // get file pointer while checking valid file descriptor
    file_ptr = fget(fd);
    if (NULL == file_ptr) {
        return -EBADF;              // fd is not a valid file descriptor
    }
    
    // check if the file is open for read
    if (0 == (file_ptr->f_mode & (int)FMODE_READ)) {
        fput(file_ptr);             // de-ref file_ptr
        return -EBADF;              // fd is not open for read
    }
    
    // check if the file is actually a directory
    if (NULL == file_ptr->f_vnode->vn_ops->read) {
        fput(file_ptr);             // de-ref file ptr
        return -EISDIR;
    }
    
    // only call read for positive nbytes
    if (nbytes > 0) {
        bytes_read = file_ptr->f_vnode->vn_ops->read(file_ptr->f_vnode,     // file vnode
                                                     file_ptr->f_pos,       // file offset
                                                     buf,                   // buffer
                                                     nbytes);               // number of bytes to read
        
        if (bytes_read == nbytes) {
            do_lseek(fd, bytes_read, SEEK_CUR);                             // not reaching end-of-file yet, adding offset with bytes_read
        } else {                                                            // bytes_read < nbytes; end-of-file reached
            do_lseek(fd, 0, SEEK_END);                                      // file offset stops at the end-of-file
        }
    }
    
    fput(file_ptr);                                                         // release file_ptr reference
    return bytes_read;                                                      // return number of bytes read
}

/* Very similar to do_read.  Check f_mode to be sure the file is writable.  If
 * f_mode & FMODE_APPEND, do_lseek() to the end of the file, call the write
 * vn_op, and fput the file.  As always, be mindful of refcount leaks.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for writing.
 */
int
do_write(int fd, const void *buf, size_t nbytes)
{
    // NOT_YET_IMPLEMENTED("VFS: do_write");
    file_t* file_ptr = NULL;            // file pointer
    size_t  bytes_write = 0;            // number of bytes to write
    
    // get file pointer while checking valid file descriptor
    file_ptr = fget(fd);
    if (NULL == file_ptr) {
        return -EBADF;                  // fd is not a valid file descriptor
    }
    
    // check if the file is open for write or append
    if ((0 == (file_ptr->f_mode & (int)FMODE_WRITE)) && (0 == (file_ptr->f_mode & (int)FMODE_APPEND))) {
        fput(file_ptr);                 // de-ref file_ptr
        return -EBADF;                  // fd is not open for read
    }
    
    // check if the file is actually a directory
    //if (NULL == file_ptr->f_vnode->vn_ops->write) {
    //    fput(file_ptr);                 // de-ref file ptr
    //    return -EISDIR;
    //}
    
    // write data if nbytes > 0
    if (nbytes > 0) {
        if (0 != (file_ptr->f_mode & (int)FMODE_APPEND)) {
            // move offset to the end of file for appending mode
            do_lseek(fd, 0, SEEK_END);                                        // file offset moves to the end-of-file for further appending data
        }
        bytes_write = file_ptr->f_vnode->vn_ops->write(file_ptr->f_vnode,     // file vnode
                                                       file_ptr->f_pos,       // file offset
                                                       buf,                   // buffer for write
                                                       nbytes);               // nbytes for write  
        if (bytes_write == nbytes) {
        KASSERT((S_ISCHR(file_ptr->f_vnode->vn_mode)) ||
                (S_ISBLK(file_ptr->f_vnode->vn_mode)) ||
                 ((S_ISREG(file_ptr->f_vnode->vn_mode)) && (file_ptr->f_pos <= file_ptr->f_vnode->vn_len)));
        }                                               
        do_lseek(fd, bytes_write, SEEK_CUR);                                  // move file offset to current + bytes_write (works for both write and append)
    }
    
    // de-ref file vnode
    fput(file_ptr);
    
    // return number of bytes written
    return bytes_write;
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
    // NOT_YET_IMPLEMENTED("VFS: do_close");
    file_t* file_ptr = NULL;
    
    // check fd validity
    if (fd < 0 || fd >= NFILES) {
        // invalid file descriptor
        return -EBADF;
    }
    
    // get a copy of the file pointer
    file_ptr = curproc->p_files[fd];
    if (NULL == file_ptr) {
        return -EBADF;
    }
    
    // zero the p_files reference
    curproc->p_files[fd] = NULL;
    
    // de-ref the file
    fput(file_ptr);
    
    // successfully return
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
    // NOT_YET_IMPLEMENTED("VFS: do_dup");
    file_t* file_ptr = NULL;                    // file pointer
    int new_fd;                                 // new file descriptor
    
    if(fd < 0 || fd >= NFILES) 
    {
        return -EBADF;
    }

    file_ptr = fget(fd);                        // get file ptr and up fd's refcount
    if (NULL == file_ptr) {
        return -EBADF;
    }
    
    new_fd = get_empty_fd(curproc);             // get an empty fd from current process
//    if (new_fd < 0) {
//        fput(file_ptr);                         // something goes wrong. The process runs out of file descriptor. fput the fd

//    } else {
    curproc->p_files[new_fd] = file_ptr;    // point the new fd to the same file_ptr as the given fd

//    }
    
    return new_fd;                              // return new file descriptor or error code per circumstances
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
    // NOT_YET_IMPLEMENTED("VFS: do_dup2");
    file_t* file_ptr = NULL;
    
    if(ofd < 0 || ofd >= NFILES) 
    {
        return -EBADF;
    }

    file_ptr = fget(ofd);                           // get file ptr and up fd's refcount
    if (NULL == file_ptr) {
        return -EBADF;
    }
    
//    if (nfd < 0 || nfd >= NFILES) {
//        fput(file_ptr);                             // de-ref file_ptr due to something wrong

//        return -EBADF;                              // nfd is out of range
//    }
    
    if (NULL != curproc->p_files[nfd]) {            // close the file nfd is pointing to
        if (nfd == ofd) {
            fput(file_ptr);                         // de-ref the file_ptr as nfd is the same as ofd (so not duplicating at all)
//        } else {
//            do_close(nfd);                          // close the file pointed by nfd first
        }
    }
    
    curproc->p_files[nfd] = curproc->p_files[ofd];  // dup nfd as ofd
    
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
    // NOT_YET_IMPLEMENTED("VFS: do_mknod");
    const char* mk_name = NULL;                 // name of the folder for mkdir
    size_t      mk_namelen = 0;                 // length of the folder name
    vnode_t*    parent_vnode = NULL;            // parent node for the folder
    vnode_t*    mk_vnode = NULL;                // folder node for mkdir
    int         ret = 0;                        // return value for each call
    
//    if (S_IFCHR != mode && S_IFBLK != mode) {   // not creating special files

//        return -EINVAL;
//    }
    
    // get the parent node and the target file name
    ret = dir_namev(path, &mk_namelen, &mk_name, NULL, &parent_vnode);
//    if (0 != ret) {

//        return ret;
//    }
    
    // look up the specifial file in folder
    ret = lookup(parent_vnode, mk_name, mk_namelen, &mk_vnode);
//    if (-ENOTDIR == ret || NULL == parent_vnode->vn_ops->mknod) {       // not a directory
//        vput(parent_vnode);

//        return ret;
//    } else if (0 == ret) {
//        vput(parent_vnode);                                             // file exist already

//        return -EEXIST;
//    }
    
    // call vn_ops mknod method
    KASSERT(NULL != parent_vnode->vn_ops->mknod);
    ret = parent_vnode->vn_ops->mknod(parent_vnode, mk_name, mk_namelen, mode, devid);
    vput(parent_vnode);
    
    return ret;
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
        // NOT_YET_IMPLEMENTED("VFS: do_mkdir");
        // dir_namev()
        const char* mk_name = NULL;                 // name of the folder for mkdir
        size_t      mk_namelen = 0;                 // length of the folder name
        vnode_t*    parent_vnode = NULL;            // parent node for the folder
        vnode_t*    mk_vnode = NULL;                // folder node for mkdir
        int         ret = 0;                        // return value for each call
        
        /**
         * 
         * Extract the folder name and name length for mkdir
         * Find the parent node for the folder for mkdir 
         */
        ret = dir_namev(path, &mk_namelen, &mk_name, NULL, &parent_vnode);
        if (0 != ret) {
            // possible return values:
            // ENOENT: A directory component in path does not exist.
            // ENAMETOOLONG: A component of path was too long. (TODO: Do we really need this?)
            return ret;
        }
        
        /**
         * 
         * Look up the folder in parent folder to see if the folder already exist
         * If the folder already exist, return -EEXIST
         * Also need to decrease reference count for parent_vnode and mk_vnode
         * before turn the error code 
         */
        ret = lookup(parent_vnode, mk_name, mk_namelen, &mk_vnode);
        if (0 == ret) {
            // de-ref vnodes
            vput(parent_vnode);         // de-ref parent vnode
            vput(mk_vnode);             // de-ref looked up mk_vnode (already exists)
            
            // return error code
            return -EEXIST;
        }
        
        /**
         * 
         * The parent is actually not a directory
         */
        /* 
        if (NULL == parent_vnode->vn_ops->mkdir) {
            // de-ref parent vnode
            vput(parent_vnode);         // de-ref parent vnode
            
            // return error code

            return -ENOTDIR;
        }
        */
        // make dir with vnode ops mkdir 
        KASSERT(NULL != parent_vnode->vn_ops->mkdir);
        ret = parent_vnode->vn_ops->mkdir(parent_vnode, mk_name, mk_namelen);
        vput(parent_vnode);
        
        // return mkdir return value
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
        //NOT_YET_IMPLEMENTED("VFS: do_rmdir");
        //return -1;

        const char *name;
        vnode_t *n;
	size_t namelen;
	int error, len, return_value;
	len=strlen(path);
	error = dir_namev(path,&namelen,&name,NULL,&n);
        if(error!=0)
	{
                return error;
	}

        if (path[len-1] == '.') {
        	if ((len > 1 && path[len-2] == '/') || len == 1) {	
                        vput(n);
                        return -EINVAL;
                } 
                else if (len> 1 && path[len-2] == '.') {
                        vput(n);
                        return -ENOTEMPTY;           
                }
        }
	 
//	if(n -> vn_ops -> rmdir == NULL) {
//            vput(n);

//            return -ENOTDIR;
//        }
	 
        
        KASSERT(NULL != n->vn_ops->rmdir);
        
	return_value = n->vn_ops->rmdir(n,  name, namelen);
        vput(n);
	return return_value;
    
}

/*
 * Similar to do_rmdir, but for files.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EPERM
 *        path refers to a directory.
 *      o ENOENT
 *        Any component in path does not exist, including the element at the
 *        very end.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_unlink(const char *path)
{
        //NOT_YET_IMPLEMENTED("VFS: do_unlink");
        //return -1;

        size_t namelen;
        const char *name;
        vnode_t *n;
        vnode_t *r;
        int error,errorlookup,return_value;
        error = dir_namev(path, &namelen, &name, NULL, &n);
        if (error!= 0) 
        { 
                return error;
        }
        
	errorlookup= lookup(n,name, namelen, &r);
        if (errorlookup != 0)
        {
                vput(n);
                return errorlookup;
        }
        if (S_ISDIR(r->vn_mode))
        {
                vput(n);
                vput(r);
                return -EPERM;
        }
        
        KASSERT(NULL != n->vn_ops->unlink);
        
        return_value = n->vn_ops->unlink(n, name, namelen);
        vput(r);
        vput(n);
        return return_value;
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
 *      o EPERM
 *        from is a directory.
 */
int
do_link(const char *from, const char *to)
{
        //NOT_YET_IMPLEMENTED("VFS: do_link");
        //return -1;

        size_t namelen;
        const char *name;
        int error,errordir,errorlookup,return_value;
        vnode_t *fromn;
        vnode_t *tod;
        vnode_t *ton;
        error = open_namev(from, 0, &fromn, NULL);  
        if (error!= 0)
        {
                return error;
        }
        else{
         errordir= dir_namev(to, &namelen, &name, NULL, &tod);
                vput(fromn);
                return errordir;
        
        }
       
        //if (S_ISDIR(fromn->vn_mode)) {
          //  vput(fromn); 

          //  return -EPERM;
        //}
        
//        errorlookup= lookup(tod, name, namelen, &ton);
//        if (errorlookup == 0)
//        {
//                vput(fromn);
//                vput(tod);
//                vput(ton);

//                return -EEXIST;
//        }
//
//        if (tod->vn_ops->link == NULL)
//        {
//                vput(fromn);
//                vput(tod);

//                return -ENOTDIR;
//        }

       // return_value = tod->vn_ops->link(fromn, tod, name, namelen);
      //  vput(fromn);
      //  vput(tod);

      //  return return_value;
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
        //NOT_YET_IMPLEMENTED("VFS: do_rename");
        //return -1;

        int error;
        error = do_link(oldname, newname);
        
                return error;
        

       // return do_unlink(oldname);
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
        //NOT_YET_IMPLEMENTED("VFS: do_chdir");
        //return -1;
        vnode_t *old = curproc -> p_cwd; 
        vnode_t *new;
        int error;
	error= open_namev(path, 0, &new, NULL);      
        if (error!= 0) {
                return error;
        }  
        if(!S_ISDIR(new ->vn_mode))
        {
                vput(new);
                return -ENOTDIR;
        }
        vput(old);
        curproc -> p_cwd = new;
        return 0;
}

/* Call the readdir vn_op on the given fd, filling in the given dirent_t*.
 * If the readdir vn_op is successful, it will return a positive value which
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
        //NOT_YET_IMPLEMENTED("VFS: do_getdent");
        //return -1;
         if(fd < 0)
        {
                return -EBADF;
        }
        if(fd >= NFILES)
        {
                return -EBADF;
        }
        file_t *f;
        f = fget(fd);
        if(f == NULL)
        {
                return -EBADF;
        }
        if(!S_ISDIR(f->f_vnode->vn_mode) || f->f_vnode->vn_ops->readdir == NULL)
        {     
                fput(f);
		return -ENOTDIR;
        }

        int c = f -> f_vnode->vn_ops->readdir(f->f_vnode, f->f_pos, dirp);
        
        f->f_pos = f->f_pos + c;
	fput(f);
        if(c == 0)
        {
                return 0;
        } 
        return sizeof(*dirp);
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
    // NOT_YET_IMPLEMENTED("VFS: do_lseek");
    file_t* file_ptr = NULL;
    
    // check valid file descriptor
    if (fd < 0 || fd >= NFILES) {
        return -EBADF;
    }
    
    // check valid whence
    if ((SEEK_SET != whence) && (SEEK_CUR != whence) && (SEEK_END != whence)) {
        return -EINVAL;
    }
    
    // get file ptr
    file_ptr = fget(fd);
    if (NULL == file_ptr) {
        return -EBADF;
    }
    
    switch (whence) {
        case SEEK_SET:
            {
                if (offset < 0) {                                   // resulting f_pos can not be negative
                    fput(file_ptr);
                    return -EINVAL;
                }
                file_ptr->f_pos = offset;
            }
            break;
        case SEEK_CUR:
            {
                if ((file_ptr->f_pos + offset) < 0) {               // resulting f_pos can not be negative
                    fput(file_ptr);
                    return -EINVAL;
                }
                file_ptr->f_pos += offset;
            }
            break;
        case SEEK_END:
            {
                if ((file_ptr->f_vnode->vn_len + offset) < 0) {     // resulting f_pos can not be negative
                    fput(file_ptr);
                    return -EINVAL;
                }
                file_ptr->f_pos = (file_ptr->f_vnode->vn_len + offset);
            }
            break;
        default:
            break;
    };
    
    fput(file_ptr);
    
    return (file_ptr->f_pos);
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
 *      o EINVAL
 *        path is an empty string.
 */
int
do_stat(const char *path, struct stat *buf)
{
       // NOT_YET_IMPLEMENTED("VFS: do_stat");
       // return -1;
       if(strlen(path)==0){
                return -EINVAL;
        }
		vnode_t *n;
        int error;
		error = open_namev(path, 0, &n, NULL);
        if(error >= 0){
            KASSERT(NULL != n->vn_ops->stat);
            int return_value = n->vn_ops->stat(n, buf);
            
            vput(n);
            return return_value;
        }
        
        return error;
         
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
 * checking.
 */
int
do_umount(const char *target)
{
        NOT_YET_IMPLEMENTED("MOUNTING: do_umount");
        return -EINVAL;
}
#endif
