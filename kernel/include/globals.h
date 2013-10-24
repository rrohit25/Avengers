#pragma once

#include "proc/kthread.h"
#include "proc/proc.h"

extern kthread_t *curthr;
extern proc_t *curproc;

extern void *testproc(int arg1, void *arg2);
extern void *sunghan_deadlock_test(int arg1, void *arg2);
extern void *sunghan_test(int arg1, void *arg2);
