#include "defines.h"
#include "cpu.h"
#include "memoria.h"

void *erlojua(void *param);
void *tenporizadorea();
void *prozesu_sortzailea(void *proz_kop);
void *scheduler();
void print_pcb(void *param);
void programaren_informazioa_inprimatu(Programa *programa);

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_sched = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_prozesuak = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_prozesua = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond_clock_tick = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_scheduler = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_loader = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_prozesua = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_cpu = PTHREAD_COND_INITIALIZER;

memoria_fisikoa mem_fisikoa; //memoria fisikoaren aldagaia

Programa program;

makina systemConfig;

struct pcb ready_ilara[PROZ_KOP_MAX];
int prozesu_kop = 0;

int done = 0;
int clock_tick = 0;
int sched_tick = 1;
int pro_sor_tick = 1;
int quantum;// = 0;

int frame_libreak[FRAME_TAMAINA]; // frame-entzako bit-map-a

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
        } else {
            printf("Errorea: orri honek: %d, ez ditu frame-rik esleituta.\n", page);
        }
    }
    // Datuak
    for (int i = 0; i < data_size; i++) {
        int page = (prozesua->mm.data / HITZ) + i;
        if (prozesua->orri_taula[page].valid) {
            int frame = prozesua->orri_taula[page].frame_zenb;
            int offset = frame * HITZ;

            for (int j = 0; j < HITZ; j++) {
                mem_fisikoa.memoria[offset + j] = ((unsigned char *)&data[i])[j];
            }
        } else {
            printf("Errorea: orri honek: %d, ez ditu frame-rik esleituta.\n", page);
        }
    }
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