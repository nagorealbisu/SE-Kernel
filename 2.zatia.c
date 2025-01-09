#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <string.h>

#define PROZ_KOP_MAX 10
#define QUANTUM 2

#define FCFS 1
#define RR 2
#define RussianRoulette 3

#define READY 0
#define RUNNING 1
#define FINISHED 2


int done = 0;
int clock_tick = 0;

void *cpu_haria(void *param);
void *erlojua(void *param);
void *tenporizadorea();
void *prozesu_sortzailea(void *proz_kop);
void *scheduler();
void *cpu(void *param);
void print_pcb(void *param);
void prozesua(void *param);


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_sched = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_prozesuak = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_prozesua = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_prozesuen_ilara = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond_clock_tick = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_scheduler = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_prozesu_sortzailea = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_prozesua = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_cpu = PTHREAD_COND_INITIALIZER;


typedef struct pcb{
    int pid;
    int egoera;      //READY, RUNNING, FINISHED
    int ticks_left;
    int tick_kop;
    int quantum_left;
} pcb;

typedef struct CPU{
    int id;
    pcb uneko_pcb;
} CPU;

typedef struct{
    int num_cpus;
    int num_cores;
    int num_hariak;
    CPU *cpus;
} makina;

makina systemConfig;

struct pcb ready_ilara[PROZ_KOP_MAX];
int prozesu_kop = 0;

int sched_tick = 1;
int pro_sor_tick = 1;
int quantum;// = 0;

makina getSystemConfig() {
    makina sys_config;

    sys_config.num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    sys_config.num_cores = sys_config.num_cpus;
    sys_config.num_hariak = sys_config.num_cpus;

    printf("\n--- Sistemaren konfigurazioa ---\n");
    printf("CPU kopurua: %d\n", sys_config.num_cpus);
    //printf("Core kopurua: %d\n", sys_config.num_cores);
    //printf("Hari kopurua: %d\n", sys_config.num_hariak);
    printf("----------------------------------\n");

    return sys_config;
}

void *cpu(void *param){
        struct CPU *cpu = (struct CPU *)param;
        while(1){
            pthread_cond_wait(&cond_cpu, &mutex_prozesua);
            //pthread_mutex_lock(&mutex_prozesua);
                printf("\nCPU: PID = %d duen prozesua exekutatzen...\n", cpu->uneko_pcb.pid);
                prozesua(&(cpu->uneko_pcb)); // ez da konkurrentea
            pthread_cond_signal(&cond_prozesua);
            //pthread_mutex_unlock(&mutex_prozesua);
        }
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
            printf("Prozesu-sortzailearen tenporizadorea hasi da %d. tick-ean. \n", clock_tick);
            pthread_cond_signal(&cond_prozesu_sortzailea);
        }
        if(clock_tick%sched_tick == 0){
            printf("Scheduler-aren tenporizadorea hasi da %d. tick-ean. \n", clock_tick);
            pthread_cond_signal(&cond_scheduler);
        }
        pthread_mutex_unlock(&mutex);
    }
}

void *prozesu_sortzailea(void *proz_kop){
    int *kop = (int *)proz_kop;
    int errorea;
    pcb p;
    for(int i=0; i<(*kop); i++){
        pthread_cond_wait(&cond_prozesu_sortzailea, &mutex_prozesuak);
        printf("Prozesu Sortzailea: Prozesua sortzen... PID = %d\n", i);

        p.egoera = READY;
        p.pid = i;
        p.tick_kop = rand() % 5 + 1;
        p.ticks_left = p.tick_kop;
        p.quantum_left = quantum;
        pthread_mutex_lock(&mutex_prozesuen_ilara);
            ready_ilara[i] = p;
            prozesu_kop++;
        pthread_mutex_unlock(&mutex_prozesuen_ilara);
        print_pcb(&p);
    }
    return NULL;
}


void *scheduler(void *param) {
    int *politika = (int *)param;
    pthread_t p;
    printf("Scheduler: POLITIKA = %d\n", *politika);
    if(*politika == FCFS){
        while (1) {
            printf("\nFUCK\n"); fflush(stdout);
            pthread_cond_wait(&cond_scheduler, &mutex_sched);

            pthread_mutex_lock(&mutex_prozesuen_ilara);
            if (prozesu_kop > 0) {
                pthread_mutex_lock(&mutex_prozesua);
                pthread_cond_signal(&cond_cpu);

                    printf("Scheduler FIFO: Prozesua CPU-ari pasatzen... PID = %d\n", ready_ilara[0].pid);
                
                    systemConfig.cpus[0].uneko_pcb = ready_ilara[0];

                pthread_cond_wait(&cond_prozesua, &mutex_prozesua);
                pthread_mutex_unlock(&mutex_prozesua);
                ready_ilara[0].ticks_left--;

                if(ready_ilara[0].ticks_left == 0){ // amaitu da
                    ready_ilara[0].egoera = FINISHED;
                    for (int i = 0; i < prozesu_kop - 1; i++) {
                        //mutex
                        ready_ilara[i] = ready_ilara[i + 1];
                    }
                    //ready_ilara[prozesu_kop-1] = NULL;
                    prozesu_kop--;
                }
            } else {
                printf("Scheduler: Ilara hutsik dago.\n");
            }
            pthread_mutex_unlock(&mutex_prozesuen_ilara);
        }

    }else if(*politika == RR){
        quantum = 2;
        while (1) {
            pthread_cond_wait(&cond_scheduler, &mutex_sched);
            pthread_mutex_lock(&mutex_prozesuen_ilara);
            if (prozesu_kop > 0) {
                pthread_mutex_lock(&mutex_prozesua);
                pthread_cond_signal(&cond_cpu);
                    
                    printf("Scheduler RR: Prozesua CPU-ari pasatzen... PID = %d\n", ready_ilara[0].pid);
                    systemConfig.cpus[0].uneko_pcb = ready_ilara[0];

                pthread_cond_wait(&cond_prozesua, &mutex_prozesua);
                pthread_mutex_unlock(&mutex_prozesua);
                ready_ilara[0].ticks_left--;
                ready_ilara[0].quantum_left--;
                if(ready_ilara[0].ticks_left == 0){
                    ready_ilara[0].egoera = FINISHED;
                    ready_ilara[0].ticks_left = ready_ilara[0].tick_kop;
                    printf("Scheduler: %d prozesua amaitu da, FINISHED\n", ready_ilara[0].pid);
                }
                // edo --> for(int j=0; j<prozesu_ilara[0].quantum; j++) prozesua_egoerak(&prozesu_ilara[0]);
                if((ready_ilara[0].egoera == FINISHED) || (ready_ilara[0].quantum_left == 0)){ // amaitu du
                    struct pcb azkena = ready_ilara[0];
                    for (int i = 0; i < prozesu_kop - 1; i++) {
                        //mutex
                        ready_ilara[i] = ready_ilara[i + 1];
                    }
                    // hasieratu balio guztiak
                    azkena.egoera = READY;
                    azkena.quantum_left = quantum;
                    //azkena.ticks_left = azkena.tick_kop;
                    ready_ilara[prozesu_kop-1] = azkena;
                }
            } else {
                printf("Scheduler: Ilara hutsik dago.\n");
            }
            pthread_mutex_unlock(&mutex_prozesuen_ilara);
        }
    }else if(*politika == RussianRoulette){
        while(1){
            pthread_cond_wait(&cond_scheduler, &mutex_sched);
            pthread_mutex_lock(&mutex_prozesuen_ilara);
            if (prozesu_kop > 0) {
                
                int bala = rand() % 6;

                pthread_mutex_lock(&mutex_prozesua);
                pthread_cond_signal(&cond_cpu);
                    
                    if(ready_ilara[0].egoera != FINISHED){
                        printf("Scheduler Русская Рулетка: Prozesua CPU-ari pasatzen... PID = %d\n", ready_ilara[0].pid);
                        systemConfig.cpus[0].uneko_pcb = ready_ilara[0];
                    }
                
                pthread_cond_wait(&cond_prozesua, &mutex_prozesua);
                pthread_mutex_unlock(&mutex_prozesua);

                if(bala == 1) printf("\n\t\033[31mPUM\033[0m\n\n"); // akatuta
                
                ready_ilara[0].ticks_left--;
                if(ready_ilara[0].ticks_left == 0 || bala == 1){ // amaitu du edo hil egin da
                    ready_ilara[0].egoera = FINISHED;
                    ready_ilara[0].ticks_left = ready_ilara[0].tick_kop;
                    printf("Scheduler: Процесс %d закончен, (FINISHED)\n", ready_ilara[0].pid);
                }
                if(ready_ilara[0].egoera == FINISHED){
                    for (int i = 0; i < prozesu_kop - 1; i++) {
                        //mutex
                        ready_ilara[i] = ready_ilara[i + 1];
                    }
                    prozesu_kop--;
                }
            } else {
                printf("Scheduler: Cписок пуст.\n");
            }
            pthread_mutex_unlock(&mutex_prozesuen_ilara);
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


int main(int argc, char *argv[]){ // argv[1] proz_kop

    if(argc != 3){
        printf("Erabilpena: ./programaren_izena <prozesu_kopurua> <politika>");
        printf("\nPolitika aukerak: ");
        printf("\nFCFS = 1");
        printf("\nRR = 2");
        printf("\nRR (Russian Roulette) = 3\n");
        exit(1);
    }
    char* kop;
    int proz_kop = strtol(argv[1], &kop, 10);

    systemConfig = getSystemConfig();
    /*if(systemConfig.num_hariak > 4){
        printf("Hari kopurua nahikoa da programa exekutatzeko.\n");
        systemConfig.cpus = malloc(sizeof(CPU)*(systemConfig.num_cpus - 4));
        for(int i=0; i<(systemConfig.num_cpus - 4); i++){
            CPU cpu;
            cpu.id = i;
            systemConfig.cpus[i] = cpu;
        }
    }else{
        printf("Makinaren hari kopuruak ez dira nahikoak kernel birtuala exekutatzeko.\n");
        printf("Ateratzen...");
        exit(1);
    }*/

    systemConfig.cpus = malloc(sizeof(CPU));
    CPU cpu0;
    cpu0.id = 0;
    systemConfig.cpus[0] = cpu0;
    

    int erloju_tick = 1;
    sched_tick = 3;
    pro_sor_tick = 2;

    char* string2;
    int politika = strtol(argv[2], &string2, 10);
    if(1 > politika || politika > 3){
        printf("Politika ez da baliozkoa.\n");
        printf("\nPolitika aukerak: ");
        printf("\nFCFS = 1");
        printf("\nRR = 2");
        printf("\nRR (Russian Roulette) = 3\n");
    }
    printf("\nAukeratutako politika: %d", politika);

    if(politika == RR) quantum = 2; else quantum = 0;

    pthread_t erloju_haria, tenporizadore_haria, prozesu_sortzaileen_haria, scheduler_haria, cpu_haria;

    printf("CPU-aren haria sortzen... \n"); // CPU bakarrarekin lan egingo dugu nahiz eta gehiago aduki
    int err_cpu = pthread_create(&cpu_haria, NULL, cpu, (void *)&systemConfig.cpus[0]);
    if(err_cpu > 0){
        printf("Errorea cpu-aren haria sortzean: \n");
        exit(1);
    }

    printf("Prozesu sortzailearen haria sortzen... \n");
    int err_pro_sor = pthread_create(&prozesu_sortzaileen_haria, NULL, prozesu_sortzailea, (void *)&proz_kop);
    //int err_pro_sor = pthread_create(&scheduler_haria, NULL, scheduler, NULL);
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

    printf("Scheduler-aren haria sortzen... \n");
    int err_sched = pthread_create(&scheduler_haria, NULL, scheduler, &politika);
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
    pthread_join(prozesu_sortzaileen_haria, NULL);
    pthread_join(scheduler_haria, NULL);
    pthread_join(cpu_haria, NULL);
}