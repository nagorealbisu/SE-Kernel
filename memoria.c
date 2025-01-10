#include "memoria.h"

extern int frame_libreak[FRAME_TAMAINA];

void memoria_fisikoa_hasieratu(){
    mem_fisikoa.tamaina = MEM_FISK_BIT_KOP;
    printf("Memoria fisikoaren tamaina: %d\n", mem_fisikoa.tamaina);
    mem_fisikoa.memoria = calloc(MEM_FISK_BIT_KOP, sizeof(char));
    memset(mem_fisikoa.memoria, 0, MEM_FISK_BIT_KOP);
}

void memoria_fisikoa_askatu(){
    free(mem_fisikoa.memoria);
}

void frameak_hasieratu() {
    for (int i = 0; i < FRAME_TAMAINA; i++) {
        frame_libreak[i] = 1;
    }
}

int frame_librea_lortu() {
    for (int i = 0; i < FRAME_TAMAINA; i++) { // 000264 helbidetik hasi
        if (frame_libreak[i]) {
            frame_libreak[i] = 0;
            return i;
        }
    }
    return -1;
}

void framea_askatu(int frame) {
    if (frame >= 0 && frame < FRAME_TAMAINA) {
        frame_libreak[frame] = 1;
    }
}

void orriak_hasieratu(pcb *p, int orri_kop) {
    p->orri_taula = malloc(sizeof(TBL) * orri_kop);
    for (int i = 0; i < orri_kop; i++) {
        p->orri_taula[i].page_num = i;
        int frame = frame_librea_lortu();
        if (frame != -1) {
            p->orri_taula[i].frame_zenb = frame;
            p->orri_taula[i].valid = 1; // valid
        } else {
            p->orri_taula[i].frame_zenb = -1; // framerik ezin izan da lortu
            p->orri_taula[i].valid = 0;     // invalid
            printf("Errorea: frame librerik ez %d.\n", i);
        }
    }
}

void framea_esleitu(pcb *prozesua, int orri, int frame) {
    prozesua->orri_taula[orri].frame_zenb = frame;
    prozesua->orri_taula[orri].valid = 1;
}

int memoria_fisikoa_irakurri(int helbide_fisikoa) {
    int balioa;
    memcpy(&balioa, &mem_fisikoa.memoria[helbide_fisikoa], sizeof(int));
    return balioa;
}

int memoria_birtuala_irakurri(struct CPU_thread *cpu, int helbide_birtuala) {
    int helbide_fisikoa = mmu_helbidea_itzuli(cpu, helbide_birtuala);
    printf("\n MEMORIA BIRTUALAREN HELBIDEA: %08X\n", helbide_birtuala);
    if (helbide_fisikoa == -1) return 0;
    printf("\n MEMORIA FISIKOAREN HELBIDEA: %08X\n", helbide_fisikoa);
    return helbide_fisikoa;
}

void memoria_fisikoa_inprimatu() {
    printf("Memoria fisikoaren edukia:\n");
    printf("Helbidea\t\tEdukia\n");
    printf("=====================================\n");

    for (int i = 0; i < MEM_F_TAM_MAX/2; i += HITZ) {
        printf("0x%06X\t", i);

        printf("%08X", *(unsigned int *)&mem_fisikoa.memoria[i]);
        printf("\n");
    }

    printf("=====================================\n");

    for (int i = 512; i < 640; i += HITZ) {
        printf("0x%06X\t", i);

        printf("%08X", *(unsigned int *)&mem_fisikoa.memoria[i]);
        printf("\n");
    }

    printf("=====================================\n");
}

void mmu_hasieratu(struct CPU_thread *cpu) {
    int page_table_start = strtol(PAGE_TABLE_HELB, NULL, 16);
    int page_table_size = sizeof(TBL) * (MEM_F_TAM_MAX / HITZ);
    
    cpu->mmu.tbl = malloc(page_table_size);
    if (!cpu->mmu.tbl) {
        printf("Errorea MMU-an. Ateratzen...\n");
        exit(1);
    }

    int value = 0;
    for(int i = 0; i < page_table_start / sizeof(TBL); i++){
        memcpy(&mem_fisikoa.memoria[page_table_start + i*HITZ], (unsigned int *)&value, HITZ);
        //printf("\n\n\n\t\tMEMORIA FISIKOAREN EDUKIA: %08X\n\n\n", *(unsigned int *)&mem_fisikoa.memoria[page_table_start + i*HITZ]);
        value = i*4;
    }
    fflush(stdout);

    for (int i = 0; i < (MEM_F_TAM_MAX / HITZ); i++) {
        int direccion_fisica = page_table_start + (i * HITZ);
        memcpy(&cpu->mmu.tbl[i], &mem_fisikoa.memoria[direccion_fisica], sizeof(TBL));
    }
    printf("\nMMU-a ondo hasieratu da.\n"); fflush(stdout);
}

int mmu_helbidea_itzuli(void *param, int helbide_logikoa) {
    struct CPU_thread *cpu = (struct CPU_thread *)param;
    int orr = helbide_logikoa / HITZ;
    int offset = helbide_logikoa % HITZ;
    
    int datua;
    memcpy(&datua, &mem_fisikoa.memoria[helbide_logikoa], sizeof(int));
    printf("HELBIDEA ITZULI: %08X\n", datua);
    return datua;
}

void memoria_birtualean_idatzi(struct CPU_thread *cpu, int helbide_birtuala, int balioa) {
    int helbide_fisikoa = mmu_helbidea_itzuli(cpu, helbide_birtuala);
    
    if (helbide_fisikoa == -1) return;

    printf("Helbide fisikoan idazten: %08X, birtuala: %08X: %08X\n", helbide_fisikoa, helbide_birtuala, balioa);
    memcpy(&mem_fisikoa.memoria[helbide_birtuala], &balioa, sizeof(int));
}