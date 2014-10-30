#define TAM_VECTOR 256
#define BANDERA -1
#include <unistd.h>
#include <stdio.h>

class NachosOpenFilesTable {
public:
    NachosOpenFilesTable();       // Initialize
    ~NachosOpenFilesTable();      // De-allocate
    
    int Open( int UnixHandle ); // Register the file handle
    void semOpen(int pos, long semaforo);
    int Close( int NachosHandle );      // Unregister the file handle
    bool isOpened( int NachosHandle );
    int getUnixHandle( int NachosHandle );
    void addThread();		// If a user thread is using this table, add it
    void delThread();		// If a user thread is using this table, delete it
    int getUsage();		//devuelve cuantos hilos hay en la tabla

    NachosOpenFilesTable& operator=(const NachosOpenFilesTable& tablaOriginal); //para copiar NachosOpenFilesTable's

    void Print();               // Print contents
    
private:
    int * openFiles;		// A vector with user opened files
    int usage;              // How many threads are using this table

};
