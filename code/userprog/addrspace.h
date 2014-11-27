/** Universidad de Costa Rica
 * Escuela de Ciencias de la Computacion e Informatica
 * Sistemas Operativos CI-1310
 * Modificacion a NachOS
 * Kevin Delgado Sandi  B22214
 * Fabian Rodriguez Obando  B25695
 * II Semestre 2014
 */

// addrspace.h
//	Data structures to keep track of executing user programs
//	(address spaces).
//
//	For now, we don't keep any information about address spaces.
//	The user level CPU state is saved and restored in the thread
//	executing the user program (see thread.h).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef ADDRSPACE_H
#define ADDRSPACE_H

#include "copyright.h"
#include "filesys.h"

#define UserStackSize		1024 	// increase this as necessary!

class AddrSpace {
	public:

		AddrSpace (OpenFile* executable);	// Create an address space
		AddrSpace (AddrSpace* fatherSpace);	//para uso del Fork
		// initializing it with the program
		// stored in the file "executable"
		~AddrSpace();			// De-allocate an address space

		void InitRegisters();		// Initialize user-level CPU registers,
		// before jumping to user code

		void SaveState();			// Save/restore address space-specific
		void RestoreState();		// info on a context switch

        /**
         * @brief load
         * @param missingPage
         */
        void load(int missingPage); //va a cargar la pagina que falta en memoria (si no esta en el TLB)

	private:
        TranslationEntry* pageTable;

        /**
         * @brief revisar_RAM se encarga de revisar si hay campo disponible en memoria, si no encuentra entonces saca a alguien para ocupar su campo
         * @return direccion del campo libre en memoria.
         */
        int revisar_RAM(); //va a buscar campo disponible en memoria, si no encuentra entonces llama al algoritmo_reemplazoRAM

		unsigned int numPages;		// Number of pages in the virtual
									// address space
        int vecTamaSegments[] = new int[4];
};

#endif // ADDRSPACE_H
