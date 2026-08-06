#define _GNU_SOURCE

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>

static volatile sig_atomic_t terminate_requested = 0;
static volatile sig_atomic_t child_exited = 0;
static volatile sig_atomic_t child_status = 0;
static pid_t child_pid = -1;

static void log_message(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
    fputc('\n', stdout);
}

static void error_message(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

static void ensure_directory(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return;
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        error_message("[Noah] WARNING: Failed to create %s: %s", path, strerror(errno));
    }
}

static void ensure_console(void) {
    struct stat st;
    if (stat("/dev/console", &st) == 0) return;

    if (getuid() != 0) return;

    ensure_directory("/dev");
    if (mknod("/dev/console", S_IFCHR | 0600, makedev(5, 1)) != 0) {
        error_message("[Noah] WARNING: Failed to create /dev/console: %s", strerror(errno));
    }
}

static void safe_mount(const char *source, const char *target, const char *fstype, unsigned long flags, const char *data) {
    ensure_directory(target);
    if (mount(source, target, fstype, flags, data) != 0) {
        if (errno == EBUSY || errno == EINVAL || errno == EPERM || errno == ENOENT) {
            return;
        }
        error_message("[Noah] WARNING: Failed to mount %s on %s: %s", source, target, strerror(errno));
    }
}

static char *read_config_value(const char *path, const char *key) {
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;

    char line[256];
    size_t key_len = strlen(key);
    char *result = NULL;

    while (fgets(line, sizeof(line), fp)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        if (strncmp(p, key, key_len) != 0 || p[key_len] != '=') continue;
        p += key_len + 1;
        char *end = p + strlen(p) - 1;
        while (end >= p && (*end == '\n' || *end == '\r' || *end == ' ' || *end == '\t')) {
            *end-- = '\0';
        }
        result = strdup(p);
        break;
    }

    fclose(fp);
    return result;
}

static char *get_config_value(const char *env_key, const char *file_key) {
    char *value = getenv(env_key);
    if (value && *value) return strdup(value);
    return read_config_value("/etc/noah.conf", file_key);
}

static bool parse_id(const char *value, uid_t *result) {
    char *endptr;
    long parsed = strtol(value, &endptr, 10);
    if (endptr == value || *endptr != '\0' || parsed < 0) return false;
    *result = (uid_t)parsed;
    return true;
}

static bool get_user_ids(const char *name, uid_t *uid, gid_t *gid) {
    FILE *fp = fopen("/etc/passwd", "r");
    if (!fp) return false;

    char line[256];
    bool found = false;
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char *user = strtok(line, ":\n");
        if (!user || strcmp(user, name) != 0) continue;

        strtok(NULL, ":\n"); /* password */
        char *uid_str = strtok(NULL, ":\n");
        char *gid_str = strtok(NULL, ":\n");
        if (!uid_str || !gid_str) break;

        *uid = (uid_t)strtoul(uid_str, NULL, 10);
        *gid = (gid_t)strtoul(gid_str, NULL, 10);
        found = true;
        break;
    }

    fclose(fp);
    return found;
}

static bool drop_privileges(void) {
    const char *user_env = getenv("NOAH_USER");
    const char *uid_env = getenv("NOAH_UID");
    const char *gid_env = getenv("NOAH_GID");
    uid_t uid = (uid_t)-1;
    gid_t gid = (gid_t)-1;

    if (user_env && *user_env) {
        if (!get_user_ids(user_env, &uid, &gid)) {
            error_message("[Noah] ERROR: unknown user %s", user_env);
            return false;
        }
    }

    if (gid_env && *gid_env) {
        if (!parse_id(gid_env, &gid)) {
            error_message("[Noah] ERROR: invalid NOAH_GID value %s", gid_env);
            return false;
        }
    }

    if (uid_env && *uid_env) {
        if (!parse_id(uid_env, &uid)) {
            error_message("[Noah] ERROR: invalid NOAH_UID value %s", uid_env);
            return false;
        }
    }

    if (uid == (uid_t)-1 && gid == (gid_t)-1) return true;

    if (getuid() != 0) return true;

    if (gid != (gid_t)-1) {
        if (setgid(gid) != 0) {
            error_message("[Noah] ERROR: unable to setgid(%u): %s", (unsigned)gid, strerror(errno));
            return false;
        }
        if (setgroups(1, &gid) != 0) {
            error_message("[Noah] WARNING: unable to setgroups(%u): %s", (unsigned)gid, strerror(errno));
        }
    }

    if (uid != (uid_t)-1) {
        if (setuid(uid) != 0) {
            error_message("[Noah] ERROR: unable to setuid(%u): %s", (unsigned)uid, strerror(errno));
            return false;
        }
    }

    return true;
}

static int run_init_script(const char *path) {
    if (access(path, X_OK) != 0) return 0;

    pid_t pid = fork();
    if (pid < 0) {
        error_message("[Noah] ERROR: Failed to fork for init script %s: %s", path, strerror(errno));
        return -1;
    }

    if (pid == 0) {
        execl(path, path, (char *)NULL);
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        error_message("[Noah] WARNING: init script %s terminated abnormally: %s", path, strerror(errno));
    }

    return 0;
}

static void run_init_scripts(const char *dir) {
    struct dirent **entries;
    int count = scandir(dir, &entries, NULL, alphasort);
    if (count <= 0) return;

    for (int i = 0; i < count; ++i) {
        if (!entries[i]) continue;
        const char *name = entries[i]->d_name;
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            free(entries[i]);
            continue;
        }

        char path[512];
        snprintf(path, sizeof(path), "%s/%s", dir, name);
        run_init_script(path);
        free(entries[i]);
    }
    free(entries);
}

static void handle_signal(int signum) {
    if (signum == SIGCHLD) {
        int status;
        pid_t pid;
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            if (pid == child_pid) {
                child_exited = 1;
                child_status = status;
            }
        }
        return;
    }

    if (signum == SIGTERM || signum == SIGINT || signum == SIGQUIT || signum == SIGHUP) {
        terminate_requested = 1;
        if (child_pid > 0) {
            kill(-child_pid, signum);
        }
    }
}

static void setup_signal_handlers(void) {
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NOCLDSTOP | SA_RESTART;
    sa.sa_handler = handle_signal;
    sigaction(SIGCHLD, &sa, NULL);

    sa.sa_flags = SA_RESTART;
    sa.sa_handler = handle_signal;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
    sigaction(SIGTTIN, &sa, NULL);
    sigaction(SIGTTOU, &sa, NULL);
}

static pid_t spawn_workload(char *const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        if (setpgid(0, 0) != 0) {
            error_message("[Noah] WARNING: Failed to set child process group: %s", strerror(errno));
        }

        if (isatty(STDIN_FILENO)) {
            ioctl(STDIN_FILENO, TIOCSCTTY, 1);
        }

        execvp(argv[0], argv);
        error_message("[Noah] FATAL ERROR: Failed to exec %s: %s", argv[0], strerror(errno));
        _exit(127);
    }

    if (isatty(STDIN_FILENO)) {
        setpgid(pid, pid);
        tcsetpgrp(STDIN_FILENO, pid);
    }

    return pid;
}

static int wait_for_child(pid_t pid) {
    int status = 0;

    while (!terminate_requested) {
        pid_t w = waitpid(pid, &status, 0);
        if (w == pid) break;
        if (w < 0 && errno == EINTR) continue;
        if (w < 0 && errno == ECHILD && child_exited) {
            status = child_status;
            break;
        }
        break;
    }

    if (terminate_requested && child_pid > 0) {
        kill(-child_pid, SIGKILL);
        waitpid(child_pid, &status, 0);
    }

    return status;
}

int main(int argc, char *argv[]) {
    prctl(PR_SET_NAME, "Noah", 0, 0, 0);
    prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0);

    setup_signal_handlers();

    ensure_directory("/etc");
    ensure_directory("/dev");
    ensure_directory("/proc");
    ensure_directory("/sys");
    ensure_directory("/etc/init.d");

    safe_mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    safe_mount("proc", "/proc", "proc", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL);
    safe_mount("sysfs", "/sys", "sysfs", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL);
    ensure_console();

    if (!drop_privileges()) {
        error_message("[Noah] FATAL ERROR: unable to drop privileges");
        return 1;
    }

    printf("\n===================================================\n");
    printf("   Boot successful! Welcome to Ark OS.\n");
    printf("   I am PID 1, and my name is Noah.\n");
    printf("===================================================\n\n");

    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin", 1);

    char *shell_path = get_config_value("NOAH_SHELL", "SHELL");
    if (!shell_path) shell_path = strdup("/bin/sh");

    char *init_command = get_config_value("NOAH_INIT", "INIT");
    char *restart_value = get_config_value("NOAH_RESTART", "RESTART");
    int restart = 0;
    int restart_on_failure = 0;

    if (restart_value) {
        if (strcmp(restart_value, "always") == 0) restart = 1;
        else if (strcmp(restart_value, "on-failure") == 0) restart_on_failure = 1;
    }

    run_init_scripts("/etc/init.d");

    int exit_code = 0;
    do {
        if (argc > 1) {
            child_pid = spawn_workload(&argv[1]);
        } else if (init_command) {
            char *child_args[] = {"/bin/sh", "-c", init_command, NULL};
            child_pid = spawn_workload(child_args);
        } else {
            char *child_args[] = {shell_path, NULL};
            if (isatty(STDIN_FILENO)) {
                char *interactive_args[] = {shell_path, "-i", NULL};
                child_pid = spawn_workload(interactive_args);
            } else {
                child_pid = spawn_workload(child_args);
            }
        }

        if (child_pid < 0) {
            error_message("[Noah] FATAL ERROR: unable to launch workload: %s", strerror(errno));
            exit_code = 125;
            break;
        }

        int status = wait_for_child(child_pid);
        child_pid = -1;
        child_exited = 0;

        if (WIFEXITED(status)) {
            exit_code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            exit_code = 128 + WTERMSIG(status);
        } else {
            exit_code = 1;
        }

        if (terminate_requested) break;
        if (restart_on_failure && exit_code == 0) break;
    } while (restart);

    sync();
    free(shell_path);
    free(init_command);
    free(restart_value);

    return exit_code;
}
