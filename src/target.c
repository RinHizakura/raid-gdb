#include "target.h"
#include <stdio.h>
#include <string.h>
#include <sys/personality.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <unistd.h>
#include "arch.h"

bool target_lauch(target_t *t, char *cmd)
{
    int wstatus;
    pid_t pid = fork();
    /* for child process */
    if (pid == 0) {
        // disable address space randomization
        personality(ADDR_NO_RANDOMIZE);
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        execl(cmd, cmd, NULL);
    }

    /* for parent process */
    if (waitpid(pid, &wstatus, __WALL) < 0) {
        perror("waitpid");
        return false;
    }

    t->pid = pid;
    t->hit_bp = NULL;
    /* we should guarantee the initial value of breakpoint array */
    memset(t->bp, 0, sizeof(bp_t) * MAX_BP);
    int options = PTRACE_O_EXITKILL;
    ptrace(PTRACE_SETOPTIONS, pid, NULL, options);
    printf("PID(%d)\n", t->pid);
    return true;
}

static bool target_handle_bp(target_t *t)
{
    if (!t->hit_bp) {
        return true;
    }

    size_t addr;
    bool ret = target_get_reg(t, REG_RIP, &addr);
    if (!ret)
        return ret;

    /* If the address isn't at the last breakpoint we hit, it means
     * the user may change pc during this period. We don't execute an extra step
     * of the original instruction in that case. */
    if (addr == t->hit_bp->addr) {
        ret = target_step(t);
        if (!ret)
            return ret;
    }

    /* restore the trap instruction before we do cont command */
    ret = bp_set(t->hit_bp);
    if (!ret)
        return ret;

    t->hit_bp = NULL;
    return true;
}

bool target_step(target_t *t)
{
    int wstatus;
    ptrace(PTRACE_SINGLESTEP, t->pid, NULL, NULL);
    if (waitpid(t->pid, &wstatus, __WALL) < 0) {
        perror("waitpid");
        return false;
    }
    return true;
}

bool target_conti(target_t *t)
{
    bool ret;
    ret = target_handle_bp(t);
    if (!ret)
        return ret;

    int wstatus;
    ptrace(PTRACE_CONT, t->pid, NULL, NULL);

    if (waitpid(t->pid, &wstatus, __WALL) < 0) {
        perror("waitpid");
        return false;
    }

    /* When a breakpoint is hit previously, to keep executing instead of hanging
     * on the trap instruction latter, we first rollback pc to the previous
     * instruction and restore the original instruction temporarily. */
    size_t addr;
    ret = target_get_reg(t, REG_RIP, &addr);
    if (!ret)
        return ret;

    /* FIXME: we should match all of the possible registered breakpoint instead
     * of only checking bp0 */
    if ((addr - 1) == t->bp[0].addr) {
        t->hit_bp = &t->bp[0];
        ret = bp_unset(&t->bp[0]);
        if (!ret)
            return ret;

        ret = target_set_reg(t, REG_RIP, addr - 1);
        if (!ret)
            return ret;
    }

    return true;
}

bool target_set_breakpoint(target_t *t, size_t addr)
{
    /* FIXME: We have to enable more break point and also be
     * awared to set two breakpoint on the same address */
    bp_init(&t->bp[0], t->pid, addr);
    return bp_set(&t->bp[0]);
}

bool target_set_reg(target_t *t, size_t idx, size_t value)
{
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, t->pid, NULL, &regs);

    *(((size_t *) &regs) + idx) = value;

    ptrace(PTRACE_SETREGS, t->pid, NULL, &regs);
    return true;
}

bool target_get_reg(target_t *t, size_t idx, size_t *value)
{
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, t->pid, NULL, &regs);

    *value = *(((size_t *) &regs) + idx);
    return true;
}

bool target_get_reg_by_name(target_t *t, char *name, size_t *value)
{
    struct user_regs_struct regs;
    ptrace(PTRACE_GETREGS, t->pid, NULL, &regs);

    /* TODO: choose the corresponding register by name */
    bool found = false;
    int idx = 0;
    for (; idx < REGS_CNT; idx++) {
        if (strcmp(name, reg_desc_array[idx].name) == 0) {
            found = true;
            break;
        }
    }

    if (!found)
        return false;

    *value = *(((size_t *) &regs) + idx);
    return true;
}
