#include "cpu.h"
#include "memoria.h"

void *cpu_hari(void *param) {
    struct CPU_thread *cpu = (struct CPU_thread *)param;
    mmu_hasieratu(cpu);

    while (1) {
        pthread_cond_wait(&cond_cpu, &mutex_prozesua);

        printf("\nCPU: PID = %d duen prozesua exekutatzen...\n", cpu->uneko_pcb.pid);
        cpu->egoera.PC = cpu->uneko_pcb.mm.code + cpu->egoera.PC;

        int helbide_birtuala = cpu->egoera.PC + strtol(PAGE_TABLE_HELB, NULL, 16);
        printf("HELBIDE BIRTUALA PC: %d , PAGE_TABLE_HELB: %d\n", cpu->egoera.PC, helbide_birtuala);
        cpu->egoera.IR = memoria_birtuala_irakurri(cpu, cpu->egoera.PC);

        printf("\n\tIR: 0x%08X\n", cpu->egoera.IR);
        printf("PC: 0x%X\n", cpu->egoera.PC);

        int out = agindua(cpu);
        printf("Emaitza: %d", out);
        cpu->egoera.PC += 4;
        printf("PC: 0x%X\n", cpu->egoera.PC);

        pthread_cond_signal(&cond_prozesua);
    }
}

int agindua(void *param1) {
    struct CPU_thread *cpu = (struct CPU_thread *)param1;
    
    int agindua;
    agindua = (cpu->egoera.IR >> 28) & 0xF;
    
    int erreg; // erregistroa
    int helbide_birtuala;
    
    switch (agindua) {
        case 0x0: // ld
            erreg = (cpu->egoera.IR >> 24) & 0xF;
            helbide_birtuala = cpu->egoera.IR & 0xFFFFFF;
            cpu->egoera.erregistroak[erreg] = memoria_birtuala_irakurri(cpu, helbide_birtuala);
            printf("\n\t\033[33mld\033[0m\n\n");
            //memoria_fisikoa_inprimatu();
            break;
        case 0x1: // st
            erreg = (cpu->egoera.IR >> 24) & 0xF;  // R: Erregistroa
            helbide_birtuala = cpu->egoera.IR & 0xFFFFFF;
            memoria_birtualean_idatzi(cpu, helbide_birtuala, cpu->egoera.erregistroak[erreg]);
            printf("\n\t\033[34mstore\033[0m\n\n");
            memoria_fisikoa_inprimatu();
            break;
        case 0x2: // add
            int erreg1 = (cpu->egoera.IR >> 24) & 0xF;  // R1
            int erreg2 = (cpu->egoera.IR >> 20) & 0xF;  // R2
            int erreg3 = (cpu->egoera.IR >> 16) & 0xF;  // R3
            helbide_birtuala = cpu->egoera.IR & 0xFFFFFF;
            printf("R1: %d, R2: %d, R3: %d\n", cpu->egoera.erregistroak[erreg1], cpu->egoera.erregistroak[erreg2], cpu->egoera.erregistroak[erreg3]);
            cpu->egoera.erregistroak[erreg1] = cpu->egoera.erregistroak[erreg2] + cpu->egoera.erregistroak[erreg3];

            printf("\n\t\033[31madd\033[0m\n\n");
            break;
        case 0xF: // exit
            printf("\n\t\033[32mexit\033[0m\n\n");
            return 0;
        default:
            printf("Agindu ezezaguna: 0x%X\n", agindua);
            return 0;
    }
    return 1;
}

makina getSystemConfig() {
    makina sys_config;

    sys_config.num__threads = sysconf(_SC_NPROCESSORS_ONLN);
    sys_config.num_cores = sys_config.num__threads;
    sys_config.num_hariak = sys_config.num__threads;

    printf("\n--- Sistemaren konfigurazioa ---\n");
    printf("CPU_thread kopurua: %d\n", sys_config.num__threads);
    //printf("Core kopurua: %d\n", sys_config.num_cores);
    //printf("Hari kopurua: %d\n", sys_config.num_hariak);
    printf("----------------------------------\n");

    return sys_config;
}