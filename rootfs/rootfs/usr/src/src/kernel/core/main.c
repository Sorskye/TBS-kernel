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
#include "ps2_mouse.h"

#define KERNEL_VER_REL 0
#define KERNEL_VER_MAJ 1
#define KERNEL_VER_MIN 3
#define KERNEL_VER_CODENAME "POLESTAR"
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
       // halt();
    }

    parse_memory_map(mbinfo);

    const char* cmdline = (const char*) mbinfo->cmdline;

    if (cmdline) {
    // Example parsing
    }

    
    serial_print("vga init\n");

    // TODO: check multiboot checksum in parse_memory_map()

    GDT_install();
    IDT_install();

    pit_init(200);
    
    
    // setup tty
    
    tty_init();
    serial_print("init tty\n");
    tty_t* tty0 = create_tty();
    tty_t* tty1 = create_tty();
    tty_t* tty2 = create_tty();
    active_tty = tty0;


    // setup ramfs

    multiboot_module_t* mods = (multiboot_module_t*) mbinfo->mods_addr;

    if (mbinfo->mods_count > 0) {
        uint32_t mod_start_phys = mods[0].mod_start;
        uint32_t mod_end_phys   = mods[0].mod_end;

        serial_print("=== MODULE DIAGNOSTIC ===\n");
        serial_print("mod_start_phys = 0x%x, mod_end_phys = 0x%x\n", mod_start_phys, mod_end_phys);
        serial_print("Module size: %u bytes\n", mod_end_phys - mod_start_phys);
        
        // Check which PDE/PTE entries the module uses
        uint32_t start_pde = (mod_start_phys >> 22) & 0x3FF;
        uint32_t end_pde   = ((mod_end_phys - 1) >> 22) & 0x3FF;
        serial_print("Module uses PDE[%u] to PDE[%u] (page dirs)\n", start_pde, end_pde);
        
        extern uint32_t page_directory[];
        for (uint32_t pde_idx = start_pde; pde_idx <= end_pde && pde_idx < 256; pde_idx++) {
            uint32_t pde = page_directory[pde_idx];
            serial_print("  PDE[%u] = 0x%x %s\n", pde_idx, pde, 
                         (pde & 1) ? "(present)" : "(NOT PRESENT!)");
        }
        
        // Debug the page table entries for specific module pages
        debug_module_pte(mod_start_phys, mbinfo);
        debug_module_pte(mod_start_phys + 0x1000, mbinfo);  // second page
        debug_module_pte(mod_start_phys + 0x2000, mbinfo);  // third page

        void* fs_start = (void*)mod_start_phys;
        size_t fs_size = mod_end_phys - mod_start_phys;

        serial_print("fs_start = %p, fs_size = %d\n", fs_start, fs_size);

        // Verify page tables are intact before reading module
        verify_page_table_integrity();

        // Debug: print first 64 bytes
        uint8_t* debug_bytes = (uint8_t*)fs_start;
        serial_print("First 64 bytes of module: ");
        for (int i = 0; i < 64; i++) {
            serial_print("%02x ", debug_bytes[i]);
        }
        serial_print("\n");

        root_inode = ramfs_create_root(fs_start, fs_size);
    } else {
        root_inode = ramfs_create_root(NULL, 0);
    }

    vfs_init(root_inode);
    vga_init();
    init_scheduler();

    // scheduler tasks

    void console_task(){
        char input[64];
        char *argv[8];
        int argc;

        printf("TBS-86 Version: %d.%d.%d codename: %s\n",KERNEL_VER_REL, KERNEL_VER_MAJ, KERNEL_VER_MIN, KERNEL_VER_CODENAME);
        speaker_sound_ok();
        printf("\n");
        
        while(1){
            
            char cwd[256];
            memset(cwd, 0, 256);
            build_path(current_process->cwd, cwd);
           
            printf("%s $ ",cwd);

            
            tty_read_line(active_tty, input, 256);
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

            if(argc == 0){
                continue;
            }


            if(strcmp(argv[0], "ls") == 0){
                
                int fd = sys_open(cwd, 0);
                dirent_t ent;

                while (sys_readdir(fd, &ent) == 0) {
                    if(strcmp(ent.name, ".") == 0 || strcmp(ent.name, "..") == 0)
                        continue;
                    printf(" %c%s",'-',ent.name);
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
                int size = strlen(argv[2]);
                serial_print("size: %d",size);
                sys_write(fd,argv[2], size);
                sys_close(fd);
                continue;
            }

            if(strcmp(argv[0], "lspci") == 0){
                pci_enumerate();
                continue;
            }

            if(strcmp(argv[0], "reboot") == 0){
                outb(0x64, 0xFE);
                continue;
            }

            if(strcmp(argv[0], "panic") == 0){
                panic("Manual panic triggered", NULL);
                continue;
            }

            if(strcmp(argv[0], "cursor") == 0){
                printf("scanning for ps/2 mouse..\n");
                mouse_init();
                task_t* cursor_task = create_ktask((void*)CompositorEnableMouse, 0);
                continue;
            }

            SpeakerBlip();
            printf("?\n");
        }
        
    }

    
    
    process_t* console_app = create_process("cons", (void*)console_task);
    serial_print("console app %d\n", console_app->main_task->tid);
    serial_print("create task made\n");
    task_t* compositor_task = create_ktask((void*)compositor_main, 0);
    
    
    active_tty->task_read_wait = console_app->main_task;
    active_tty->task_backend_wait = compositor_task;
    tty1->task_backend_wait = compositor_task;
    console_app->main_task->tty = active_tty;

    serial_print("start mtd\n");
    start_multitasking();


    while (true)
    {
        halt();
    }
    
}

 
