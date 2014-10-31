/** Universidad de Costa Rica
 * Escuela de Ciencias de la Computacion e Informatica
 * Sistemas Operativos CI-1310
 * Modificacion a NachOS
 * Fabian Rodriguez Obando  B25695
 * Kevin Delgado Sandi      B22214
 * II Semestre 2014
 */

#include "nachostabla.h"


NachosOpenFilesTable::NachosOpenFilesTable(){

    openFiles = new int(TAM_VECTOR);
    for(int i=0; i<TAM_VECTOR; ++i){    //pone todos los datos del vector en -1
        openFiles[i] = BANDERA;
    }
    usage = 1;

}

NachosOpenFilesTable::~NachosOpenFilesTable(){
    if(usage>0){
        delete[] openFiles;
        usage = 0;
    }
}

NachosOpenFilesTable& NachosOpenFilesTable::operator=(const NachosOpenFilesTable& tablaOriginal){
    for(int i=0; i<TAM_VECTOR; ++i){
        openFiles[i] = tablaOriginal.openFiles[i];
    }
    usage = tablaOriginal.usage;    //los archivos que hay abiertos
    ++usage;                        //mas yo
    return *this;
}

int NachosOpenFilesTable::Open(int UnixHandle){
    int posicion = 3;
    while(openFiles[posicion] != BANDERA && posicion<TAM_VECTOR){ //busca el primer campo que este vacio
        ++posicion;
    }
    if(posicion<TAM_VECTOR){
        openFiles[posicion] = UnixHandle;
        return posicion;
    }else{
        return -1;  //no hay campo
    }
}

int NachosOpenFilesTable::Close(int NachosHandle){
    if(NachosHandle<=usage){
        openFiles[NachosHandle] = BANDERA;
        return 0;
    }else{
        return -1;
    }
}

bool NachosOpenFilesTable::isOpened(int NachosHandle){
    if(openFiles[NachosHandle] != BANDERA){
        return true;
    }else{
        return false;
    }
}

int NachosOpenFilesTable::getUnixHandle(int NachosHandle){
    if(NachosHandle <= usage ){
        return openFiles[NachosHandle];
    }else{
        return -1;
    }
}

void NachosOpenFilesTable::addThread(){
    ++usage;
}

void NachosOpenFilesTable::delThread(){
    --usage;
}

void NachosOpenFilesTable::semOpen(int posicion, long sem){
    openFiles[posicion]= sem;
}

int NachosOpenFilesTable::getUsage(){
    return usage;
}

void NachosOpenFilesTable::Print(){
    printf("The table contains:\n");

    for(int i=2; i<usage; ++i){
        printf("%d --> %d", i, openFiles[i]);
    }
}
