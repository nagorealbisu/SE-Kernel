#ifndef DEFINES_H
#define DEFINES_H

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
#define ERREG_KOP 16
#define MEM_FISK_BIT_KOP 8*HITZ*pow(2, 24)/256
#define FRAME_TAMAINA 64  // Frame-tamaina = orri-tamaina
#define KERNEL_HASIERA_HELB "0001C0" //kernel-aren hasiera helbidea
#define PAGE_TABLE_HELB "000200" //prozesuen orri-taulak helbide hontatik aurrera gordeko dira.

extern pthread_mutex_t mutex;
extern pthread_mutex_t mutex_sched;
extern pthread_mutex_t mutex_prozesuak;
extern pthread_mutex_t mutex_prozesua;

extern pthread_cond_t cond_clock_tick;
extern pthread_cond_t cond_scheduler;
extern pthread_cond_t cond_loader;
extern pthread_cond_t cond_prozesua;
extern pthread_cond_t cond_cpu;

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

typedef struct memoria_fisikoa {
    unsigned char *memoria;
    int tamaina;
} memoria_fisikoa;

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

typedef struct CPU_thread{
    int id;
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

extern memoria_fisikoa mem_fisikoa;

#endif