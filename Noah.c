#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>

int main() {
    // 1. Set the process name
    prctl(PR_SET_NAME, "Noah", 0, 0, 0);

    // 2. Mount filesystems and check for errors!
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) != 0) {
        printf("[Noah] WARNING: Failed to mount /dev! Error: %s\n", strerror(errno));
    }
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);

    // Mute non-critical kernel logs
    int printk_fd = open("/proc/sys/kernel/printk", O_WRONLY);
    if (printk_fd >= 0) {
        // "3 4 1 3" tells the kernel to only print KERN_ERR (3) and higher to the console
        write(printk_fd, "3 4 1 3\n", 8);
        close(printk_fd);
    }

    printf("\n===================================================\n");
    printf("   Boot successful! Welcome to Ark OS.\n");
    printf("   I am PID 1, and my name is Noah.\n");
    printf("===================================================\n\n");

    setenv("PATH", "/bin:/sbin:/usr/bin:/usr/sbin", 1);

    // 3. The Process Loop
    while(1) {
        pid_t pid = fork(); 
        
        if (pid == 0) {
            // ---> CHILD PROCESS <---
            
            // Start a new session
            setsid();
            
            // Open the terminal specifically for this child session
            int fd = open("/dev/console", O_RDWR);
            if (fd < 0) {
                printf("[Noah] WARNING: Could not open /dev/console! Error: %s\n", strerror(errno));
                // Fallback to the QEMU serial port
                fd = open("/dev/ttyS0", O_RDWR);
            }
            
            // Wire up keyboard and screen
            if (fd >= 0) {
                dup2(fd, 0);
                dup2(fd, 1);
                dup2(fd, 2);
                if (fd > 2) close(fd);
                ioctl(0, TIOCSCTTY, 1);
            }

            // Attempt to launch the shell
            char *args[] = {"sh", NULL};
            execv("/bin/sh", args);
            
            // --- IF WE REACH THIS LINE, THE SHELL FAILED TO LAUNCH ---
            printf("\n[Noah] FATAL ERROR: Failed to execute /bin/sh! Error: %s\n", strerror(errno));
            
            // Fallback: Try launching BusyBox directly in case the symlinks are broken
            printf("[Noah] Attempting fallback to /bin/busybox sh...\n");
            char *bb_args[] = {"busybox", "sh", NULL};
            execv("/bin/busybox", bb_args);
            
            printf("[Noah] FATAL ERROR: Fallback failed too! Error: %s\n", strerror(errno));
            
            // Sleep for 5 seconds so the screen doesn't spam, then exit to trigger restart
            sleep(5); 
            exit(1); 
            
        } else if (pid > 0) {
            // ---> PARENT PROCESS (NOAH) <---
            wait(NULL);
            printf("\n[Noah] Shell exited. Restarting...\n");
            sleep(1);
        }
    }
    
    return 0;
}
