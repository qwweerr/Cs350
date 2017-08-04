/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#if OPT_A2
#include <limits.h>
#include <copyinout.h>
#endif /* OPT_A2 */

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
#if OPT_A2
runprogram(unsigned long nargs, char** args)
#else
runprogram(char *progname)
#endif /* OPT_A2 */
{
	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	/* Open the file. */
#if OPT_A2
        /* make sure the program's path is valid */
        if (strlen(args[0]) >= PATH_MAX) return ENAMETOOLONG;
        char progname[PATH_MAX];
        strcpy(progname, args[0]);
#endif /* OPT_A2 */
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new process. */
	KASSERT(curproc_getas() == NULL);

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(as, &stackptr);
	if (result) {
		/* p_addrspace will go away when curproc is destroyed */
		return result;
	}

#if OPT_A2
        char* argv[nargs + 1];
        argv[nargs] = NULL;
        size_t len;
        for (int i = nargs - 1; i >= 0; --i) {
                /* find the lenth of argument and align the lenth */
                len = strlen(args[i]) + 1;
                if (len % 4) len += 4 - len % 4;

                /* copy out the argument */
                stackptr -= len;
                argv[i] = (char *)stackptr;
                result = copyoutstr(args[i], (userptr_t)stackptr, len, NULL);
                if (result) {
                        /* p_addrspace will go away when curproc is destroyed */
                        return result;
                }
        }

        /* setup the char**, which is argv, on the user-space stack */
        /* make sure the stack is aligned */
        if (stackptr % 4) stackptr -= 4 - stackptr % 4;

        stackptr -= sizeof(char *) * (nargs + 1);
        result = copyout((const void *)argv,
                         (userptr_t)stackptr,
                         sizeof(char *) * (nargs + 1));
        if (result) return result;

        
        /* make sure the stack is aligned */
        userptr_t argvs = (userptr_t)stackptr;
        if (stackptr % 8) stackptr -= 8 - stackptr % 8;

        /* Warp to user mode. */
        enter_new_process(nargs /*argc*/, argvs /*userspace addr of argv*/,
                          stackptr, entrypoint);
#else
	/* Warp to user mode. */
	enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
			  stackptr, entrypoint);
#endif /* OPT_A2 */
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

#if OPT_A2
int
sys_execv(const char *progname, char **argvs)
{
        if (progname == NULL || argvs == NULL) {
                return EFAULT;
        }

        char* argvs_str;
        int argc = 0;
        size_t* argvs_len;
        size_t args_size = 0;
        size_t len;
        struct addrspace *as;
        struct vnode *v;
        vaddr_t entrypoint, stackptr;
        int result;

        /* copy the progname from user-space address into
           kernel-space address for temporary use of vfs_open */
        char progname_temp[PATH_MAX];
        result = copyinstr((const_userptr_t)progname,
                           progname_temp,
                           PATH_MAX,
                           &len);
        if (result) {
                return result;
        }

        /* Open the file. */
        result = vfs_open(progname_temp, O_RDONLY, 0, &v);
        if (result) {
                return result;
        }

        /* We should not be a new process. */
        KASSERT(curproc_getas() != NULL);

        /* count the number of arguments */
        while (argvs[argc] != NULL) ++argc;

        /* Prepare to copy the arguments into kernel heap*/
        argvs_str = (char *)kmalloc(sizeof(char) * ARG_MAX);
        if (argvs_str == NULL) {
                vfs_close(v);
                return ENOMEM;
        }

        argvs_len = (size_t *)kmalloc(sizeof(size_t) * argc);
        if (argvs_len == NULL) {
                kfree(argvs_str);
                vfs_close(v);
                return ENOMEM;
        }

        /* Start to copy arguments into kernel heap */
        for (int i = 0; i < argc; ++i) {
                result = copyinstr((const_userptr_t)argvs[i],
                                   argvs_str + args_size,
                                   ARG_MAX - args_size,
                                   &len);
                if (result) {
                        kfree(argvs_len);
                        kfree(argvs_str);
                        vfs_close(v);

                        /* check if E2BIG */
                        if (result == ENAMETOOLONG) return E2BIG;
                        return result;
                }

                /* shift bit */
                if (len % 4) len += 4 - len % 4;

                argvs_len[i] = len;
                args_size += len;
        }

        /* Create a new address space. */
        as = as_create();
        if (as == NULL) {
                kfree(argvs_len);
                kfree(argvs_str);
                vfs_close(v);
                return ENOMEM;
        }

        /* Destroy the old address space */
        as_deactivate();
        as_destroy(curproc_getas());

        /* Switch to new one and activate it. */
        curproc_setas(as);
        as_activate();

        /* Load the executable. */
        result = load_elf(v, &entrypoint);
        /* Done with the file now. */
        vfs_close(v);
        if (result) {
                kfree(argvs_len);
                kfree(argvs_str);
                /* p_addrspace will go away when curproc is destroyed */
                return result;
        }

        /* Define the user stack in the address space */
        result = as_define_stack(as, &stackptr);
        if (result) {
                kfree(argvs_len);
                kfree(argvs_str);
                /* p_addrspace will go away when curproc is destroyed */
                return result;
        }

        /* modify the user-space stack to pass arguments */
        stackptr -= args_size;
        result = copyout(argvs_str, (userptr_t)stackptr, args_size);
        /* Nolonger to use argvs_str */
        kfree(argvs_str);
        argvs_str = (char *)stackptr;
        if (result) {
                kfree(argvs_len);
                /* p_addrspace will go away when curproc is destroyed */
                return result;
        }

        char* temp[(argc + 1)];
        for (int i = 0; i < argc; ++i) {
                temp[i] = argvs_str;
                argvs_str += argvs_len[i];
        }
        temp[argc] = NULL;
        kfree(argvs_len);

        /* shift bit */
        if (stackptr % 4) stackptr -= 4 - stackptr % 4;

        stackptr -= sizeof(char *) * (argc + 1);
        result = copyout((const void *)temp,
                         (userptr_t)stackptr,
                         sizeof(char *) * (argc + 1));
        if (result) return result;

        userptr_t argv = (userptr_t)stackptr;
        /* shift the stackptr */
        if (stackptr % 8) stackptr -= 8 - stackptr % 8;

        /* Warp to user mode. */
        enter_new_process(argc /*argc*/, argv /*userspace addr of argv*/,
                          stackptr, entrypoint);
        
        /* enter_new_process does not return. */
        panic("enter_new_process returned\n");
        return EINVAL;
}
#endif /* OPT_A2 */
