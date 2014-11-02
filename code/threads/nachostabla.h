#define TAM_VECTOR 64
#define BANDERA -1
#include <unistd.h>
#include <stdio.h>
#include "../userprog/bitmap.h"

class NachosOpenFilesTable {
	public:
		NachosOpenFilesTable();       // Initialize
		~NachosOpenFilesTable();      // De-allocate

		// Métodos correspondiente a archivos
		int Open (int UnixHandle);  // Register the file handle
		int Close (int NachosHandle);       // Unregister the file handle
		bool isOpened (int NachosHandle);
		int getUnixHandle (int NachosHandle);

		// Métodos correspondiente a Thread
		void addThread();		// If a user thread is using this table, add it
		void delThread();		// If a user thread is using this table, delete it
		int getUsage();		    //devuelve cuantos hilos hay en la tabla

		void Print();               // Print contents

	private:
		// Los miebros de datos estarán en memoria dinámica ya que serán compartidos por varios Threads
		long int* openFiles;			// A vector with user opened files
		BitMap* openFilesMap;	// A bitmap to control our vector
		int* usage;				// How many threads are using this table
};
