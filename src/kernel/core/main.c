// kernel/kernel.c
//libs
#include "main.h"
#include "types.h"
#include "stdio.h"
#include "kerror.h"
#include "GDT.h"
#include "IDT.h"
#include "io.h"
#include "memory.h"
#include "string.h"
#include "ps2_key.h"
#include "sleep.h"
#include "serial.h"
#include "pci.h"
#include "fs.h"
#include "vfs.h"
#include "ramfs.h"

#include "task.h"
#include "tty.h"
#include "compositor.h"
#include "lpcspeak.h"



#define KERNEL_VER_REL 0
#define KERNEL_VER_MAJ 1
#define KERNEL_VER_MIN 2
#define KERNEL_VER_CODENAME "ORBIT"
// drivers
#include "vga-textmode.h"
#include "pit.h"

struct inode* root_inode;

// temp
void no_sse(){
    vga_refresh();
    printf("FOUT: SSE is niet beschikbaar..\n");
    printf("zonder SSE kan deze versie van TBS-86 niet opstarten.");
    vga_refresh();
    return;
}

void kernel_main(uint32_t magic, struct multiboot_info* mbinfo) {
    if (magic != 0x1BADB002){
        //halt();
    }

    parse_memory_map(mbinfo);

    const char* cmdline = (const char*) mbinfo->cmdline;

    if (cmdline) {
    // Example parsing
    }


    
    vga_init();
    serial_print("vga init\n");

    // TODO: check multiboot checksum in parse_memory_map()

    GDT_install();
    IDT_install();


    
    pit_init(200);
    init_scheduler();
    
    // setup tty
    
    tty_init();
    serial_print("init tty\n");
    tty_t* tty0 = create_tty();
    tty_t* tty1 = create_tty();
    tty_t* tty2 = create_tty();
    active_tty = tty0;

    // setup ramfs
    root_inode = ramfs_create_root();
    vfs_init(root_inode);

    // scheduler tasks

    void console_task(){
        char input[64];
        char *argv[8];
        int argc;

        printf("TBS-86 Version: %d.%d.%d codename: %s\n",KERNEL_VER_REL, KERNEL_VER_MAJ, KERNEL_VER_MIN, KERNEL_VER_CODENAME);
        printf("\n");
        
        while(1){
            
            char cwd[64];
            memset(cwd, 0, 64);
            build_path(current_process->cwd, cwd);
           
            printf("%s $ ",cwd);

            
            tty_read_line(active_tty, input, 64);
            for (int i = 0; input[i]; i++) {
                if (input[i] == '\n' || input[i] == '\r') {
                    input[i] = '\0';
                    break;
                }
            }

            argc = 0;
            char *p = input;

            // Parse arguments
            while (*p) {

                while (*p == ' ')
                    p++;

                if (*p == '\0')
                    break;

                argv[argc++] = p;

                if (argc >= 64)
                    break;

                while (*p && *p != ' ')
                    p++;

                if (*p) {
                    *p = '\0';
                    p++;
                }
            }


            if(strcmp(argv[0], "ls") == 0){
                
                int fd = sys_open(cwd, 0);
                dirent_t ent;

                while (sys_readdir(fd, &ent) == 0) {
                    printf(ent.name);
                    printf("\n");
                }
                sys_close(fd);
                continue;

            }

            if(!argv[0]){
                continue;
            }

            if(strcmp(argv[0], "cd") == 0){
                int ret = sys_chdir(argv[1]);
                serial_print("Ret: %d",ret);
                continue;
            }

            if(strcmp(argv[0], "mkdir") == 0){
                sys_mkdir(argv[1]);
                continue;
            }


            if(strcmp(argv[0], "touch") == 0){
                sys_create(argv[1]);
                continue;
            }

            if(strcmp(argv[0], "clear") == 0){
                compositor_clear_screen();
                continue;
            }

            if(strcmp(argv[0], "cat") == 0){
                int fd = sys_open(argv[1],0);
                int size;
                if(atoi(argv[2]) != 0){
                    size = atoi(argv[2]);
                }else{
                    size = 512;
                }
               
                char buff[size];
                sys_read(fd,buff,size);
                printf("%s\n",buff);
                sys_close(fd);
                continue;
            }

            if(strcmp(argv[0], "wf") == 0){
                int fd = sys_open(argv[1],0);
                int size = strlen(argv[1]);
                sys_write(fd,argv[2], size);
                sys_close(fd);
                continue;
            }

            if(strcmp(argv[0], "lspci") == 0){
                pci_enumerate();
                continue;
            }

            SpeakerBlip();
            printf("?\n");

            
        
        }
        
    }

    
    process_t* console_app =create_process("cons", (void*)console_task);

    serial_print("create task made\n");
    task_t* compositor_task = create_ktask((void*)compositor_main, 0);
    serial_print("comp task made\n");
    active_tty->task_read_wait = console_app->main_task;
    active_tty->task_backend_wait = compositor_task;
    tty1->task_backend_wait = compositor_task;
    console_app->main_task->tty = active_tty;

    serial_print("start mtd\n");
    speaker_sound_ok();
    start_multitasking();


    while (true)
    {
        halt();
    }
    
}

 
