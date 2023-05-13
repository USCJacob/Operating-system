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

/* the */
char rootBase = '/';
char endChar = '\0';

/* This takes a base 'dir', a 'name', its 'len', and a result vnode.
 * Most of the work should be done by the vnode's implementation
 * specific lookup() function.
 *
 * If dir has no lookup(), return -ENOTDIR.
 *
 * Note: returns with the vnode refcount on *result incremented.
 */
int lookup(vnode_t *dir, const char *name, size_t len, vnode_t **result)
{
        /*precondition*/
        // the dir must not be null
        KASSERT(NULL != dir);
        // the name must not be null
        KASSERT(NULL != name);
        // the result must not be null
        KASSERT(NULL != result);

//        if (NAME_LEN < len)
//        {
//                // self check

//                return -ENAMETOOLONG;
//        }

        if (0 == len)
        {
                *result = dir;
                vref(dir);
                // self check
                return 0;
        }

//        if (NULL == dir->vn_ops->lookup)
//        {
//                // self check

//                return -ENOTDIR;
//        }

        int rtval = dir->vn_ops->lookup(dir, name, len, result);

        if (rtval >= 0 && rootBase == name[len] && S_IFDIR != (*result)->vn_mode)
        {
                // self check
                vput(*result);
                return -ENOTDIR;
        }

        // self check
        return rtval;
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
int dir_namev(const char *pathname, size_t *namelen, const char **name,
              vnode_t *base, vnode_t **res_vnode)
{
        KASSERT(NULL != pathname);
        KASSERT(NULL != namelen);
        KASSERT(NULL != name);
        KASSERT(NULL != res_vnode);

//        if (endChar == pathname[0])
//        {
//                // self check

//                return -EINVAL;
//        }

        vnode_t *currentBase = NULL;

        unsigned int startIndex = 0;

        if (rootBase == pathname[0])
        {
                // self check
                currentBase = vfs_root_vn;
                *res_vnode = currentBase;
        }
        else if (NULL == base)
        {
                // self check
                currentBase = curproc->p_cwd;
                *res_vnode = currentBase;
        }

        vnode_t *currentNode = NULL;
        KASSERT(NULL != currentBase);

        // increase the ref coutn of the current base
        vref(currentBase);
        while (startIndex < strlen(pathname))
        {
                int curNameLen = 0;
                // get the next vnode's name
                while (pathname[startIndex + curNameLen] != '\0' && pathname[startIndex + curNameLen] != '/')
                {
                        curNameLen++;
                }
                // check the vnode's name's length
                if (curNameLen > NAME_LEN)
                {
                        vput(currentBase);
                        return -ENAMETOOLONG;
                }
                // if meet another '/'
                if (curNameLen == 0 && endChar != pathname[startIndex + 1])
                {                        
                        startIndex++;
                        continue;
                }
                // iterate to the next vnode
                if (pathname[startIndex + curNameLen] == '/' && pathname[startIndex + curNameLen + 1] != '\0')
                {
                        int rtval = lookup(currentBase, &pathname[startIndex], curNameLen, &currentNode);
                        if (rtval < 0)
                        {
                                // pathname resolution must start with a valid directory
                                // decrease the ref count of the current base
                                if (currentBase != NULL)
                                {
                                        vput(currentBase);
                                }

                                // self check
                                return rtval;
                        }

                        // decrease the ref count of the current base
                        if (currentBase != NULL)
                        {
                                vput(currentBase);
                        }
                        currentBase = currentNode;
                        *namelen = curNameLen;
                        *name = &pathname[startIndex];
                        startIndex += (curNameLen + 1);
                }
                else if (pathname[startIndex + curNameLen] == '\0' || pathname[startIndex + curNameLen + 1] == '\0')
                {
                        *name = &pathname[startIndex];
                        *namelen = curNameLen;
                        startIndex += (curNameLen + 1);
                }
        }

        if (NULL != currentNode)
        {
//                if (currentNode->vn_ops->lookup == NULL)
//                {
//                        vput(currentNode);
//                        // self check

//                        return -ENOTDIR;
//                }
                *res_vnode = currentNode;
        }
        else
        {
                *res_vnode = currentBase;
                // vref(*res_vnode);
        }
        return 0;
}

/* This returns in res_vnode the vnode requested by the other parameters.
 * It makes use of dir_namev and lookup to find the specified vnode (if it
 * exists).  flag is right out of the parameters to open(2); see
 * <weenix/fcntl.h>.  If the O_CREAT flag is specified and the file does
 * not exist, call create() in the parent directory vnode. However, if the
 * parent directory itself does not exist, this function should fail - in all
 * cases, no files or directories other than the one at the very end of the path
 * should be created.
 *
 * Note: Increments vnode refcount on *res_vnode.
 */
int open_namev(const char *pathname, int flag, vnode_t **res_vnode, vnode_t *base)
{
//        if (endChar == pathname[0])
//        {
//                // self check

//                return -EINVAL;
//        }

        size_t namelen = 0;
        const char *name = NULL;
        vnode_t *resDirVNode = NULL;

        int rtvalOfDir = dir_namev(pathname, &namelen, &name, base, &resDirVNode);
        // fail
        if (0 > rtvalOfDir)
        {
                // self check
                return rtvalOfDir;
        }

        // create the not exist file.
        int rtvalOfLookUpAndCreate = lookup(resDirVNode, name, namelen, res_vnode);
        if (0 > rtvalOfLookUpAndCreate)
        {
                if (O_CREAT == (flag & O_CREAT))
                {
                        KASSERT(NULL != resDirVNode->vn_ops->create);
                        rtvalOfLookUpAndCreate = resDirVNode->vn_ops->create(resDirVNode, name, namelen, res_vnode);
                        vput(resDirVNode);

                        // self check
                        return rtvalOfLookUpAndCreate;
                }
                vput(resDirVNode);

                // self check
                return rtvalOfLookUpAndCreate;
        }

        vput(resDirVNode);

        // self check
        return 0;
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
int lookup_name(vnode_t *dir, vnode_t *entry, char *buf, size_t size)
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
