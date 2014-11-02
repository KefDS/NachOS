/** Universidad de Costa Rica
 * Escuela de Ciencias de la Computacion e Informatica
 * Sistemas Operativos CI-1310
 * Modificacion a NachOS
 * Fabian Rodriguez Obando  B25695
 * Kevin Delgado Sandi      B22214
 * II Semestre 2014
 */

#include "nachostabla.h"

NachosOpenFilesTable::NachosOpenFilesTable()
	: openFiles (new int[TAM_VECTOR])
	, openFilesMap (new BitMap(TAM_VECTOR))
	, usage (0) {
	// Marca como ocupado las 3 primeras pocisiones (stdin, stdout, stderr)
	openFilesMap->Mark (0);
	openFilesMap->Mark (1);
	openFilesMap->Mark (2);
}

NachosOpenFilesTable::~NachosOpenFilesTable() {
	delete [] openFiles;
	delete openFilesMap;
}

NachosOpenFilesTable& NachosOpenFilesTable::operator= (const NachosOpenFilesTable& other) {
	for (int i = 0; i < TAM_VECTOR; ++i) {
		openFiles[i] = other.openFiles[i];
	}
	*(openFilesMap) = *(other.openFilesMap);
	usage = 0;
	return *this;
}

int NachosOpenFilesTable::Open (int UnixHandle) {
	int i = BANDERA;
	if (!isOpened (UnixHandle)) {
		i = openFilesMap->Find();	// Encuentra un espacio libre
		openFiles[i] = UnixHandle;	// Lo inserta en la tabla
	}
	return i; // Devuelve el índice donde se encuentra el archivo, o -1 si no encuentra campo
}

int NachosOpenFilesTable::Close (int NachosHandle) {
	int control = BANDERA;
	if (openFilesMap->Test (NachosHandle)) {
		openFilesMap->Clear (NachosHandle); // Hace que el bitmap ponga ese índice como libre
		control = 1;
	}
	return control;
}

bool NachosOpenFilesTable::isOpened (int NachosHandle) {
	bool resultado = false;
	if (openFilesMap->Test (NachosHandle)) {
		resultado = true;
	}
	return resultado;
}

int NachosOpenFilesTable::getUnixHandle (int NachosHandle) {
	int index = BANDERA;
	if (isOpened (NachosHandle)) {
		index = openFiles[NachosHandle];
	}
	return index;
}

void NachosOpenFilesTable::addThread() {
	++usage;
}

void NachosOpenFilesTable::delThread() {
	--usage;
}

int NachosOpenFilesTable::getUsage() {
	return usage;
}

void NachosOpenFilesTable::Print() {
	printf ("The table contains:\n");

	for (int i = 3; i < TAM_VECTOR; ++i) {
		printf ("%d --> %d\n", i, openFiles[i]);
	}
}
