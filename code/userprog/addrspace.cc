/** Universidad de Costa Rica
 * Escuela de Ciencias de la Computacion e Informatica
 * Sistemas Operativos CI-1310
 * Modificacion a NachOS
 * Kevin Delgado Sandi  B22214
 * Fabian Rodriguez Obando  B25695
 * II Semestre 2014
 */

// addrspace.cc
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
// ejemplo
//	1. link with the -N -T 0 option
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "addrspace.h"
#include "noff.h"

#define CODESEG 0
#define INITDATA 1
#define UNINITDATA 2
#define STACKSEG 3

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void SwapHeader (NoffHeader* noffH) {
    noffH->noffMagic = WordToHost (noffH->noffMagic);
    noffH->code.size = WordToHost (noffH->code.size);
    noffH->code.virtualAddr = WordToHost (noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost (noffH->code.inFileAddr);
    noffH->initData.size = WordToHost (noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost (noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost (noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost (noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost (noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost (noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Load the program from a file "executable", and set everything
//	up so that we can start executing user instructions.
//
//	Assumes that the object code file is in NOFF format.
//
//	First, set up the translation from program memory to physical
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//
//	"executable" is the file containing the object code to load into memory
//----------------------------------------------------------------------
AddrSpace::AddrSpace (OpenFile* executable) {
    NoffHeader noffH;

    executable->ReadAt ( (char*) &noffH, sizeof (noffH), 0);
    if ( (noffH.noffMagic != NOFFMAGIC) &&
         (WordToHost (noffH.noffMagic) == NOFFMAGIC)) {
        SwapHeader (&noffH);
    }
    ASSERT (noffH.noffMagic == NOFFMAGIC);

    // how big is address space?
    unsigned int size = noffH.code.size + noffH.initData.size + noffH.uninitData.size + UserStackSize;

    //llenando el vector que dice cuantas paginas de cada segmento
    vecTamaSegments[CODESEG] = divRoundUp(nuffH.code.size, PageSize);
    vecTamaSegments[INITDATA] = divRoundUp(nuffH.initData.size, PageSize);
    vecTamaSegments[UNINITDATA] = divRoundUp(nuffH.uninitData.size, PageSize);
    vecTamaSegments[STACKSEG] = divRoundUp(UserStackSize, PageSize);

    numPages = divRoundUp (size, PageSize);
    size = numPages * PageSize;

    ASSERT (numPages <= NumPhysPages);		// check we're not trying
    // to run anything too big --
    // at least until we have
    // virtual memory

    DEBUG ('a', "Initializing address space, num pages %d, size %d\n", numPages, size);

    // first, set up the translation
    pageTable = new TranslationEntry[numPages];

    // Se asigna la realción página virtual <--> página física
#ifdef VM
    //como es en memoria virtual, le pone la physicalPage en -1 y lo pone la página como inválida
    for (unsigned int i = 0; i < numPages; ++i) {
        pageTable[i].virtualPage = i; // Las páginas virtuales sí son lineales, el programa cree que los tiene contiguas
        pageTable[i].physicalPage = -1; // la página física la pone en -1
        pageTable[i].valid = false; //pone la página como inválida
        pageTable[i].use = false;
        pageTable[i].dirty = false;
        pageTable[i].readOnly = false;  // if the code segment was entirely on
        // a separate page, we could set its
        // pages to be read-only
    }
#else
    for (unsigned int i = 0; i < numPages; ++i) {
        pageTable[i].virtualPage = i; // Las páginas virtuales sí son lineales, el programa cree que los tiene contiguas
        pageTable[i].physicalPage = MiMapa->Find(); // Buscará páginas disponible en la memoria física
        pageTable[i].valid = true;
        pageTable[i].use = false;
        pageTable[i].dirty = false;
        pageTable[i].readOnly = false;  // if the code segment was entirely on
        // a separate page, we could set its
        // pages to be read-only
    }
    //hola esto es una prueba

    // Asignacion de memoria para el Code Segment
    DEBUG ('a', "Code Segment asignation.\n");
    unsigned int cantPagUsadas_CodeSeg = divRoundUp (noffH.code.size, PageSize);
    if (noffH.code.size > 0) {
        // Tomará las páginas asignadas al ejecutable, obtendrá la dirección física y se copiará el CS
        for (unsigned int i = 0; i < cantPagUsadas_CodeSeg; ++i) {
            int pagina = pageTable[i].physicalPage;
            DEBUG ('a', "Page number: %i, Used bytes: %i noffH.code.InFileAdr %d\n", i, PageSize * i, noffH.code.inFileAddr);
            executable->ReadAt (& (machine->mainMemory[pagina * PageSize]), PageSize, (PageSize * i + noffH.code.inFileAddr));
        }
    }

    // Asignacion de memoria para el Data Segment
    DEBUG ('a', "Used Data Segment asignation.\n");
    unsigned int cantPagUsadas_DataSeg = divRoundUp (noffH.initData.size, PageSize);
    if (noffH.initData.size > 0) {
        // Tomará las páginas asignadas al ejecutable, obtendrá la dirección física y se copiará el DS
        for (unsigned int i = cantPagUsadas_CodeSeg; i < cantPagUsadas_DataSeg; ++i) {
            int pagina = pageTable[i].physicalPage;
            DEBUG ('a', "Page number: %i, Used bytes: %i\n", i, PageSize * i);
            executable->ReadAt (& (machine->mainMemory[pagina * PageSize]), PageSize, (PageSize * i + noffH.initData.inFileAddr));
        }
    }

    // Asignacion de espacio para los datos no inicializados
    DEBUG ('a', "Unitialized Data Segment asignation.\n");
    unsigned int cantPagUsadas_UninitDataSeg = divRoundUp (noffH.uninitData.size, PageSize);
    if (noffH.uninitData.size > 0) {
        for (unsigned int i = cantPagUsadas_DataSeg; i < cantPagUsadas_UninitDataSeg; ++i) {
            int pagina = pageTable[i].physicalPage;
            DEBUG ('a', "Page number: %i, Used byte: %i\n", i, PageSize * i);
            executable->ReadAt (& (machine->mainMemory[pagina * PageSize]), PageSize, (PageSize * i + noffH.uninitData.inFileAddr));
        }
    }
#endif

}

//----------------------------------------------------------------------
// Constructor para uso del fork
// Hace que papa e hijo compartan datos y codigo
// pero le hace una nueva pila al hijo
//----------------------------------------------------------------------
AddrSpace::AddrSpace (AddrSpace* fatherSpace) {
    int tamanioPila = divRoundUp (UserStackSize, PageSize);
    numPages = fatherSpace->numPages;
    ASSERT (numPages <= (unsigned int) MiMapa->NumClear()); // Hay espacio sufieciente?

    pageTable = new TranslationEntry[numPages];
    for (unsigned int i = 0; i < numPages; ++i) {
        pageTable[i].virtualPage = i;	  // Como es virtual es lineal (0,1,2,3...)
        if (i < (numPages - tamanioPila)) {  // Para el code & data segment realiza el if, el espacio del la pila se hará en el else
            pageTable[i].physicalPage = fatherSpace->pageTable[i].physicalPage; // Comparten los mismas páginas
        }
        else {
#ifdef VM
            pageTable[i].physicalPage = -1;
#else
            pageTable[i].physicalPage = MiMapa->Find(); // Busca espacio libre para asignar el espacio de pila
#endif
            bzero (& (machine->mainMemory[pageTable[i].physicalPage * PageSize]), PageSize);
        }
        pageTable[i].valid = true;
        pageTable[i].use = false;
        pageTable[i].dirty = false;
        pageTable[i].readOnly = false;
    }
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.  Nothing for now!
//----------------------------------------------------------------------
AddrSpace::~AddrSpace() {
    delete pageTable;
    delete [] vecTamaSegments;
}

//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------
void AddrSpace::InitRegisters() {
    for (int i = 0; i < NumTotalRegs; i++) {
        machine->WriteRegister (i, 0);
    }

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister (PCReg, 0);

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister (NextPCReg, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we don't
    // accidentally reference off the end!
    machine->WriteRegister (StackReg, numPages * PageSize - 16);
    DEBUG ('a', "Initializing stack register to %d\n", numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, nothing!
//----------------------------------------------------------------------
void AddrSpace::SaveState() {
#ifdef USE_TLB
    for(int i=0; i<TLBSize; ++i){
        if(machine->tlb[i].valid && machine->tlb[i].dirty){ //la pag esta sucia y valida?
            pageTable[machine->tlb[i].virtualPage] = machine->tlb[i];   //la guarda en "cache"
        }
    }
#else
    pageTable = machine->pageTable;
    numPages = machine->pageTableSize;
#endif
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------
void AddrSpace::RestoreState() {
    // "esto debe cambiarse porque sino "tlb" y "pageTable" de "machine"
    // serían ambas distintos de nulo y falla unos de los "ASSERT" de "translate""
#ifdef USE_TLB
    for(int i=0; i<TLBSize; ++i){
        machine->tlb[i].valid = false;
    }
#else
    machine->pageTable = pageTable;
    machine->pageTableSize = numPages;
#endif
}



//----------------------------------------------------------------------
//AddrSpace::load
//      Se encarga de cargar que falta en la TLB
//----------------------------------------------------------------------
void AddrSpace::load(int missingPage)
{
    int posReemplazo = machine->algoritmo_ReemplazoTLB();   //selecciono cual reemplazar en la TLB
    if(pageTable[missingPage].valid == false){  //no esta en la TLB entonces hay que hacer un despilingue
        //lado derecho del arbol del esquema
        if(pageTable[missingPage].dirty){
            //si entra aca, estoy en el SWAP
            int posLibre = MiMapa->find();  //busco a ver si hay frames libres
            if(posLibre != -1){ //si hay uno libre
                //readAt del SWAP para ponerlo en memoria
                //lo cargo en la TLB
            }else{
                //tengo que reemplazar de memoria
                //algoritmo de reemplazo de RAM
                //tengo que ver si lo que voy a quitar es codigo | datos para quitarlo sin problema
                //si es datos sin inicializar | pila entonces tengo que guardarlo en el SWAP
                //lo cargo en la TLB
            }
        }else{
            //estoy en el ejecutable
            int posActual = 0;
            bool estoy = false;
            for(int i=0; i<4 && estoy==false ; ++i){
                if(missingPage >= posActual && missingPage < vecTamaSegments[i]){
                    posActual = i;  //queda guardado en que segmento esta la pagina
                    estoy = true;
                }else{
                    posActual += vecTamaSegments[i];
                }
            }

            switch(posActual){
            case CODESEG:
                //no importa, no me tienen que guardar
                //no hay break xq hace lo mismo que INITDATA
            case INITDATA:
                //busco campo en RAM (MiMapa)
                int posLibre = revisar_RAM();
                //luego leo del ejecutable y lo pongo en la posicion libre
                //actualizo pageTable, actualizo TLB
                break;
            default: //es pila o datos sin inicializar
                //busco campo en RAM (MiMapa)
                int posLibre = revisar_RAM();
                //      pone todo en cero

                break;
            }
        }
    }else{  // la pagina si esta cargada en memoria pero no en la TLB
        if(machine->tlb[posReemplazo].valid){
            //eliminar una entrada del PageTable
            TranslationEntry* tmp = pageTable[machine->tlb[posReemplazo].virtualPage];
            tmp.valid = machine->tlb[posReemplazo].valid;
            tmp.readOnly = machine->tlb[posReemplazo].readOnly;
            tmp.use = machine->tlb[posReemplazo].use;
            tmp.dirty = machine->tlb[posReemplazo].dirty;
        }

        machine->tlb[posReemplazo].virtualPage = pageTable[posReemplazo].virtualPage;
        machine->tlb[posReemplazo].physicalPage = pageTable[posReemplazo].physicalPage;
        machine->tlb[posReemplazo].valid = true;
        machine->tlb[posReemplazo].readOnly = pageTable[posReemplazo].readOnly;
        machine->tlb[posReemplazo].use = pageTable[posReemplazo].use;
        machine->tlb[posReemplazo].dirty = pageTable[posReemplazo].dirty;

    }
}


int AddrSpace::revisar_RAM()
{
    int posLibre = MiMapa->find();
    if(posLibre == -1){
        posLibre = rand()%MemorySize;
    }

    return posLibre;
}
