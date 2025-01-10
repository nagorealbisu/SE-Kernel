#ifndef MEMORY_H
#define MEMORY_H

#include "defines.h"

void memoria_fisikoa_hasieratu();
void memoria_fisikoa_askatu();
void memoria_fisikoa_inprimatu();
int memoria_fisikoa_irakurri(int helbide_fisikoa);
int memoria_birtuala_irakurri(struct CPU_thread *cpu, int helbide_birtuala);
void memoria_birtualean_idatzi(struct CPU_thread *cpu, int helbide_birtuala, int balioa);
void orriak_hasieratu(pcb *p, int orri_kop);
void frameak_hasieratu();
int frame_librea_lortu();
void framea_askatu(int frame);
void mmu_hasieratu(struct CPU_thread *cpu);
int mmu_helbidea_itzuli(void *cpu, int direccion_logica);
void memoria_birtualean_idatzi(struct CPU_thread *cpu, int helbide_birtuala, int balioa);

#endif