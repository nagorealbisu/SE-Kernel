#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#define PROZ_KOP_MAX 10
#define QUANTUM 2

#define FCFS 1
#define RR 2
#define RussianRoulette 3

#define READY 0
#define RUNNING 1
#define FINISHED 2

#define HITZ 4 // 4 byte-ko hitzak
#define MEM_F_TAM_MAX 256 // 256 frame gehienez
#define HELBIDE_BUSA 24 // 24 bit --> 2**24 = 16.777.216 helbide, 0xFFFFFF azkena


int done = 0;
int clock_tick = 0;

#define ERREG_KOP 16

typedef struct CPU_egoera{
    int erregistroak[ERREG_KOP];
    int PC;
    int IR;
} CPU_egoera;

//PROGRAMAK
#define MAX_LINES 100
#define MAX_TEXT_SIZE 256
#define MAX_DATA_SIZE 256

typedef struct {
    unsigned int code_start; // .text helbidea
    unsigned int data_start; // .data helbidea
    unsigned int text[MAX_TEXT_SIZE]; // .text zatia, aginduak
    unsigned int data[MAX_DATA_SIZE]; // .data zatia, datuak
    int text_size;
    int data_size;
} Programa;

void *cpu_thread(void *param);
void *erlojua(void *param);
void *tenporizadorea();
void *prozesu_sortzailea(void *proz_kop);
void *scheduler();
void *cpu(void *param);
void print_pcb(void *param);
void prozesua(void *param);
int agindua(void *cpu);
void programaren_informazioa_inprimatu(Programa *programa);
int mmu_helbidea_itzuli(void *cpu, int direccion_logica);


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_sched = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_prozesuak = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_prozesua = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond_clock_tick = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_scheduler = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_loader = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_prozesua = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_cpu = PTHREAD_COND_INITIALIZER;


typedef struct memoria_fisikoa {
    unsigned char *memoria;
    int tamaina;
} memoria_fisikoa;

memoria_fisikoa mem_fisikoa; //memoria fisikoaren aldagaia sortu

#define MEM_FISK_BIT_KOP 8*HITZ*pow(2, 24)/256

void memoria_fisikoa_hasieratu(){
    mem_fisikoa.tamaina = MEM_FISK_BIT_KOP;
    printf("Memoria fisikoaren tamaina: %d\n", mem_fisikoa.tamaina);
    mem_fisikoa.memoria = calloc(MEM_FISK_BIT_KOP, sizeof(char));
    memset(mem_fisikoa.memoria, 0, MEM_FISK_BIT_KOP);
}

void memoria_fisikoa_askatu(){
    free(mem_fisikoa.memoria);
}

typedef struct TBL{
    int id;
    int page_num;
    int frame_zenb;
    int valid;
} TBL;

typedef struct MM{ // Memory Management
    int pgb; // orri-taularen helbide fisikoa
    int code; // kodearen segmentuaren helbide birtuala          .text                               @[0x000000]
    int data; //datuen segmentuaren helbide birtuala             .data                               @[0x000014]
} MM;

typedef struct pcb{
    int pid;
    int egoera;      //READY, RUNNING, FINISHED
    int ticks_left;
    int tick_kop;
    int quantum_left;
    TBL *orri_taula;
    MM mm;
} pcb;

typedef struct MMU{
    int id;
    TBL *tbl;
} MMU;

Programa program;


typedef struct CPU_thread{
    int id;
    int PC;
    int IR;
    int PTBR;
    MMU mmu;
    pcb uneko_pcb;
    CPU_egoera egoera;
} CPU_thread;

typedef struct{
    int num__threads;
    int num_cores;
    int num_hariak;
    CPU_thread *cpu_threads;
} makina;

makina systemConfig;

struct pcb ready_ilara[PROZ_KOP_MAX];
int prozesu_kop = 0;

int sched_tick = 1;
int pro_sor_tick = 1;
int quantum;// = 0;


#define FRAME_TAMAINA 64  // Frame-tamaina = orri-tamaina
int frame_libreak[FRAME_TAMAINA]; // frame-entzako bit-map-a

#define KERNEL_HASIERA_HELB "0001C0" //kernel-aren hasiera helbidea
#define PAGE_TABLE_HELB "000200" //prozesuen orri-taulak helbide hontatik aurrera gordeko dira.


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

void programa_kargatu_mem_fisikoan(pcb *prozesua, unsigned int *text, unsigned int *data, int text_size, int data_size) {
    // aginduak
    for (int i = 0; i < prozesua->tick_kop; i++) {
        int page = (prozesua->mm.code / HITZ) + i;
        if (prozesua->orri_taula[page].valid) {
            int frame = prozesua->orri_taula[page].frame_zenb;
            int offset = frame * HITZ;

            for (int j = 0; j < HITZ; j++) {
                mem_fisikoa.memoria[offset + j] = ((unsigned char *)&text[i])[j];
            }
            //printf("%d. TEXT HITZA %X\n", i, text[i]);
            //printf("MEMORIA FISIKOAREN EDUKIA: %08X\n", *(unsigned int *)&mem_fisikoa.memoria[offset]);
        } else {
            printf("Errorea: orri honek: %d, ez ditu frame-rik esleituta.\n", page);
        }
    }

    // Datuak
    
    for (int i = 0; i < data_size; i++) {
        int page = (prozesua->mm.data / HITZ) + i;
        if (prozesua->orri_taula[page].valid) {
            int frame = prozesua->orri_taula[page].frame_zenb;
            int offset = frame * HITZ; //+ strtol(PAGE_TABLE_HELB, NULL, 16);

            for (int j = 0; j < HITZ; j++) {
                mem_fisikoa.memoria[offset + j] = ((unsigned char *)&data[i])[j];
            }
            //printf("%d. DATA HITZA %X\n", i, data[i]);
            //printf("MEMORIA FISIKOAREN EDUKIA: %08X\n", *(unsigned int *)&mem_fisikoa.memoria[offset]);
        } else {
            printf("Errorea: orri honek: %d, ez ditu frame-rik esleituta.\n", page);
        }
    }
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

int memoria_fisikoa_irakurri(int helbide_fisikoa) {
    //printf("MEMORIAaaaaaaaaaaaaa: %08X\n", helbide_fisikoa);
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


void cpu_erregistroak_inprimatu(void *param){
    struct CPU_thread *cpu = (struct CPU_thread *)param;
    printf("\nCPU egoera:\n");
    for(int i=0; i<ERREG_KOP; i++){
        int balioa = memoria_birtuala_irakurri(cpu, cpu->egoera.erregistroak[i]);
        printf("r%d: %d\n", i, balioa);
    }
}


void *cpu_hari(void *param) {
    struct CPU_thread *cpu = (struct CPU_thread *)param;
    mmu_hasieratu(cpu);

    while (1) {
        pthread_cond_wait(&cond_cpu, &mutex_prozesua);

        printf("\nCPU: PID = %d duen prozesua exekutatzen...\n", cpu->uneko_pcb.pid);
        cpu->egoera.PC = cpu->uneko_pcb.mm.code + cpu->egoera.PC;

        printf("\nErregistroen edukia hasieran: ");
        //memoria_fisikoa_inprimatu();

        int helbide_birtuala = cpu->egoera.PC + strtol(PAGE_TABLE_HELB, NULL, 16);
        printf("HELBIDE BIRTUALA PC: %d , PAGE_TABLE_HELB: %d\n", cpu->egoera.PC, helbide_birtuala);
        cpu->egoera.IR = memoria_birtuala_irakurri(cpu, cpu->egoera.PC);

        printf("\n\tIR: 0x%08X\n", cpu->egoera.IR);
        printf("PC: 0x%X\n", cpu->egoera.PC);

        int out = agindua(cpu);
        printf("Emaitza: %d", out);
        cpu->egoera.PC += 4;
        printf("PC: 0x%X\n", cpu->egoera.PC);

        //memoria_fisikoa_inprimatu();

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
            cpu->egoera.erregistroak[erreg] = memoria_fisikoa_irakurri(helbide_birtuala);
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
            //printf("R1: %d, R2: %d, R3: %d\n", erreg1, erreg2, erreg3);
            printf("R1: %d, R2: %d, R3: %d\n", cpu->egoera.erregistroak[erreg1], cpu->egoera.erregistroak[erreg2], cpu->egoera.erregistroak[erreg3]);
            cpu->egoera.erregistroak[erreg1] = cpu->egoera.erregistroak[erreg2] + cpu->egoera.erregistroak[erreg3];

            printf("\n\t\033[31madd\033[0m\n\n");
            //printf("\n\tadd\n");
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

void prozesua(void *param){
    struct pcb *p = (struct pcb *)param;
    printf("Prozesua naiz. PID = %d\n", p->pid);
}

void *erlojua(void *param) {
    int *clock_pulse = (int *)param;
    while (1) {
        pthread_mutex_lock(&mutex); fflush(stdout);
        printf("\nClock tick %d\n", ++clock_tick);
        
        pthread_cond_broadcast(&cond_clock_tick);
        pthread_mutex_unlock(&mutex);
        
        sleep(*clock_pulse);
    }
    return NULL;
}

void *tenporizadorea(){

    while(1){
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cond_clock_tick, &mutex);

        if(clock_tick%pro_sor_tick == 0){   
            printf("Loader-aren tenporizadorea hasi da %d. tick-ean. \n", clock_tick);
            pthread_cond_signal(&cond_loader);
        }
        if(clock_tick%sched_tick == 0){
            printf("Scheduler-aren tenporizadorea hasi da %d. tick-ean. \n", clock_tick);
            pthread_cond_signal(&cond_scheduler);
        }
        pthread_mutex_unlock(&mutex);
    }
}

void *loader(void *arg) {
    char *programa = (char *)arg;  // .elf fitxategiaren izena
    pcb p;
    FILE *file = fopen(programa, "r");
    if (!file) {
        fprintf(stderr, "Error al abrir el archivo %s\n", programa);
        return NULL;
    }

    char buffer[256];
    int ald_helb = 0;

    // .text eta .data lerroak irakurri
    while (fgets(buffer, sizeof(buffer), file)) {
        if (strncmp(buffer, ".text", 5) == 0) {
            sscanf(buffer, ".text %x", &program.code_start);
        } else if (strncmp(buffer, ".data", 5) == 0) {
            sscanf(buffer, ".data %x", &program.data_start);
            ald_helb = ((int)program.data_start)/4;
            printf("DATA START: %d\n", ald_helb);
            break;
        }
    }

    // kode eta data lerro guztiak irakurri
    int reading_text = 1; // .text
    while (fgets(buffer, sizeof(buffer), file)) {
        unsigned int value;
        if (sscanf(buffer, "%x", &value) == 1) {
            if (reading_text && program.text_size < MAX_TEXT_SIZE) {
                program.text[program.text_size++] = value;
            } else if (!reading_text && program.data_size < MAX_DATA_SIZE) {
                program.data[program.data_size++] = value;
            }
        }

        if (program.text_size >= ald_helb) {
            reading_text = 0;
        }
    }

    fclose(file);

    // pcb-a hasieratu
    p.pid = prozesu_kop;
    p.egoera = READY;
    p.mm.code = program.code_start;
    p.mm.data = program.data_start;
    p.tick_kop = program.text_size;
    p.ticks_left = p.tick_kop;

    printf("\nProgramaren testuaren tamaina: %d\n", program.text_size);
    printf("Programaren datuen tamaina: %d\n", program.data_size);

    //memoria_fisikoa_inprimatu();

    printf("Orriak hasieratzen... ");
    orriak_hasieratu(&p, program.text_size + program.data_size);

    printf("\nPrograma memorian kargatzen... \n");

    programa_kargatu_mem_fisikoan(&p, program.text, program.data, program.text_size, program.data_size);
    memoria_fisikoa_inprimatu();

    pthread_mutex_lock(&mutex_prozesuak);
    ready_ilara[prozesu_kop++] = p;
    pthread_mutex_unlock(&mutex_prozesuak);

    printf("Loader: %s programa ondo kargatu da. PID %d\n", programa, p.pid);
    return NULL;
}

void *scheduler() {
    pthread_t p;
    while (1) {
        pthread_cond_wait(&cond_scheduler, &mutex_sched);
        if (prozesu_kop > 0) {
            pthread_mutex_lock(&mutex_prozesuak);
            pthread_cond_signal(&cond_cpu);
                printf("Scheduler: Prozesua CPU_thread-ari pasatzen... PID = %d\n", ready_ilara[0].pid);
                systemConfig.cpu_threads[0].uneko_pcb = ready_ilara[0];
            pthread_cond_wait(&cond_prozesua, &mutex_prozesua);
            pthread_mutex_unlock(&mutex_prozesuak);
            ready_ilara[0].ticks_left--;
            printf("\n\tTicks left = %d", ready_ilara[0].ticks_left);
            if(ready_ilara[0].ticks_left == 0){ // amaitu du
                ready_ilara[0].egoera = FINISHED;
                prozesu_kop--;
                printf("\nScheduler: Prozesua amaitu egin da.\n");
            }
        } else {
            printf("Scheduler: Ilara hutsik dago.\n");
        }
    }
    return NULL;
}

void print_pcb(void *param){
    struct pcb *p = (struct pcb *)param;
    printf("PCB { \n");
    printf("\t PID = %d\n", p->pid);
    printf("\t EGOERA = %d\n", p->egoera);
    printf("\t TICK kopurua = %d\n", p->tick_kop);
    printf("\t ticks left = %d\n", p->ticks_left);
    printf("\t quantum left = %d\n", p->quantum_left);
    printf("}\n");
}


int main(int argc, char *argv[]){ // argv[1] programa_izena

    if(argc != 2){
        printf("Erabilpena: %s <progXXX.elf>", argv[0]);
        exit(1);
    }

    char *programa_izena = argv[1];

    systemConfig = getSystemConfig();
    
    systemConfig.cpu_threads = malloc(sizeof(CPU_thread));
    CPU_thread cpu0;
    cpu0.id = 0;
    systemConfig.cpu_threads[0] = cpu0;

    memoria_fisikoa_hasieratu();

    frameak_hasieratu();

    memoria_fisikoa_inprimatu();

    int erloju_tick = 1;
    sched_tick = 3;
    pro_sor_tick = 2;

    pthread_t erloju_haria, tenporizadore_haria, loader_haria, scheduler_haria, cpu_thread;

    printf("CPU_thread-aren haria sortzen... \n"); // CPU_thread bakarrarekin lan egingo dugu
    int err_cpu = pthread_create(&cpu_thread, NULL, cpu_hari, (void *)&systemConfig.cpu_threads[0]);
    if(err_cpu > 0){
        printf("Errorea cpu-aren haria sortzean: \n");
        exit(1);
    }

    printf("Loader-aren haria sortzen... \n");
    int err_pro_sor = pthread_create(&loader_haria, NULL, loader, programa_izena);
    if(err_pro_sor > 0){
        printf("Errorea prozesu-sortzailearen haria sortzean: \n");
        exit(1);
    }

    printf("Tenporizadorearen haria sortzen... \n");
    int err_tenp = pthread_create(&tenporizadore_haria, NULL, tenporizadorea, NULL);
    if(err_tenp > 0){
        printf("Errorea tenporizadorearen haria sortzean: \n");
        exit(1);
    }

    printf("Scheduler-aren haria sortzen... \n"); //fflush(stdout);
    int err_sched = pthread_create(&scheduler_haria, NULL, scheduler, NULL);
    if(err_sched > 0){
        printf("Errorea scheduler-aren haria sortzean: \n");
        exit(1);
    }

    printf("Erlojuaren haria sortzen... \n");
    int err_erloju = pthread_create(&erloju_haria, NULL, (void *)erlojua, &erloju_tick);
    if(err_erloju > 0){
        printf("Errorea erlojuaren haria sortzean: \n");
        exit(1);
    }

    printf("Hari guztiak ondo sortu dira. \n");

    pthread_join(erloju_haria, NULL);
    pthread_join(tenporizadore_haria, NULL);
    pthread_join(loader_haria, NULL);
    pthread_join(scheduler_haria, NULL);
    pthread_join(cpu_thread, NULL);

    memoria_fisikoa_askatu();

    return 0;
}