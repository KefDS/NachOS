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
#define CANTSEMS 60
#define NUMTHREADS 10

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

/* Como nos dimos cuenta en la tarea 0 y tarea 1 los semáforos no eran propios
 * de los threads, si no del sistema operativo.
 * Es por esto que decidimos hacer un vector de semforos disponibles
 * para todos los threads y de esta manera con solo el ID del semforo puedan
 * accesar a el semáforo específico
*/
Semaphore* semVec[CANTSEMS];
BitMap* openSems = new BitMap (CANTSEMS);

 // BitMap que tendrá información acerca de los id de los threads
BitMap index_map (NUMTHREADS);

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
void execAux (void* executableName) {
	// Tomado de StartProcess en progtest.cc
	DEBUG ('f', "Entre a execAux.\n");
	OpenFile* p = (OpenFile*) executableName;
	if (p == NULL) {
		printf ("Unable to open file");
		return;
	}

	currentThread->space = new AddrSpace (p); // Crea el espacio para el ejecutable

	currentThread->space->InitRegisters(); // set the initial register values
	currentThread->space->RestoreState();  // load page table register
	DEBUG ('f', "Justo antes de correr el programa.\n");
	machine->Run(); // jump to the user progam
	ASSERT (false);			// machine->Run never returns;
	// the address space exits
	// by doing the syscall "exit"
}

/* Asistete del SysCall fork */
void forkAux (void* address) {     //Metodo auxiliar que usa Fork
	AddrSpace* space;

	space = currentThread->space;
	space->InitRegisters();
	space->RestoreState();

	// Set the return address for this thread to the same as the main thread
	// This will lead this thread to call the exit system call and finish
	machine->WriteRegister (RetAddrReg, 4);
	machine->WriteRegister (PCReg, (long) address);
	machine->WriteRegister (NextPCReg, (long) address + 4);
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

/* This user program is done (status = 0 means exited normally).
 *
 * void Exit(int status);
 */
void Nachos_Exit() {    //System call # 1
	DEBUG ('f', "Entering Exit\n");
	int status = machine->ReadRegister (4);
	DEBUG ('f', "Antes de hacer Yield\n");
	currentThread->Yield();
	DEBUG ('f', "Sali de Yield\n");
	currentThread->m_filesTable->delThread();
	DEBUG ('f', "Antes de hacer Halt\n");
	if (VACIO == currentThread->m_filesTable->getUsage()) {
		Nachos_Halt();  //soy el ultimo, apague y vamonos
	}
	if (NULL != currentThread->space) {
		delete currentThread->space;
		currentThread->space = NULL;
	}
	DEBUG ('f', "Antes de liberar los semaforos\n");
	//tengo que liberar los semaforos que yo como proceso habia creado
	for (int i = 0; i < CANTSEMS; ++i) {
		if (currentThread->m_semaphoreTable->isOpened (i)) {
			semVec[i]->V();
			openSems->Clear (i);
			currentThread->m_semaphoreTable->Close (i);
			currentThread->m_semaphoreTable->delThread();
		}
	}

	currentThread->Finish();    //termina el proceso
	returnFromSystemCall();
	DEBUG ('a', "Exiting Exit");
}//Nachos_Exit


/* Run the executable, stored in the Nachos file "name", and return the
 * address space identifier
 *
 * SpaceId Exec(char *name);
 */
void Nachos_Exec() {    //System call # 2
	char bufferExec_Aux[TAMBUFFER];
	DEBUG ('a', "Entering exec System call\n");
	mapeoMemoria (4, bufferExec_Aux);
	DEBUG ('f', "Buffer tiene: %s\n", bufferExec_Aux); // Temporal

	// Trata de abrir la ruta indicada
	OpenFile* executable = fileSystem->Open (bufferExec_Aux);
	if (executable == NULL) {
		printf ("Unable to open file %s\n", bufferExec_Aux);
		machine->WriteRegister (2, ERROR);
		return;
	}

	Thread* newThread = new Thread ("New Executable");    //nuevo proceso hijo
	DEBUG ('f', "Antes de llamar a Fork de thread.\n");
	newThread->Fork (execAux, (void*) executable);

	machine->WriteRegister (2, index_map.Find());
	returnFromSystemCall();
} //Nachos_Exec

/* Only return once the the user program "id" has finished.
 * Return the exit status.
 *
 * int Join(SpaceId id);
 */
void Nachos_Join() {    //System call # 3
	SpaceId sID = machine->ReadRegister (4);
	DEBUG ('f', "Entering System Call Join.\n");
	Semaphore* semaforo = new Semaphore ("Semaforo join", 0);
	//currentThread->m_semaphoreTable->semOpen (sID, (long) semaforo);
	//currentThread->m_semaphoreTable->addThread();
	if (currentThread->m_processTable->getUnixHandle (sID) == ERROR) {
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

/* Create a Nachos file, with "name"
 *
 * void Create(char *name)
*/
void Nachos_Create() {  //System call # 4
	DEBUG ('f', "Entering Create.\n");
	char bufferCreate[TAMBUFFER];
	mapeoMemoria (4, bufferCreate);
	//S_IRUSR -> usuario tiene permiso de lectura
	//S_IWUSR -> usuario tiene permisos de escritura
	//S_IRGRP -> grupo triene permisos de lectura
	//S_IWGRP -> grupo tiene permisos de escritura
	//S_IROTH -> otros tienen permisos de lectura
	//S_IWOTH -> otros tienen permisos de escritura
	int idFileUnix = creat (bufferCreate, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

	DEBUG ('f', "File created on UNIX with id: %d\n", idFileUnix);
	int idFileNachos = currentThread->m_filesTable->Open (idFileUnix);
	currentThread->m_filesTable->addThread();
	DEBUG ('f', "File created on Nachos with id: %d\n", idFileNachos);
	machine->WriteRegister (2, idFileNachos);
	returnFromSystemCall();
	DEBUG ('f', "Exiting Create.\n");
} //Nachos_Create

/* Open the Nachos file "name", and return an "OpenFileId" that can
 * be used to read and write to the file.
 *
 * OpenFileId Open(char *name);
 */
void Nachos_Open() {    // System call # 5
	DEBUG ('a', "Entering Open.\n");
	char bufferOpen[TAMBUFFER];
	mapeoMemoria (4, bufferOpen);

	OpenFileId idFileUnix = open (bufferOpen, O_RDWR); // Abre archivo (SysCall de UNIX)

	if (idFileUnix != ERROR) {
		int idFileNachos = currentThread->m_filesTable->Open (idFileUnix); // Lo inserta en la tabla de archivos
		machine->WriteRegister (2, idFileNachos); // El método devuelve el índice del archivo en la tabla nachOS
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
 *
 * int Read(char *buffer, int size, OpenFileId id);
 */
void Nachos_Read() {    //System call # 6
	DEBUG ('a', "Entering Nachos_Read.\n");
	int size = machine->ReadRegister (5);				 // tamano de lo que voy a leer
	OpenFileId idFileNachOS = machine->ReadRegister (6); // el índice del archivo
	char bufferRead[TAMBUFFER];

	int cantidadTotalBytesLeidos = 0;
	semMutexAux.P();   // lock
	switch (idFileNachOS) {
		//lecturas de consola
		case ConsoleOutput:
			machine->WriteRegister (2, ERROR);
			printf ("ERROR: User can't read from console output.\n");
			break;
		case ConsoleInput:
			cantidadTotalBytesLeidos = read (0, bufferRead, size);  // lee en el stdin
			machine->WriteRegister (2, cantidadTotalBytesLeidos);
			stats->numConsoleCharsRead += cantidadTotalBytesLeidos;
			break;
		case ConsoleError:
			printf ("ERROR: Console error %d.\n", -1);
			break;
		default:
			//lectura de archivos
			if (currentThread->m_filesTable->isOpened (idFileNachOS)) { //esta abierto?
				int idUnix = currentThread->m_filesTable->getUnixHandle (idFileNachOS);
				cantidadTotalBytesLeidos = read (idUnix, bufferRead, size);   //lee con el read de UNIX
				machine->WriteRegister (2, cantidadTotalBytesLeidos);
				// Copia el contenido del buffer local al buffer proporcionado por el usuario
				bool flag = true;
				for (int i = 0; i < cantidadTotalBytesLeidos && flag; ++i) {
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
	semMutexAux.V(); // release
	returnFromSystemCall();
	DEBUG ('a', "Exiting Nachos_Read.\n");
} //Nachos_Read

/* Write "size" bytes from "buffer" to the open file.
 *
 * void Write(char *buffer, int size, OpenFileId id);
 */
void Nachos_Write() {   // System call 7
	DEBUG ('a', "System call Write.\n");
	int size = machine->ReadRegister (5);	// Read size to write
	OpenFileId idFileNachOS = machine->ReadRegister (6);	// Read file descriptor
	int cantCaracteres = 0;
	char bufferWrite[TAMBUFFER];

	mapeoMemoria (4, bufferWrite); // Deja el contenido que se tiene que escribir en bufferWrite
	semMutexAux.P();   // lock
	switch (idFileNachOS) {
		case ConsoleInput:	// User could not write to standard input
			machine->WriteRegister (2, ERROR);
			break;
		case ConsoleOutput:
			bufferWrite[ size ] = '\0';
			stats->numConsoleCharsWritten += size;
			write (1, bufferWrite, size); // Escribe en stdout
			break;
		case ConsoleError: // This trick permits to write integers to console
			printf ("%d\n", machine->ReadRegister (4));
			break;
		default:
			if (currentThread->m_filesTable->isOpened (idFileNachOS)) {
				write (currentThread->m_filesTable->getUnixHandle (idFileNachOS), bufferWrite, size);
				machine->WriteRegister (2, cantCaracteres);
				stats->numDiskWrites++;
			}
			else {
				machine->WriteRegister (2, ERROR);
			}
	}
	semMutexAux.V(); // Release
	returnFromSystemCall(); // Update the PC registers
	DEBUG ('a', "Exiting Write System Call.\n");
} // Nachos_Write

/* Close the file, we're done reading and writing to it. */
void Nachos_Close() {       //System call # 8
	DEBUG ('a', "Entering Close System Call.\n");
	OpenFileId idFile = machine->ReadRegister (4);
	if (currentThread->m_filesTable->isOpened (idFile)) {
		int idUnix = currentThread->m_filesTable->getUnixHandle (idFile);
		int tmp = currentThread->m_filesTable->Close (idFile);
		tmp = close (idUnix);
		machine->WriteRegister (2, tmp);
		DEBUG ('a', "File closed.\n");
	}
	else {
		printf ("ERROR: Atempt to close a file that isn't opened.\n");
		machine->WriteRegister (2, ERROR);
	}
	returnFromSystemCall();
	DEBUG ('a', "Exiting Close System Call.\n");
} //Nachos_Close


/* User-level thread operations: Fork and Yield.  To allow multiple
 * threads to run within a user program.
 */

/* Fork a thread to run a procedure ("func") in the *same* address space
 * as the current thread.
 *
 * void Fork(void (*func)());
 */
void Nachos_Fork() {    //System call # 9
	DEBUG ('a', "Entering Fork System call\n");
	Thread* newThread = new Thread ("Hilo Hijo");    //nuevo proceso hijo
	newThread->m_filesTable = currentThread->m_filesTable; // Se copian los archivos abiertos
	currentThread->m_filesTable->addThread(); // Un hijo está usando la tabla de archivos
	newThread->space = new AddrSpace (currentThread->space); // Asigna el espacio a al hijo (DS y CS = father. SS independiente)

	DEBUG ('a', "Calls ForkHelper.\n");
	void* tmp = (void*) machine->ReadRegister (4);
	newThread->Fork (forkAux, tmp);

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
} //Nachos_Yield

/* SemCreate creates a semaphore initialized to initval value
 * return the semaphore id
 *
 * int SemCreate( int initval );
 */
void Nachos_SemCreate() {   //System call # 11
	DEBUG ('f', "Entering SemCreate.\n");

	int valSem = machine->ReadRegister (4);     //lee del registro de MIPS
	Semaphore* semaforo = new Semaphore ("Semaforo nuevo", valSem);  //crea el semaforo de NachOS

	int idSem = openSems->Find();
	if (idSem != ERROR) {
		semVec[idSem] = semaforo;
		currentThread->m_semaphoreTable->Open (idSem); //agrega el semaforo que creó a la tabla propia del hilo
		currentThread->m_semaphoreTable->addThread();
	}
	machine->WriteRegister (2, idSem);     //retorna el el id del semaforo creado como parametro
	returnFromSystemCall();
	DEBUG ('a', "Exiting SemCreate. \n");
}//Nachos_SemCreate

/* SemDestroy destroys a semaphore identified by id
 *
 * int SemDestroy( int SemId );
 */
void Nachos_SemDestroy() {  //System call # 12
	DEBUG ('f', "Entering SemDestroy.\n");
	int idSem = machine->ReadRegister (4);     //lee el parametro
	int retorna = ERROR;
	if (openSems->Test (idSem) == true) {
		currentThread->m_semaphoreTable->Close (idSem);
		currentThread->m_semaphoreTable->delThread();
		delete semVec[idSem];
		openSems->Clear (idSem);
		retorna = VACIO;
	}
	machine->WriteRegister (2, retorna);
	returnFromSystemCall();
	DEBUG ('a', "Exiting SemDestroy.\n");
}//Nachos_SemDestroy

/* SemSignal signals a semaphore, awakening some other thread if necessary
 *
 * int SemSignal( int SemId );
 */
void Nachos_SemSignal() {   //System call # 13
	DEBUG ('f', "Entering SemSignal.\n");
	int idSem = machine->ReadRegister (4);
	int retorna = ERROR;
	if (openSems->Test (idSem) == true) {
		semVec[idSem]->V();
		retorna = VACIO;
	}
	machine->WriteRegister (2, retorna);
	returnFromSystemCall();
	DEBUG ('a', "Exiting SemSignal.\n");
}//Nachos_SemSignal

/* SemWait waits a semaphore, some other thread may awake if one blocked
 *
 * int SemWait( int SemId );
 */
void Nachos_SemWait() {     //System call # 14
	DEBUG ('f', "Entering SemWait.\n");
	int idSem = machine->ReadRegister (4);
	int retorna = ERROR;
	if (openSems->Test (idSem) == true) {
		semVec[idSem]->P();
		retorna = VACIO;
	}
	machine->WriteRegister (2, retorna);
	returnFromSystemCall();
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
