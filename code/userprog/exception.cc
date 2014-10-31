/** Universidad de Costa Rica
 * Escuela de Ciencias de la Computacion e Informatica
 * Sistemas Operativos CI-1310
 * Modificacion a NachOS
 * Fabian Rodriguez Obando  B25695
 * Kevin Delgado Sandi      B22214
 * II Semestre 2014
 */

// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.
#define ERROR -1
#define TAMBUFFER 1024
#define VACIO 0

#include "copyright.h"
#include "system.h"
#include "syscall.h"

//Para usar los semaforos de threads
#include "synch.h"

//Para usar el Open y el Write de UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions
//	are in machine.h.
//----------------------------------------------------------------------

char buffer[TAMBUFFER];   //se va a usar como buffer
Semaphore semMutexAux("Semamoro para bloquear secciones criticas", 1);

void returnFromSystemCall() {

	int pc, npc;

	pc = machine->ReadRegister(PCReg);
	npc = machine->ReadRegister(NextPCReg);
	machine->WriteRegister(PrevPCReg, pc);          // PrevPC <- PC
	machine->WriteRegister(PCReg, npc);             // PC <- NextPC
	machine->WriteRegister(NextPCReg, npc + 4);     // NextPC <- NextPC + 4

} // returnFromSystemCall

void Nachos_Halt() {        //System call #0
	DEBUG('a', "Shutdown, initiated by user program.\n");
	interrupt->Halt();
}//Nachos_Halt

void Copy_Mem(int registro) {   //Pasa lo que hay en el registro a un String
	int carac = 1;
	int i;
	int direccion = machine->ReadRegister(registro);
	for(i = 0; carac != '\0'; ++i) {
		machine->ReadMem(direccion + i, 1, &carac);
		buffer[i] = (char)carac;
	}
	buffer[++i] = '\0';
}

void Nachos_Exit() {    //System call # 1

	DEBUG('a', "Entering Exit");
	int idProc = machine->ReadRegister(4);
	currentThread->Yield();
	currentThread->tablaProcesos->delThread();
	if(VACIO == currentThread->tablaProcesos->getUsage()) {
		Nachos_Halt();  //soy el ultimo, apague y vamonos
	}
	if(NULL != currentThread->space) {
		delete currentThread->space;
		currentThread->space = NULL;
	}

	//tengo que liberar los semaforos que yo como proceso habia creado
	while(VACIO != currentThread->tablaSemaforos->getUsage()) {
		Semaphore* semaforo = (Semaphore*)currentThread->tablaSemaforos->getUnixHandle(idProc);
		currentThread->tablaSemaforos->delThread();
		semaforo->V();  //lo libero
	}

	currentThread->Finish();    //termina el proceso
	returnFromSystemCall();
	DEBUG('a', "Exiting Exit");

}//Nachos_Exit

void Nachos_Exec() {    //System call # 2

}//Nachos_Exec

void Nachos_Join() {    //System call # 3

	SpaceId sID = machine->ReadRegister(4);
	DEBUG('a', "Entering System Call Join.\n");
	Semaphore* semaforo = new Semaphore("Semaforo join", 0);
	currentThread->tablaSemaforos->semOpen(sID, (long)semaforo);
	currentThread->tablaSemaforos->addThread();
	if(currentThread->tablaProcesos->getUnixHandle(sID) == ERROR) {
		machine->WriteRegister(2, ERROR);
	}
	else {
		machine->WriteRegister(2, VACIO);
	}
	semaforo->P();
	returnFromSystemCall();
	DEBUG('a', "Exitin System Call Join.\n");

}//Nachos_Join

void Nachos_Create() {  //System call # 4
	DEBUG('a', "Entering Create.\n");
	Copy_Mem(4);
	int idFileUnix = creat(buffer, S_IRWXU | S_IRWXO | S_IRWXG);
	//S_IRWXU->permiso (al dueño) de lectura, ejecucion y escritura
	//S_IRWXO->otros tienen permiso de escritura, lectura y ejecucion
	//S_IRWXG->el grupo tiene permiso de escritura, lectura y ejecucion

	DEBUG('a', "File created on UNIX with id: %d\n", idFileUnix);
	int idFileNachos = currentThread->tablaFiles->Open(idFileUnix);
	currentThread->tablaFiles->addThread();
	DEBUG('a', "File created on Nachos with id: %d\n", idFileNachos);
	machine->WriteRegister(2, idFileNachos);
	returnFromSystemCall();
	DEBUG('a', "Exiting Create.\n");

}//Nachos_Create

void Nachos_Open() {    // System call # 5

	DEBUG('a', "Entering Open.\n");
	int rutaVirtual = machine->ReadRegister(4);
	int rutaFisica;
	char bufferNombre[TAMBUFFER];
	int i = 0;
	bool seguir = true;
	do {
		machine->ReadMem(rutaVirtual, 1, &rutaFisica);
		if((char)rutaFisica != '\0') {
			bufferNombre[i] = (char)rutaFisica;
			++rutaVirtual;
			++i;
		}
		else {
			bufferNombre[i] = (char)rutaFisica;
			seguir = false;
		}
	}
	while(seguir);

	printf("File name %s opened.\n", bufferNombre);

	OpenFileId idFileUnix = open(bufferNombre, O_RDWR);

	if(idFileUnix != ERROR) {
		int idFileNachos = currentThread->tablaFiles->Open(idFileUnix);
		machine->WriteRegister(2, idFileNachos);
	}
	else {
		printf("ERROR: File doesn't exist.\n");
		machine->WriteRegister(2, ERROR);
	}
	returnFromSystemCall();
	DEBUG('a', "Exiting Open.\n");
}// Nachos_Open

void Nachos_Read() {    //System call # 6

	DEBUG('a', "Entering Nachos_Read.\n");
	int queLeer = machine->ReadRegister(4);         //lo que hay que leer
	int tama = machine->ReadRegister(5);            //tamano de lo que voy a leer
	OpenFileId idFileNachOS = machine->ReadRegister(6);   //le indica si lee de consola o de un archivo

	int cant = 0;
	semMutexAux.P();   //Solo yo puedo leer
	switch(idFileNachOS) {
		//lecturas de consola
		case ConsoleOutput:
			machine->WriteRegister(2, ERROR);
			printf("ERROR: User can't read from console output.\n");
			break;
		case ConsoleInput:
			cant = read(1, buffer, tama);   //lee con el read de UNIX
			machine->WriteRegister(2, cant);
			stats->numConsoleCharsRead += cant;
			break;
		case ConsoleError:
			printf("ERROR: Console error %d.\n", queLeer);
			break;
		default:
			//lectura de archivos
			if(currentThread->tablaFiles->isOpened(idFileNachOS)) { //esta abierto?
				DEBUG('n', "Entro a read de UNIX.\n");
				int idUnix = currentThread->tablaFiles->getUnixHandle(idFileNachOS);
				cant = read(idUnix, buffer, tama);    //lee con el read de UNIX
				DEBUG('n', "Sale del read de UNIX.\n");
				machine->WriteRegister(2, cant);
				stats->numDiskReads++;
			}
			else {
				machine->WriteRegister(2, ERROR);   //se quiere leer de un archivo que no esta abierto
				printf("ERROR: File doesn't exist.\n");
			}
	}
	semMutexAux.V();
	returnFromSystemCall();
	DEBUG('a', "Exiting Nachos_Read.\n");
}//Nachos_Read

void Nachos_Write() {   // System call 7
	DEBUG('a', "System call Write.\n");
	int size = machine->ReadRegister(5);	// Read size to write
	OpenFileId idFileNachOS = machine->ReadRegister(6);	// Read file descriptor
	int cantCaracteres = 0;

	Copy_Mem(4);
	semMutexAux.P();   //solo yo puedo escribir
	switch(idFileNachOS) {
		case  ConsoleInput:	// User could not write to standard input
			machine->WriteRegister(2, ERROR);
			break;
		case  ConsoleOutput:
			buffer[ size ] = VACIO;
			stats->numConsoleCharsWritten += size;
			printf("%s \n", buffer);
			break;
		case ConsoleError: // This trick permits to write integers to console
			printf("%d\n", machine->ReadRegister(4));
			break;
		default:
			if(currentThread->tablaFiles->isOpened(idFileNachOS)) {
				DEBUG('a', "Entro a write de UNIX.\n");
				write(currentThread->tablaFiles->getUnixHandle(idFileNachOS), buffer, size);
				DEBUG('a', "Salgo del write de UNIX.\n");
				machine->WriteRegister(2, cantCaracteres);
				stats->numDiskWrites++;
			}
			else {
				machine->WriteRegister(2, ERROR);
			}
	}
	semMutexAux.V();
	returnFromSystemCall(); // Update the PC registers
	DEBUG('a', "Exiting Write System Call.\n");

} // Nachos_Write

void Nachos_Close() {       //System call # 8

	DEBUG('a', "Entering Close System Call.\n");
	int idHilo = machine->ReadRegister(6);
	if(currentThread->tablaFiles->isOpened(idHilo)) {
		int idUnix = currentThread->tablaFiles->getUnixHandle(idHilo);
		int tmp = currentThread->tablaFiles->Close(idHilo);
		tmp = close(idUnix);
		printf("File closed.\n");
		machine->WriteRegister(2, tmp);
	}
	else {
		printf("ERROR: Atempt to close a file that isn't opened.\n");
		machine->WriteRegister(2, ERROR);
	}
	returnFromSystemCall();
	DEBUG('a', "Exiting Close System Call.\n");

}//Nachos_Close

void AyudanteFork(void* vo) {      //Metodo auxiliar que usa Fork

	int* p = (int*)vo;
	AddrSpace* space;
	space = currentThread->space;
	space->InitRegisters();
	space->RestoreState();

	machine->WriteRegister(RetAddrReg, 4);
	machine->WriteRegister(PCReg, *p);
	machine->WriteRegister(NextPCReg, *p + 4);
	machine->Run();                     // jump to the user progam
	ASSERT(false);

}

void Nachos_Fork() {    //System call # 9

	DEBUG('a', "Entering Fork System call\n");

	Thread* nuevoHilo = new Thread("Hijo");     //proceso hijo
	nuevoHilo->tablaFiles = currentThread->tablaFiles;  //NachosOpenFilesTable tiene sobrecargado el operador = entonces no hay bronca :D
	int idHilo = currentThread->tablaProcesos->Open((long)nuevoHilo);    //agrega el hilo nuevo a la tabla de procesos
	currentThread->tablaFiles->addThread();
	currentThread->tablaProcesos->addThread();
	nuevoHilo->space = new AddrSpace(currentThread->space);
	DEBUG('a', "Calls ForkHelper.\n");
	nuevoHilo->Fork(AyudanteFork, (void*)machine->ReadRegister(4));

	returnFromSystemCall();

	DEBUG('a', "Exiting Fork System call\n");

}//Kernel_Fork

void Nachos_Yield() {               //System call # 10

	DEBUG('a', "Yield, returns control to the CPU.\n");
	currentThread->Yield();
	returnFromSystemCall();
	DEBUG('a', "Exiting Yield.\n");

}//Nachos_Yield

void Nachos_SemCreate() {   //System call # 11

	DEBUG('a', "Entering SemCreate.\n");

	int valSem = machine->ReadRegister(4);      //lee del registro de MIPS
	Semaphore* semaforo = new Semaphore("Semaforo nuevo", valSem);   //crea el semaforo de NachOS
	int idSem = currentThread->tablaFiles->Open((long)semaforo);
	currentThread->tablaFiles->addThread();

	machine->WriteRegister(2, idSem);      //retorna el el id del semaforo creado como parametro
	returnFromSystemCall();
	DEBUG('a', "Exiting SemCreate. \n");

}//Nachos_SemCreate

void Nachos_SemDestroy() {  //System call # 12

	DEBUG('a', "Entering SemDestroy.\n");
	int idSem = machine->ReadRegister(4);      //lee el parametro
	Semaphore* semaforo = (Semaphore*)currentThread->tablaFiles->getUnixHandle(idSem);
	currentThread->tablaFiles->Close(idSem);
	currentThread->tablaFiles->delThread();
	semaforo->Destroy();
	returnFromSystemCall();
	DEBUG('a', "Exiting SemDestroy.\n");

}//Nachos_SemDestroy

//Usa la clase Semaphore
//llama a V() de esa clase
void Nachos_SemSignal() {   //System call # 13

	DEBUG('a', "Entering SemSignal.\n");
	int idSem = machine->ReadRegister(4);
	Semaphore* semaforo = (Semaphore*)currentThread->tablaFiles->getUnixHandle(idSem);
	semaforo->V();
	returnFromSystemCall();
	DEBUG('a', "Exiting SemSignal.\n");

}//Nachos_SemSignal

//Analogo a signal
//pero usa P()
void Nachos_SemWait() {     //System call # 14

	DEBUG('a', "Entering SemWait.\n");
	int idSem = machine->ReadRegister(4);
	Semaphore* semaforo = (Semaphore*)currentThread->tablaFiles->getUnixHandle(idSem);
	semaforo->P();
	returnFromSystemCall();
	DEBUG('a', "Exiting SemWait.\n");

}//Nachos_SemWait

void ExceptionHandler(ExceptionType which) {

	int type = machine->ReadRegister(2);
	DEBUG('a', "Entre en exception handler con type %d y ExceptionType %d\n", type, which);

	switch(which) {

		case SyscallException:
			switch(type) {
				case SC_Halt:
					Nachos_Halt();              // System call # 0
					returnFromSystemCall();
					break;
				case SC_Exit:
					Nachos_Exit();              //System call # 1
					break;
				case SC_Exec:
					//Nachos_Exec();
					printf("No se ha implementado Exec\n");             //** FALTA **
					ASSERT(false);
					break;
				case SC_Join:
					Nachos_Join();              //System call # 3
					break;
				case SC_Create:
					Nachos_Create();            //System call # 4
					break;
				case SC_Open:
					Nachos_Open();             // System call # 5
					break;
				case SC_Read:
					Nachos_Read();              //System call # 6
					break;
				case SC_Write:
					Nachos_Write();             // System call # 7
					break;
				case SC_Close:
					Nachos_Close();             //System call # 8
					break;
				case SC_Fork:                   //System call # 9
					Nachos_Fork();
					break;
				case SC_Yield:                  //System call # 10
					Nachos_Yield();
					break;
				case SC_SemCreate:              //System call # 11
					Nachos_SemCreate();
					break;
				case SC_SemDestroy:             //System call # 12
					Nachos_SemDestroy();
					break;
				case SC_SemSignal:              //System call # 13
					Nachos_SemSignal();
					break;
				case SC_SemWait:                //System call # 14
					Nachos_SemWait();
					break;
				default:
					printf("Second switch-> unexpected exception %d\n", which);
					ASSERT(false);
					break;
			}
			break;
		default:
			printf("First switch -> unexpected exception %d\n", which);
			ASSERT(false);
			break;
	}
}
