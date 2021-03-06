#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
#if OPT_A2
        set_exitcode(exitcode);
#else
  (void)exitcode;
#endif /* OPT_A2 */

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
#if OPT_A2
        *retval = curproc->pid;
#else
  *retval = 1;
#endif /* OPT_A2 */
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */
#if OPT_A2
        // check if the options argument requested invalid or unsupported options
        if (options != 0) {
                return EINVAL;
        }
        
        // check if the status argument was an invalid pointer
        if (status == NULL) {
                return EFAULT;
        }
        
        // check if the pid argument named a nonexistent process
        if (!is_exists(pid)) {
                return ESRCH;

        // if the pid argument named a process that the current
        // process was not interested in or terminate with error,
        // raise error. Otherwise save exitcode in exitstatus
        } else if (!get_child_exitcode(pid, &exitstatus)) {
                exitstatus = _MKWAIT_STOP(0);
                copyout((void *)&exitstatus,status,sizeof(int));
                return ECHILD;
        }

        // encode the exitstatus. only if the process exit
        // correctly would get here
        exitstatus = _MKWAIT_EXIT(exitstatus);
#else
  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
#endif /* OPT_A2 */

  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#if OPT_A2
int
sys_fork(struct trapframe* tf, pid_t* retval) {
        if (!can_create_proc()) return ENPROC;

        // create a new process, make them to be parent - child relation
        struct proc* new_proc = proc_create_runprogram(curproc->p_name);
        if (new_proc == NULL) return ENOMEM;
        *retval = new_proc->pid;

        // create a new address space and copy it
        struct addrspace* as;
        int err = as_copy(curproc_getas(), &as);
        if (err) {
                proc_destroy(new_proc);
                return err;
        }

        // simply pass tf and as to new forked process,
        // the new process would activate the as and setup the tf
        new_proc->p_addrspace = as;
        err = thread_fork((const char *)curthread->t_name /* thread name */,
                              new_proc /* new process */,
                              (void *)enter_forked_process /* thread function */,
                              tf /* thread arg */, 0 /* thread arg */);
        if (err) {
                new_proc->p_addrspace = NULL;
                proc_destroy(new_proc);
                as_destroy(as);
                return err;
        }

        return 0;
}
#endif /* OPT_A2 */
