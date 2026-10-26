/*
 * DeadOps-PS5: DeadOps' script override for the native PS5 build of Black Ops II
 * Zombies (PPSA34502), as an ELF-loader payload instead of an in-game sprx.
 *
 * Compiles /data/DeadOps/t6_zm/_clientids.gsc with DeadOps' T6 compiler, waits
 * for the zombies executable, then hooks DB_FindXAssetHeader from outside so
 * the game gets the compiled script in place of its own _clientids.gsc.
 * Based on DeadOps by ItsJokerZz (MIT).
 */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <machine/reg.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <ps5/kernel.h>

#include "gsc_compile.hpp"

#define TITLE_ID        "PPSA34502"
#define SCRIPT_PATH     "/data/DeadOps/t6_zm/_clientids.gsc"
#define SCRIPT_NAME     "maps/mp/gametypes_zm/_clientids.gsc"
#define LOG_PATH        "/data/DeadOps/log.txt"
#define LOCK_PATH       "/data/DeadOps/deadops-ps5.lock"

/* PPSA34502 codzm.elf. DB_FindXAssetHeader is the function called as
   (0x31, "..._clientids.gsc", 1, -1) by the script loader, which then checks
   the returned buffer at +0x10 for the T6 GSC magic. */
#define OFF_DETECT      0xA95144    /* "ui_multiplayer" */
#define OFF_DB_FIND     0x1C5940    /* DB_FindXAssetHeader */
#define PATCH_SIZE      20

#define GSC_MAGIC_T6    0x06000A0D43534780ULL
#define NID_GETPID      "HoLVWNanBBc"
#define AUTHID_DEBUGGER 0x4800000000010003ULL

#define WAIT_FOR_GAME_MS   (5 * 60 * 1000)
#define LINGER_AFTER_MS    (60 * 1000)
#define MAX_SEEN           64

static const uint8_t k_prologue[PATCH_SIZE] = {
    0x55, 0x48, 0x89, 0xe5, 0x41, 0x57, 0x41, 0x56, 0x41, 0x55,
    0x41, 0x54, 0x53, 0x48, 0x81, 0xec, 0xb8, 0x00, 0x00, 0x00,
};

extern const uint8_t deadops_hook_begin[];
extern const uint8_t deadops_hook_name_imm[];
extern const uint8_t deadops_hook_tree_imm[];
extern const uint8_t deadops_hook_ret_imm[];
extern const uint8_t deadops_hook_end[];

typedef struct {
    uint32_t app_id;
    uint64_t unknown1;
    char     title_id[16];
    char     unknown2[0x40];
} app_info_t;

int sceKernelGetAppInfo(pid_t pid, app_info_t *info);
int sceKernelSendNotificationRequest(int device, const void *req, size_t size, int blocking);

/* ---- logging ------------------------------------------------------------- */

static void logmsg(const char *fmt, ...)
{
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line) - 1, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n > (int)sizeof(line) - 2) n = sizeof(line) - 2;
    line[n++] = '\n';
    int fd = open(LOG_PATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd >= 0) {
        (void)write(fd, line, (size_t)n);
        close(fd);
    }
}

static void notify(const char *fmt, ...)
{
    struct {
        char unused[45];
        char message[3075];
    } req;
    memset(&req, 0, sizeof(req));
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    snprintf(req.message, sizeof(req.message), "[DeadOps] %s", msg);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
    logmsg("%s", msg);
}

static long now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

/* ---- process privileges ---------------------------------------------------- */

/* Full filesystem view and root ids for this payload (the loader may start it
   sandboxed), as the patched ploader does for itself. */
static void escalate_self(void)
{
    pid_t self = getpid();
    intptr_t root = kernel_get_root_vnode();
    if (root) {
        kernel_set_proc_rootdir(self, root);
        kernel_set_proc_jaildir(self, root);
    }
    kernel_set_ucred_uid(self, 0);
    kernel_set_ucred_ruid(self, 0);
    kernel_set_ucred_svuid(self, 0);
    kernel_set_ucred_rgid(self, 0);
    kernel_set_ucred_svgid(self, 0);
}

static int sys_ptrace(int req, pid_t pid, caddr_t addr, int data)
{
    pid_t self = getpid();
    uint64_t authid = kernel_get_ucred_authid(self);
    kernel_set_ucred_authid(self, AUTHID_DEBUGGER);
    int ret = (int)syscall(SYS_ptrace, req, pid, addr, data);
    int err = errno;
    kernel_set_ucred_authid(self, authid);
    errno = err;
    return ret;
}

static int wait_stopped(pid_t pid, int timeout_ms)
{
    for (int waited = 0; waited <= timeout_ms; waited += 10) {
        int status = 0;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid)
            return WIFSTOPPED(status) ? 0 : -1;
        if (r < 0 && errno != EINTR)
            return -1;
        usleep(10000);
    }
    return -1;
}

/* ---- remote syscalls ------------------------------------------------------- */

/* The kernel only accepts syscalls issued from libkernel, so the game runs the
   syscall instruction inside its own libkernel getpid wrapper (+0xA) and is
   single-stepped until that wrapper returns. */
static long remote_syscall(pid_t pid, long sysno, uint64_t a1, uint64_t a2, uint64_t a3,
                           uint64_t a4, uint64_t a5, uint64_t a6)
{
    intptr_t getpid_fn = kernel_dynlib_resolve(pid, 0x1, NID_GETPID);
    if (!getpid_fn)
        getpid_fn = kernel_dynlib_resolve(pid, 0x2001, NID_GETPID);
    if (!getpid_fn)
        return -1;

    struct reg saved, r;
    if (sys_ptrace(PT_GETREGS, pid, (caddr_t)&saved, 0))
        return -1;
    r = saved;
    r.r_rip = getpid_fn + 0xA;
    r.r_rax = sysno;
    r.r_rdi = a1;
    r.r_rsi = a2;
    r.r_rdx = a3;
    r.r_r10 = a4;
    r.r_r8 = a5;
    r.r_r9 = a6;
    if (sys_ptrace(PT_SETREGS, pid, (caddr_t)&r, 0))
        return -1;

    long result = -1;
    for (int i = 0; i < 10000; i++) {
        if (sys_ptrace(PT_STEP, pid, (caddr_t)1, 0) || wait_stopped(pid, 1000))
            break;
        if (sys_ptrace(PT_GETREGS, pid, (caddr_t)&r, 0))
            break;
        if (r.r_rsp > saved.r_rsp) {
            result = (long)r.r_rax;
            break;
        }
    }
    sys_ptrace(PT_SETREGS, pid, (caddr_t)&saved, 0);
    return result;
}

/* ---- script ---------------------------------------------------------------- */

static uint8_t *g_script;
static size_t g_script_len;

static bool load_script(void)
{
    int fd = open(SCRIPT_PATH, O_RDONLY);
    if (fd < 0) {
        notify("No script at %s", SCRIPT_PATH);
        return false;
    }
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size <= 0 || st.st_size > 16 * 1024 * 1024) {
        close(fd);
        notify("Unreadable script %s", SCRIPT_PATH);
        return false;
    }
    size_t size = (size_t)st.st_size;
    char *src = malloc(size + 1);
    size_t got = 0;
    ssize_t n;
    while (src && got < size && (n = read(fd, src + got, size - got)) > 0)
        got += (size_t)n;
    close(fd);
    if (!src || got != size) {
        free(src);
        notify("Could not read %s", SCRIPT_PATH);
        return false;
    }
    src[size] = '\0';

    if (size >= 0x40 && *(const uint64_t *)src == GSC_MAGIC_T6) {
        g_script = (uint8_t *)src;
        g_script_len = size;
        logmsg("using precompiled %s (%zu bytes)", SCRIPT_PATH, size);
        return true;
    }

    size_t out_len = 0;
    const void *obj = GSC_CompileT6(SCRIPT_NAME, src, size, &out_len);
    free(src);
    if (!obj || out_len < 0x40) {
        notify("Compile failed: %s", GSC_CompileError());
        return false;
    }
    g_script = malloc(out_len);
    if (!g_script)
        return false;
    memcpy(g_script, obj, out_len);
    g_script_len = out_len;
    logmsg("compiled %s: %zu bytes", SCRIPT_PATH, out_len);
    return true;
}

/* ---- game detection and patching ------------------------------------------- */

static bool is_zombies(pid_t pid, intptr_t *base_out, bool explain)
{
    intptr_t base = kernel_dynlib_mapbase_addr(pid, 0);
    if (!base) {
        if (explain) logmsg("pid %d: no executable base yet", pid);
        return false;
    }
    char probe[16] = {0};
    if (kernel_proc_copyout(pid, base + OFF_DETECT, probe, sizeof(probe)) != 0) {
        if (explain) logmsg("pid %d: base 0x%lx, detect string unreadable", pid, (long)base);
        return false;
    }
    if (memcmp(probe, "ui_multiplayer", 15) != 0) {
        if (explain) {
            char shown[16];
            for (int i = 0; i < 15; i++)
                shown[i] = probe[i] >= 0x20 && probe[i] < 0x7f ? probe[i] : '.';
            shown[15] = '\0';
            logmsg("pid %d: base 0x%lx, not the zombies build (found \"%s\")", pid, (long)base, shown);
        }
        return false;
    }
    *base_out = base;
    return true;
}

static int write_code(pid_t pid, intptr_t addr, const void *bytes, size_t len)
{
    /* Debugger write path first: the kernel copies on write into the text. */
    struct ptrace_io_desc io = {
        .piod_op = PIOD_WRITE_I,
        .piod_offs = (void *)addr,
        .piod_addr = (void *)bytes,
        .piod_len = len,
    };
    if (sys_ptrace(PT_IO, pid, (caddr_t)&io, 0) == 0 && io.piod_len == len)
        return 0;
    logmsg("PT_IO write failed (errno %d), trying mprotect", errno);

    intptr_t page = addr & ~(intptr_t)0x3FFF;
    size_t span = (size_t)((addr + (intptr_t)len + 0x3FFF) & ~(intptr_t)0x3FFF) - (size_t)page;
    int old = kernel_get_vmem_protection(pid, page, span);
    if (kernel_mprotect(pid, page, span, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        logmsg("kernel_mprotect RWX failed");
        return -1;
    }
    int rc = kernel_proc_copyin(pid, bytes, addr, len);
    if (old > 0)
        kernel_mprotect(pid, page, span, old);
    return rc;
}

static bool patch_game(pid_t pid, intptr_t base)
{
    const intptr_t target = base + OFF_DB_FIND;

    uint8_t current[PATCH_SIZE];
    if (kernel_proc_copyout(pid, target, current, sizeof(current)) == 0) {
        bool original = memcmp(current, k_prologue, PATCH_SIZE) == 0;
        bool ours = current[0] == 0x48 && current[1] == 0xB8 &&
                    current[10] == 0xFF && current[11] == 0xE0;
        if (!original && !ours) {
            notify("Unknown game version (pid %d): not patching", pid);
            return false;
        }
        logmsg("pid %d prologue is %s", pid, original ? "original" : "already hooked");
    } else {
        logmsg("pid %d prologue not readable (execute-only), relying on string check", pid);
    }

    /* Two allocations, because PS5 executable memory is execute-only: the
       hook code (never read as data), and the tree, name and script that the
       hook and the game read. Data layout: tree at 0, name at 0x20, script
       at 0x80. */
    const size_t code_len = (size_t)(deadops_hook_end - deadops_hook_begin);
    const size_t code_map = 0x4000;
    const size_t data_len = 0x80 + g_script_len;
    const size_t data_map = (data_len + 0x3FFF) & ~(size_t)0x3FFF;

    uint8_t *code = malloc(code_len);
    uint8_t *data = calloc(1, data_len);
    if (!code || !data) {
        free(code);
        free(data);
        return false;
    }

    if (sys_ptrace(PT_ATTACH, pid, 0, 0) != 0 || wait_stopped(pid, 2000) != 0) {
        notify("Could not attach to BO2 (pid %d, errno %d)", pid, errno);
        free(code);
        free(data);
        return false;
    }

    bool ok = false;
    long data_mem = remote_syscall(pid, SYS_mmap, 0, data_map, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANON, (uint64_t)-1, 0);
    long code_mem = remote_syscall(pid, SYS_mmap, 0, code_map, PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE | MAP_ANON, (uint64_t)-1, 0);
    if (data_mem <= 0 || code_mem <= 0) {
        notify("Remote mmap failed in BO2 (%ld, %ld)", data_mem, code_mem);
        goto detach;
    }

    const uint64_t tree_ptr = (uint64_t)data_mem;
    const uint64_t name_ptr = (uint64_t)data_mem + 0x20;
    const uint64_t buf_ptr = (uint64_t)data_mem + 0x80;
    const uint32_t len32 = (uint32_t)g_script_len;
    memcpy(data + 0x00, &name_ptr, 8);
    memcpy(data + 0x08, &len32, 4);
    memcpy(data + 0x10, &buf_ptr, 8);
    memcpy(data + 0x20, SCRIPT_NAME, sizeof(SCRIPT_NAME));
    memcpy(data + 0x80, g_script, g_script_len);

    const uint64_t ret_addr = (uint64_t)(target + PATCH_SIZE);
    memcpy(code, deadops_hook_begin, code_len);
    memcpy(code + (deadops_hook_name_imm - deadops_hook_begin), &name_ptr, 8);
    memcpy(code + (deadops_hook_tree_imm - deadops_hook_begin), &tree_ptr, 8);
    memcpy(code + (deadops_hook_ret_imm - deadops_hook_begin), &ret_addr, 8);

    if (kernel_proc_copyin(pid, data, data_mem, data_len) != 0 ||
        kernel_proc_copyin(pid, code, code_mem, code_len) != 0) {
        notify("Could not write hook into BO2");
        goto detach;
    }
    if (kernel_mprotect(pid, code_mem, code_map, PROT_READ | PROT_EXEC) != 0) {
        notify("Could not make hook memory executable");
        goto detach;
    }

    /* movabs rax, hook; jmp rax: the target address is an immediate, so the
       patched (execute-only) game code is never read as data. */
    uint8_t patch[PATCH_SIZE];
    memset(patch, 0x90, sizeof(patch));
    const uint64_t hook_addr = (uint64_t)code_mem;
    patch[0] = 0x48;
    patch[1] = 0xB8;
    memcpy(patch + 2, &hook_addr, 8);
    patch[10] = 0xFF;
    patch[11] = 0xE0;

    if (write_code(pid, target, patch, sizeof(patch)) != 0) {
        notify("Could not patch DB_FindXAssetHeader (pid %d)", pid);
        goto detach;
    }

    logmsg("pid %d: hook code 0x%lx, script %zu bytes at 0x%lx, DB_FindXAssetHeader 0x%lx",
           pid, code_mem, g_script_len, (long)buf_ptr, (long)target);
    ok = true;

detach:
    sys_ptrace(PT_DETACH, pid, (caddr_t)1, 0);
    free(code);
    free(data);
    return ok;
}

/* ---- process scan ----------------------------------------------------------- */

static int list_pids(pid_t *out, int max)
{
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0 };
    size_t size = 0;
    if (sysctl(mib, 4, NULL, &size, NULL, 0) != 0)
        return 0;
    uint8_t *buf = malloc(size);
    if (!buf || sysctl(mib, 4, buf, &size, NULL, 0) != 0) {
        free(buf);
        return 0;
    }
    int count = 0;
    for (uint8_t *p = buf; p + 0x4C <= buf + size && count < max;) {
        int len = *(int *)p;              /* kinfo_proc.ki_structsize */
        if (len < 0x4C || p + len > buf + size)
            break;
        pid_t pid = *(pid_t *)(p + 0x48); /* kinfo_proc.ki_pid */
        if (pid > 1)
            out[count++] = pid;
        p += len;
    }
    free(buf);
    return count;
}

static bool is_title(pid_t pid)
{
    app_info_t info;
    memset(&info, 0, sizeof(info));
    return sceKernelGetAppInfo(pid, &info) == 0 && strcmp(info.title_id, TITLE_ID) == 0;
}

static bool acquire_lock(void)
{
    int fd = open(LOCK_PATH, O_RDWR | O_CREAT, 0666);
    if (fd < 0)
        return true;
    char buf[16] = {0};
    if (read(fd, buf, sizeof(buf) - 1) > 0) {
        pid_t other = (pid_t)atoi(buf);
        if (other > 1 && other != getpid() && kill(other, 0) == 0) {
            close(fd);
            return false;
        }
    }
    ftruncate(fd, 0);
    lseek(fd, 0, SEEK_SET);
    int n = snprintf(buf, sizeof(buf), "%d\n", getpid());
    (void)write(fd, buf, (size_t)n);
    close(fd);
    return true;
}

int main(void)
{
    escalate_self();
    mkdir("/data/DeadOps", 0777);
    unlink(LOG_PATH);

    if (!acquire_lock()) {
        notify("Already running");
        return 0;
    }
    logmsg("DeadOps-PS5 started (pid %d)", getpid());

    if (!load_script()) {
        unlink(LOCK_PATH);
        return 1;
    }
    notify("Script ready (%zu bytes). Start Zombies now.", g_script_len);

    pid_t patched[MAX_SEEN];
    int patched_count = 0;
    pid_t explained[MAX_SEEN];
    int explained_count = 0;
    long start = now_ms();
    long last_seen = 0;

    for (;;) {
        pid_t pids[1024];
        int n = list_pids(pids, 1024);
        bool title_running = false;

        for (int i = 0; i < n; i++) {
            if (!is_title(pids[i]))
                continue;
            title_running = true;

            bool done = false;
            for (int k = 0; k < patched_count; k++)
                done |= patched[k] == pids[i];
            if (done)
                continue;

            bool explain = true;
            for (int k = 0; k < explained_count; k++)
                explain &= explained[k] != pids[i];
            if (explain && explained_count < MAX_SEEN)
                explained[explained_count++] = pids[i];

            intptr_t base;
            if (!is_zombies(pids[i], &base, explain))
                continue;

            if (patch_game(pids[i], base)) {
                notify("Mod menu loaded into Zombies. Double-tap melee in a match.");
                if (patched_count < MAX_SEEN)
                    patched[patched_count++] = pids[i];
            } else if (patched_count < MAX_SEEN) {
                patched[patched_count++] = pids[i];   /* don't retry a failure forever */
            }
        }

        long now = now_ms();
        if (title_running)
            last_seen = now;
        if (patched_count == 0 && now - start > WAIT_FOR_GAME_MS) {
            notify("Timed out waiting for BO2 Zombies");
            break;
        }
        if (patched_count > 0 && !title_running && now - last_seen > LINGER_AFTER_MS)
            break;
        usleep(500000);
    }

    unlink(LOCK_PATH);
    logmsg("DeadOps-PS5 exiting");
    return 0;
}
