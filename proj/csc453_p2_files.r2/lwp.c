#include "lwp.h"
#include "fp.h"
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
static tid_t next_thread = 1;
static thread cur_thread = NULL;
static scheduler cur_sched = NULL;
static thread threads = NULL;
static thread original_thread = NULL;
static thread round_robin_head = NULL;
static thread round_robin_end = NULL;
#define tnext sched_one
#define tprev sched_two
static void round_robin_init(void) {
    round_robin_head = NULL;
    round_robin_end = NULL;
}
static void round_robin_shutdown(void) {
    round_robin_head = NULL;
    round_robin_end = NULL;
}
static void round_robin_admit(thread new_thread) {
    if (!new_thread) {
        return;
    }
    if (!round_robin_head) {
        round_robin_head = new_thread;
        round_robin_end = new_thread;
        new_thread->tnext = new_thread;
        new_thread->tprev = new_thread;
    } else {
        new_thread->tnext = round_robin_head;
        new_thread->tprev = round_robin_end;
        round_robin_end->tnext = new_thread;
        round_robin_head->tprev = new_thread;
        round_robin_end = new_thread;
    }
}
static void lwp_wrap(lwpfun fun, void *arg) {
    int r;
    r = fun(arg);
    lwp_exit(r);
}
static void round_robin_remove(thread thrd) {
    if (!thrd) {
        return;
    }
    if (!thrd->tnext) {
        return;
    }
    if (thrd->tnext == thrd) {
        round_robin_head = NULL;
        round_robin_end = NULL;
    } 
    else {
        thrd->tprev->tnext = thrd->tnext;
        thrd->tnext->tprev = thrd->tprev;
        if (round_robin_head == thrd) {
            round_robin_head = thrd->tnext;
        }
        if (round_robin_end == thrd) {
            round_robin_end = thrd->tprev;
        }
    }
    thrd->tnext = NULL;
    thrd->tprev = NULL;
}
static thread round_robin_next(void) {
    thread next;
    if (!round_robin_head) {
        return NULL;
    }
    next = round_robin_head;
    if (round_robin_head->tnext != round_robin_head) {
        round_robin_head = round_robin_head->tnext;
        round_robin_end = next;
    }
    return next;
}
static struct scheduler round_robin_start = {
    round_robin_init,
    round_robin_shutdown,
    round_robin_admit,
    round_robin_remove,
    round_robin_next
};
scheduler RoundRobin = &round_robin_start;
static void add_thread(thread t) {
    t->lib_one = threads;
    threads = t;
}
static void remove_thread(thread t) {
    thread prev = NULL;
    thread cur = threads;
    while (cur) {
        if (cur == t) {
            if (prev) {
                prev->lib_one = cur->lib_one;
            } else {
                threads = cur->lib_one;
            }
            cur->lib_one = NULL;
            return;
        }
        prev = cur;
        cur = cur->lib_one;
    }
}

tid_t lwp_create(lwpfun function, void *argument, size_t stacksize) {
    thread nt;
    unsigned long *sp;
    size_t stack_increment;
    struct rlimit limit;
    long page_size;
    nt = malloc(sizeof(context));
    if (!nt) {
        return NO_THREAD;
    }
    nt->tid = next_thread++;
    page_size = sysconf(_SC_PAGE_SIZE);
    if (getrlimit(RLIMIT_STACK, &limit) == 0) {
        if (limit.rlim_cur != RLIM_INFINITY) {
            stack_increment = limit.rlim_cur;
        }
    }
    else {
        stack_increment = 8 * 1024 * 1024;
    }
    if (stack_increment % page_size) {
        stack_increment = ((stack_increment / page_size) + 1) * page_size;
    }
    nt->stack = mmap(NULL, stack_increment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (nt->stack == MAP_FAILED) {
        free(nt);
        return NO_THREAD;
    }
    nt->stacksize = stack_increment;
    nt->status = LWP_LIVE;
    nt->state.fxsave = FPU_INIT;
    sp = (unsigned long *)((char*) nt->stack + stack_increment);
    sp--;
    *sp = (unsigned long)lwp_wrap;
    sp--;
    *sp = 0;
    while (((unsigned long)sp % 16) != 0) {
        sp--;
        *sp = 0;
    }
    nt->state.rdi = (unsigned long) function;
    nt->state.rsi = (unsigned long) argument;
    nt->state.rbp = (unsigned long) sp;
    nt->state.rsp = (unsigned long) (sp + 1);
    nt->state.rax = 0;
    nt->state.rbx = 0;
    nt->state.rcx = 0;
    nt->state.rdx = 0;
    nt->state.r8 = 0;
    nt->state.r9 = 0;  
    nt->state.r10 = 0;
    nt->state.r11 = 0;
    nt->state.r12 = 0;
    nt->state.r13 = 0;
    nt->state.r14 = 0;
    nt->state.r15 = 0;
    nt->lib_one = NULL;
    nt->lib_two = NULL;
    nt->sched_one = NULL;
    nt->sched_two = NULL;
    add_thread(nt);
    if (!cur_sched) {
        cur_sched = RoundRobin;
        if (cur_sched->init) {
            cur_sched->init();
        }
    }
    cur_sched->admit(nt);
    return nt->tid;
}
void lwp_start(void) {
    thread t;
    if (!cur_sched) {
        cur_sched = RoundRobin;
        if (cur_sched->init) {
            cur_sched->init();
        }
    }
    t = malloc(sizeof(context));
    if (!t) {
        fprintf(stderr, "lwp_start: failed\n");
        exit(1);
    }
    t->tid = next_thread++;
    t->stack = NULL;
    t->stacksize = 0;
    t->status = LWP_LIVE;
    t->state.fxsave = FPU_INIT;
    t->lib_one = NULL;
    t->lib_two = NULL;
    t->sched_one = NULL;
    t->sched_two = NULL;
    original_thread = t;
    add_thread(t);
    cur_sched->admit(t);
    cur_thread = t;
    lwp_yield();
}
void lwp_yield(void) {
    thread next;
    thread prev;
    if (!cur_sched) {
        return;
    }
    next = cur_sched->next();
    if (!next) {
        int status = 0;
        if (cur_thread) {
            status = LWPTERMSTAT(cur_thread->status);
        }
        exit(status);
    }
    prev = cur_thread;
    cur_thread = next;
    swap_rfiles(&(prev->state), &(next->state));
}
void lwp_exit(int status) {
    if (!cur_thread) {
        exit(status & 0xFF);
    }
    cur_thread->status = MKTERMSTAT(LWP_TERM, status & 0xFF);
    if (cur_sched) {
        cur_sched->remove(cur_thread);
    }
    lwp_yield();
    exit(status & 0xFF);
}
tid_t lwp_wait(int *status) {
    thread t;
    thread stop = NULL;
    tid_t ret;
    int run;
    while (1) {
        t = threads;
        while(t) {
            if (LWPTERMINATED(t->status)) {
                stop = t;
                break;
            }
            t = t->lib_one;
        }

        if (stop) {
            break;
        }
        run = 0;
        t = threads;
        while (t) {
            if (t!= cur_thread) {
                if (t->status == LWP_LIVE) {
                    run = 1;
                    break;
                }
            }
            t = t->lib_one;
        }
        if (!run) {
            return NO_THREAD;
        }
        lwp_yield();
    }
    if (status) {
        *status = stop->status;
    }
    ret = stop->tid;
    remove_thread(stop);
    if (stop->stack) {
        if (stop != original_thread) {
            munmap(stop->stack, stop->stacksize);
        }
    }
    free(stop);
    return ret;
    
}
tid_t lwp_gettid(void) {
    if (!cur_thread) {
        return NO_THREAD;
    }
    return cur_thread->tid;
}
thread tid2thread(tid_t tid) {
    thread t = threads;
    while (t) {
        if (t->tid == tid) {
            return t;
        }
        t = t->lib_one;
    }
    return NULL;
}
void lwp_set_scheduler(scheduler new_sched) {
    thread t;
    scheduler old_sched;
    if (!new_sched) {
        new_sched = RoundRobin;
    }
    if (new_sched == cur_sched) {
        return;
    }
    old_sched = cur_sched;
    if (new_sched->init) {
        new_sched->init();
    }
    if (old_sched) {
        while ((t = old_sched->next())) {
            old_sched->remove(t);
            new_sched->admit(t);
        }
        if (old_sched ->shutdown) {
            old_sched->shutdown();
        }
    }
    cur_sched = new_sched;
}
scheduler lwp_get_scheduler(void) {
    return cur_sched;
}
