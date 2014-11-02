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
Semaphore semMutexAux ("Semamoro para bloquear secciones criticas", 1);

// Métodos Auxiliares

void returnFromSystemCall() {
	int pc = machine->ReadRegister (PCReg);
	int npc = machine->ReadRegister (NextPCReg);
	machine->WriteRegister (PrevPCReg, pc);         // PrevPC <- PC
	machine->WriteRegister (PCReg, npc);            // PC <- NextPC
	machine->WriteRegister (NextPCReg, npc + 4);    // NextPC <- NextPC + 4
} // returnFromSystemCall

/* Este método lee datos de la memoria de usuario
 * Coloca el contenido del parámetro registro en el buffer temporal */
void mapeoMemoria (int registro, char* buffer) {
	int direccion = machine->ReadRegister (registro);
	int character = 1;
	for (int i = 0; character != '\0'; ++i) {
		machine->ReadMem (direccion + i, 1, &character);
		buffer[i] = (char) character;
	}
}

/* Asistete del SysCall exec */
void Exec_Aux (void* tmp) {
	int* executableName = (int*) tmp;
	char bufferExec_Aux[TAMBUFFER];
	mapeoMemoria (*executableName, bufferExec_Aux);

	OpenFile* executable = fileSystem->Open (bufferExec_Aux);

	currentThread->space = new AddrSpace (executable); // Crea el espacio para el ejecutable

	currentThread->space->InitRegisters();
	currentThread->space->RestoreState();
	machine->Run();	// jump to the user progam
}

/* Asistete del SysCall fork */
void AyudanteFork (void* parametro) {     //Metodo auxiliar que usa Fork
	int* p = (int*) parametro;
	AddrSpace* space;
	space = currentThread->space;
	space->InitRegisters();
	space->RestoreState();

	machine->WriteRegister (RetAddrReg, 4);
	machine->WriteRegister (PCReg, *p);
	machine->WriteRegister (NextPCReg, *p + 4);
	machine->Run();                     // jump to the user progam
	//ASSERT (false);
}

// System Calls

/* Stop Nachos, and print out performance stats */
void Nachos_Halt() {        //System call #0
	DEBUG ('a', "Shutdown, initiated by user program.\n");
	interrupt->Halt();
} //Nachos_Halt

/* Address space control operations: Exit, Exec, and Join */

/* This user program is done (status = 0 means exited normally). */
void Nachos_Exit() {    //System call # 1
	DEBUG ('a', "Entering Exit");
	int idProc = machine->ReadRegister (4);
	currentThread->Yield();
	currentThread->tablaProcesos->delThread();
	if (VACIO == currentThread->tablaProcesos->getUsage()) {
		Nachos_Halt();  //soy el ultimo, apague y vamonos
	}
	if (NULL != currentThread->space) {
		delete currentThread->space;
		currentThread->space = NULL;
	}

	//tengo que liberar los semaforos que yo como proceso habia creado
	while (VACIO != currentThread->tablaSemaforos->getUsage()) {
		Semaphore* semaforo = (Semaphore*) currentThread->tablaSemaforos->getUnixHandle (idProc);
		currentThread->tablaSemaforos->delThread();
		semaforo->V();  //lo libero
	}

	currentThread->Finish();    //termina el proceso
	returnFromSystemCall();
	DEBUG ('a', "Exiting Exit");

}//Nachos_Exit

/* Run the executable, stored in the Nachos file "name", and return the
 * address space identifier
 */
void Nachos_Exec() {    //System call # 2
	DEBUG ('a', "Entering exec System call\n");
	Thread* nuevoHilo = new Thread ("New Executable");    //nuevo proceso hijo
	int tmp = machine->ReadRegister (4);
	nuevoHilo->Fork (Exec_Aux, &tmp);

	machine->WriteRegister (2, 0); // !
	returnFromSystemCall();
}//Nachos_Exec

/* Only return once the the user program "id" has finished.
 * Return the exit status.
 */
void Nachos_Join() {    //System call # 3
	SpaceId sID = machine->ReadRegister (4);
	DEBUG ('a', "Entering System Call Join.\n");
	Semaphore* semaforo = new Semaphore ("Semaforo join", 0);
	currentThread->tablaSemaforos->semOpen (sID, (long) semaforo);
	currentThread->tablaSemaforos->addThread();
	if (currentThread->tablaProcesos->getUnixHandle (sID) == ERROR) {
		machine->WriteRegister (2, ERROR);
	}
	else {
		machine->WriteRegister (2, VACIO);
	}
	semaforo->P();
	returnFromSystemCall();
	DEBUG ('a', "Exitin System Call Join.\n");
}//Nachos_Join

/* File system operations: Create, Open, Read, Write, Close
 * These functions are patterned after UNIX -- files represent
 * both files *and* hardware I/O devices.
 *
 * If this assignment is done before doing the file system assignment,
 * note that the Nachos file system has a stub implementation, which
 * will work for the purposes of testing out these routines.
 */

/* Create a Nachos file, with "name" */
void Nachos_Create() {  //System call # 4
	DEBUG ('a', "Entering Create.\n");
	char bufferCreate[TAMBUFFER] ;
	mapeoMemoria (4, bufferCreate);

	/*!!!!!!!!!!NOTA: Cambiar Permisos lectura, escritura */
	//S_IRWXU->permiso (al dueño) de lectura, ejecucion y escritura
	//S_IRWXO->otros tienen permiso de escritura, lectura y ejecucion
	//S_IRWXG->el grupo tiene permiso de escritura, lectura y ejecucion
	int idFileUnix = creat (bufferCreate, S_IRWXU | S_IRWXO | S_IRWXG);

	DEBUG ('a', "File created on UNIX with id: %d\n", idFileUnix);
	int idFileNachos = currentThread->tablaFiles->Open (idFileUnix);
	currentThread->tablaFiles->addThread();
	DEBUG ('a', "File created on Nachos with id: %d\n", idFileNachos);
	machine->WriteRegister (2, idFileNachos);
	returnFromSystemCall();
	DEBUG ('a', "Exiting Create.\n");

}//Nachos_Create

/* Open the Nachos file "name", and return an "OpenFileId" that can
 * be used to read and write to the file.
 */
void Nachos_Open() {    // System call # 5
	DEBUG ('a', "Entering Open.\n");
	int rutaVirtual = machine->ReadRegister (4);
	int rutaFisica;
	char bufferOpen[TAMBUFFER];
	int i = 0;
	bool seguir = true;
	do {
		machine->ReadMem (rutaVirtual, 1, &rutaFisica);
		if ( (char) rutaFisica != '\0') {
			bufferOpen[i] = (char) rutaFisica;
			++rutaVirtual;
			++i;
		}
		else {
			bufferOpen[i] = (char) rutaFisica;
			seguir = false;
		}
	}
	while (seguir);
	printf ("File name %s opened.\n", bufferOpen);

	OpenFileId idFileUnix = open (bufferOpen, O_RDWR);

	if (idFileUnix != ERROR) {
		int idFileNachos = currentThread->tablaFiles->Open (idFileUnix);
		currentThread->tablaFiles->addThread();
		machine->WriteRegister (2, idFileNachos);
	}
	else {
		printf ("ERROR: File doesn't exist.\n");
		machine->WriteRegister (2, ERROR);
	}
	returnFromSystemCall();
	DEBUG ('a', "Exiting Open.\n");
}// Nachos_Open

/* Read "size" bytes from the open file into "buffer".
 * Return the number of bytes actually read -- if the open file isn't
 * long enough, or if it is an I/O device, and there aren't enough
 * characters to read, return whatever is available (for I/O devices,
 * you should always wait until you can return at least one character).
 */
void Nachos_Read() {    //System call # 6
	DEBUG ('a', "Entering Nachos_Read.\n");
	int size = machine->ReadRegister (5);           //tamano de lo que voy a leer
	OpenFileId idFileNachOS = machine->ReadRegister (6);  //le indica si lee de consola o de un archivo
	char bufferRead[TAMBUFFER] = {'\0'};

	int cantidadTotal = 0;
	semMutexAux.P();   //Solo yo puedo leer
	switch (idFileNachOS) {
		//lecturas de consola
		case ConsoleOutput:
			machine->WriteRegister (2, ERROR);
			printf ("ERROR: User can't read from console output.\n");
			break;
		case ConsoleInput:
			cantidadTotal = read (1, bufferRead, size);  //lee con el read de UNIX
			machine->WriteRegister (2, cantidadTotal);
			stats->numConsoleCharsRead += cantidadTotal;
			break;
		case ConsoleError:
			printf ("ERROR: Console error %d.\n", -1);
			break;
		default:
			//lectura de archivos
			if (currentThread->tablaFiles->isOpened (idFileNachOS)) { //esta abierto?
				int idUnix = currentThread->tablaFiles->getUnixHandle (idFileNachOS);
				cantidadTotal = read (idUnix, bufferRead, size);   //lee con el read de UNIX
				machine->WriteRegister (2, cantidadTotal);
				bool flag = true;
				for (int i = 0; i < cantidadTotal && flag; ++i) {
					int chTmp = (int) bufferRead[i];
					machine->WriteMem (machine->ReadRegister (4) + i, 1, chTmp);
					if ( (char) chTmp == '\0') {
						flag = false;
					}
				}
				stats->numDiskReads++;
			}
			else {
				machine->WriteRegister (2, ERROR);  //se quiere leer de un archivo que no esta abierto
				printf ("ERROR: File doesn't exist.\n");
			}
	}
	semMutexAux.V();
	returnFromSystemCall();
	DEBUG ('a', "Exiting Nachos_Read.\n");
} //Nachos_Read

/* Write "size" bytes from "buffer" to the open file. */
void Nachos_Write() {   // System call 7
	DEBUG ('a', "System call Write.\n");
	int size = machine->ReadRegister (5);	// Read size to write
	OpenFileId idFileNachOS = machine->ReadRegister (6);	// Read file descriptor
	int cantCaracteres = 0;
	char bufferWrite[TAMBUFFER];

	mapeoMemoria (4, bufferWrite); // Deja lo que se tiene que escribir en bufferWrite
	semMutexAux.P();   //solo yo puedo escribir
	switch (idFileNachOS) {
		case  ConsoleInput:	// User could not write to standard input
			machine->WriteRegister (2, ERROR);
			break;
		case  ConsoleOutput:
			bufferWrite[ size ] = '\0';
			stats->numConsoleCharsWritten += size;
			printf ("%s \n", bufferWrite);
			break;
		case ConsoleError: // This trick permits to write integers to console
			printf ("%d\n", machine->ReadRegister (4));
			break;
		default:
			if (currentThread->tablaFiles->isOpened (idFileNachOS)) {
				write (currentThread->tablaFiles->getUnixHandle (idFileNachOS), bufferWrite, size);
				machine->WriteRegister (2, cantCaracteres);
				stats->numDiskWrites++;
			}
			else {
				machine->WriteRegister (2, ERROR);
			}
	}
	semMutexAux.V();
	returnFromSystemCall(); // Update the PC registers
	DEBUG ('a', "Exiting Write System Call.\n");

} // Nachos_Write

/* Close the file, we're done reading and writing to it. */
void Nachos_Close() {       //System call # 8
	DEBUG ('a', "Entering Close System Call.\n");
	OpenFileId idHilo = machine->ReadRegister (4);
	if (currentThread->tablaFiles->isOpened (idHilo)) {
		int idUnix = currentThread->tablaFiles->getUnixHandle (idHilo);
		int tmp = currentThread->tablaFiles->Close (idHilo);
		tmp = close (idUnix);
		printf ("File closed.\n");
		machine->WriteRegister (2, tmp);
	}
	else {
		printf ("ERROR: Atempt to close a file that isn't opened.\n");
		machine->WriteRegister (2, ERROR);
	}
	returnFromSystemCall();
	DEBUG ('a', "Exiting Close System Call.\n");

}//Nachos_Close

/* User-level thread operations: Fork and Yield.  To allow multiple
 * threads to run within a user program.
 */

/* Fork a thread to run a procedure ("func") in the *same* address space
 * as the current thread.
 */
void Nachos_Fork() {    //System call # 9
	DEBUG ('a', "Entering Fork System call\n");
	Thread* nuevoHilo = new Thread ("Hijo");    //nuevo proceso hijo
	nuevoHilo->tablaFiles = currentThread->tablaFiles;  //NachosOpenFilesTable tiene sobrecargado el operador = entonces no hay bronca :D
	int idHilo = currentThread->tablaProcesos->Open ( (long) nuevoHilo); //agrega el hilo nuevo a la tabla de procesos
	currentThread->tablaFiles->addThread();
	currentThread->tablaProcesos->addThread();
	nuevoHilo->space = new AddrSpace (currentThread->space); // Asigna el espacio a al hijo (DS y CS = father. SS independiente)
	DEBUG ('a', "Calls ForkHelper.\n");
	int tmp = machine->ReadRegister (4);
	nuevoHilo->Fork (AyudanteFork, &tmp);

	returnFromSystemCall();

	DEBUG ('a', "Exiting Fork System call\n");
}//Kernel_Fork

/* Yield the CPU to another runnable thread, whether in this address space
 * or not.
 */
void Nachos_Yield() {               //System call # 10
	DEBUG ('a', "Yield, returns control to the CPU.\n");
	currentThread->Yield();
	returnFromSystemCall(); // !
	DEBUG ('a', "Exiting Yield.\n");
}//Nachos_Yield

/* SemCreate creates a semaphore initialized to initval value
 * return the semaphore id
 */
void Nachos_SemCreate() {   //System call # 11
	DEBUG ('a', "Entering SemCreate.\n");

	int valSem = machine->ReadRegister (4);     //lee del registro de MIPS
	Semaphore* semaforo = new Semaphore ("Semaforo nuevo", valSem);  //crea el semaforo de NachOS
	int idSem = currentThread->tablaFiles->Open ( (long) semaforo);
	currentThread->tablaFiles->addThread();

	machine->WriteRegister (2, idSem);     //retorna el el id del semaforo creado como parametro
	returnFromSystemCall();
	DEBUG ('a', "Exiting SemCreate. \n");

}//Nachos_SemCreate

/* SemDestroy destroys a semaphore identified by id */
void Nachos_SemDestroy() {  //System call # 12
	DEBUG ('a', "Entering SemDestroy.\n");
	int idSem = machine->ReadRegister (4);     //lee el parametro
	Semaphore* semaforo = (Semaphore*) currentThread->tablaFiles->getUnixHandle (idSem);
	currentThread->tablaFiles->Close (idSem);
	currentThread->tablaFiles->delThread();
	semaforo->Destroy();
	returnFromSystemCall();
	DEBUG ('a', "Exiting SemDestroy.\n");

}//Nachos_SemDestroy

/* SemSignal signals a semaphore, awakening some other thread if necessary */
void Nachos_SemSignal() {   //System call # 13
	DEBUG ('a', "Entering SemSignal.\n");
	int idSem = machine->ReadRegister (4);
	Semaphore* semaforo = (Semaphore*) currentThread->tablaFiles->getUnixHandle (idSem);
	semaforo->V();
	returnFromSystemCall();
	DEBUG ('a', "Exiting SemSignal.\n");

}//Nachos_SemSignal

/* SemWait waits a semaphore, some other thread may awake if one blocked */
void Nachos_SemWait() {     //System call # 14
	DEBUG ('a', "Entering SemWait.\n");
	int idSem = machine->ReadRegister (4);
	Semaphore* semaforo = (Semaphore*) currentThread->tablaFiles->getUnixHandle (idSem);
	semaforo->P();
	returnFromSystemCall();
	DEBUG ('a', "Exiting SemWait.\n");

}//Nachos_SemWait

void ExceptionHandler (ExceptionType which) {
	int type = machine->ReadRegister (2);
	DEBUG ('a', "Entre en exception handler con type %d y ExceptionType %d\n", type, which);

	switch (which) {
		case SyscallException:
			switch (type) {
				case SC_Halt:
					Nachos_Halt();              // System call # 0
					returnFromSystemCall();
					break;
				case SC_Exit:
					Nachos_Exit();              //System call # 1
					break;
				case SC_Exec:
					Nachos_Exec();              //System call # 2
					ASSERT (false);
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
					printf ("Second switch-> unexpected exception %d\n", which);
					ASSERT (false);
					break;
			}
			break;
		default:
			printf ("First switch -> unexpected exception %d\n", which);
			ASSERT (false);
			break;
	}
}
