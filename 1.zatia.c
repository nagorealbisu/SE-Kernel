#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define PROZ_KOP_MAX 10

int done = 0;
int clock_tick = 0;

void *prozesu_haria(void *pid);
void prozesua(int pid);
void *erlojua(void *param);
void *tenporizadorea();
void *prozesu_sortzailea(void *proz_kop);
void *scheduler();


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_sched = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_prozesuak = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_prozesuen_ilara = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t cond_clock_tick = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_scheduler = PTHREAD_COND_INITIALIZER;
pthread_cond_t cond_prozesu_sortzailea = PTHREAD_COND_INITIALIZER;


typedef struct Prozesua{
    int pid;
} Prozesua;

struct Prozesua prozesu_ilara[PROZ_KOP_MAX];
int prozesu_kop = 0;

int sched_tick = 1;
int pro_sor_tick = 1;

typedef struct CPU{
    int id;
    Prozesua uneko_pcb;
} CPU;

typedef struct{
    int num_cpus;
    int num_cores;
    int num_hariak;
    CPU *cpus;
} makina;

makina systemConfig;

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

void prozesua(int pid){
    printf("Prozesua naiz. PID = %d \n", pid);
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
    for(int i=0; i<(*kop); i++){
        pthread_cond_wait(&cond_prozesu_sortzailea, &mutex_prozesuak);
        printf("Prozesu Sortzailea: Prozesua sortzen... PID = %d\n", i);

        pthread_mutex_lock(&mutex_prozesuen_ilara);
            prozesu_ilara[i].pid = i;
            prozesu_ilara[prozesu_kop].pid = i;
            prozesu_kop++;
        pthread_mutex_unlock(&mutex_prozesuen_ilara);
    }
    return NULL;
}

void *scheduler(void *param) {
    while (1) {
        pthread_cond_wait(&cond_scheduler, &mutex_sched);

        pthread_mutex_lock(&mutex_prozesuen_ilara);
        if (prozesu_kop > 0) {
            printf("Scheduler: Prozesua exekutatzen. PID = %d\n", prozesu_ilara[0].pid);
            prozesua(prozesu_ilara[0].pid);
            for (int i = 0; i < prozesu_kop - 1; i++) {
                prozesu_ilara[i] = prozesu_ilara[i + 1];
            }
            prozesu_kop--;
        } else {
            printf("Scheduler: Ilara hutsik dago.\n");
        }
        pthread_mutex_unlock(&mutex_prozesuen_ilara);
    }
    return NULL;
}


int main(int argc, char *argv[]){ // argv[1] proz_kop

    if(argc != 2){
        printf("Erabilpena: ./programaren_izena <prozesu_kopurua>");
        exit(1);
    }
    char* kop;
    int proz_kop = strtol(argv[1], &kop, 10);

    int erloju_tick = 1;
    sched_tick = 3;
    pro_sor_tick = 2;

    systemConfig = getSystemConfig();

    pthread_t erloju_haria, tenporizadore_haria, prozesu_sortzaileen_haria, scheduler_haria;

    printf("Scheduler-aren haria sortzen... \n");
    int err_sched = pthread_create(&scheduler_haria, NULL, scheduler, NULL);
    if(err_sched > 0){
        printf("Errorea scheduler-aren haria sortzean: \n");
        exit(1);
    }

    printf("Prozesu sortzailearen haria sortzen... \n");
    pthread_create(&prozesu_sortzaileen_haria, NULL, prozesu_sortzailea, (void *)&proz_kop);
    int err_pro_sor = pthread_create(&scheduler_haria, NULL, scheduler, NULL);
    if(err_pro_sor > 0){
        printf("Errorea prozesu-sortzailearen haria sortzean: \n");
        exit(1);
    }

    printf("Tenporizadorearen haria sortzen... \n");
    pthread_create(&tenporizadore_haria, NULL, tenporizadorea, NULL);
    int err_tenp = pthread_create(&scheduler_haria, NULL, scheduler, NULL);
    if(err_tenp > 0){
        printf("Errorea tenporizadorearen haria sortzean: \n");
        exit(1);
    }

    printf("Erlojuaren haria sortzen... \n");
    int err_erloju = pthread_create(&erloju_haria, NULL, (void *)erlojua, &erloju_tick);
    if(err_erloju > 0){
        printf("Errorea erlojuaren haria sortzean: \n");
        exit(1);
    }

    printf("Hari guztiak sortu dira. \n");

    pthread_join(erloju_haria, NULL);
    pthread_join(tenporizadore_haria, NULL);
    pthread_join(prozesu_sortzaileen_haria, NULL);
}