Kernel 3
--------

Project Team:
-------------
Aakarsh Medleri Hiremath (medlerih@usc.edu)
Ajith Guthi (guthi@usc.edu)
Gaurav Murti (gauravve@usc.edu)
Rohit Raravi (raravi@usc.edu)

Running Weenix:
--------------
./weenix -n

Implementation:
--------------

41 out of the 46 functions in Kernel 3 have been implemented. The following 5 functions have NOT been implemented:

kernel/vm/brk.c:              NOT_YET_IMPLEMENTED("VM: do_brk");
kernel/drivers/memdevs.c:     NOT_YET_IMPLEMENTED("VM: zero_mmap");
kernel/proc/fork.c:           NOT_YET_IMPLEMENTED("VM: do_fork");
kernel/proc/kthread.c:        NOT_YET_IMPLEMENTED("VM: kthread_clone");
kernel/api/syscall.c:         NOT_YET_IMPLEMENTED("VM: sys_getdents");


Relevant KASSERT's and dbg statements have been added to all of the implemented 41 methods. Since we could not get the user shell running we have added the tests hello, args, uname, segfault, vfstest, usershell to the kshell.

hello world! is being displayed on invoking hello. We have exited the kshell and displayed the "hello world!"  message on the output screen as a result of which you will see the "Bye!".  



