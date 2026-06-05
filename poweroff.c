#include <stdio.h>
#include <unistd.h>
#include <sys/reboot.h>

int main() {
    printf("\n===================================================\n");
    printf("   [Ark OS] Powering down... Goodbye!\n");
    printf("===================================================\n\n");
    
    // Force the filesystem to save any pending changes
    sync();
    
    // Tell the Linux kernel to cut the power
    reboot(RB_POWER_OFF);
    
    return 0;
}
