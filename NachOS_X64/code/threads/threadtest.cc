// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create several threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustrate the inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.
//

#include <unistd.h>
#include "copyright.h"
#include "system.h"
#include "dinningph.h"
#include "synch.h"

#define MAX_CARS 4
#define NUM_HILOS 22
#define SILLAS_DISP 9
#define NORTE 1
#define SUR 2
#define VACIO 0

DinningPh * dp;

//-------------------------- Memoria compartida problema del puente ----------------------------------------
Semaphore semNorte("semaforoNorte", 0);
Semaphore semSur("semaforoSur", 0);
int carrosNorte = 0;
int carrosSur = 0;
int sentido = 0; //0=puente vacio, 1=carros pasando del norte, 2=carros pasando del sur


//-------------------------- Memoria compartida problema del agua ------------------------------------------
Semaphore semaforo("hola", 1);
Semaphore semH("H", 0);
Semaphore semO("O", 0);
int numH = 0;
int numO = 0;


//-------------------------- Memoria compartida problema canibales y misioneros-------------------------------
Semaphore semCanibal("Canibal", 0);
Semaphore semMisionero("misionero", 0);
int count_canibal = 0;
int count_misionero = 0;


//---------------------
//      Metodos       |
//---------------------
void llegaNorte(int numMax){
    semaforo.P();	//protege la región crítica
    if(sentido == VACIO || carrosNorte >= numMax){  //nadie esta pasando por el puente o hay mas de 4 carros esperando en el norte
        sentido = NORTE;
    }
    if(carrosSur >= numMax && carrosNorte >= numMax){   //si hay mas de 4 carros esperando en ambos sentidos, le da prioridad a los que vienen desde el sur
        sentido = SUR;
        ++carrosNorte; //espera un carro del norte
        printf("--- Hay más de %d carros en ambos sentidos ---\n", numMax);
        printf("** Espera un carro en el norte.\n");
        while(carrosSur > 2){
            printf("** Sube un carro del sur.\n");
            --carrosSur;
            semSur.V();
        }
        semaforo.V();
        semNorte.P();
    }else{
        if(carrosSur < numMax && sentido == NORTE){
            printf("** Baja un carro del norte.\n");
            --carrosNorte;
            semaforo.V();
            semNorte.V();
        }
        if(carrosSur>= numMax || sentido == SUR){
            ++carrosNorte;
            printf("---- Hay %d carros esperando en el norte ----\n", carrosNorte);
            semaforo.V();
            semNorte.P();
        }
    }
}

void llegaSur(int numMax){
    semaforo.P();	//protege la región crítica
    if(sentido == VACIO || carrosSur >= numMax){
        sentido = SUR;
    }
    if(carrosSur >= numMax && carrosNorte >= numMax){	//le da prioridad a los que viajan de Sur a Norte
        sentido = SUR;
        printf("--- Hay más de %d carros en ambos sentidos ---\n", numMax);
        while(carrosSur > 2){
            printf("** Sube un carro del sur.\n");
            --carrosSur;
            semSur.V();
        }
        semaforo.V();
    }else{
        if(carrosNorte < numMax && sentido == SUR){	//hay menos de numMax carros esperando en el norte
            printf("** Sube un carro del sur.\n");
            --carrosSur;
            semaforo.V();
            semSur.V();
        }
        if(carrosNorte>= numMax || sentido == NORTE){
            ++carrosSur;
            printf("---- Hay %d carros esperando en el sur ----\n", carrosSur);
            semaforo.V();
            semSur.P();
        }
    }
}

void generaCarros(void* name){
    char* nombreHilo = (char*)name;
    if((rand()%2) == 0){
        printf("%s hizo que llegara un carro desde el norte.\n", nombreHilo);
        llegaNorte(MAX_CARS);
    }else{
        printf("%s hizo que llegara un carro desde el sur.\n", nombreHilo);
        llegaSur(MAX_CARS);
    }

}

//----------------- Para crear agua ------------------------------
void hidrogeno(){

    semaforo.P(); //para que no se altere la region critica
    if( (numO>0) && (numH>0) ){ //hay suficientes?
        --numO;
        --numH;
        printf("-----Hice agua-----\n");
        semaforo.V(); //libera la region critica
        semO.V();
        semH.V();
    }else{
        ++numH;
        semaforo.V(); //libera la region critica
        semH.P();
    }
}

void oxigeno(){

    semaforo.P(); //para que no se altere la region critica
    if(numH>1){
        numH -= 2;
        printf("-----Hice agua-----\n");
        semaforo.V(); //libeta la region critica
        semH.V();
        semH.V();
    }else{
        ++numO;
        semaforo.V(); //libera la region critica
        semO.P();
    }
}

//-----------------------------------------------------------
// Se encarga de decidir si crea un hidrogeno o un oxigeno	|
//-----------------------------------------------------------
void agua(void* p){

    char* quien = (char*)p;
    if( 0 == (rand()%2) ){
        printf("%s va a crear Oxigeno\n", quien);
        oxigeno();
    }else{
        printf("%s va a crear Hidrogeno\n", quien);
        hidrogeno();
    }
}

// -------------------------------- Problema de los filosofos --------------------------------------
void Philo( void * p ) {

    int eats, thinks;
    long who = (long) p;

    currentThread->Yield();

    for ( int i = 0; i < 10; i++ ) {

        printf(" Philosopher %ld will try to pickup sticks\n", who + 1);

        dp->pickup( who );
        dp->print();
        eats = Random() % 6;

        currentThread->Yield();
        sleep( eats );

        dp->putdown( who );

        thinks = Random() % 6;
        currentThread->Yield();
        sleep( thinks );
    }

}


//----------------------------------------------------------------------
// SimpleThread
// 	Loop 10 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"name" points to a string with a thread name, just for
//      debugging purposes.
//----------------------------------------------------------------------

void SimpleThread(void* name){
    // Reinterpret arg "name" as a string
    char* threadName = (char*)name;
    
    // If the lines dealing with interrupts are commented,
    // the code will behave incorrectly, because
    // printf execution may cause race conditions.
    for (int num = 0; num < 8; num++) {
        //IntStatus oldLevel = interrupt->SetLevel(IntOff);
        semaforo.P();
        printf("*** thread %s looped %d times\n", threadName, num);
        semaforo.V();
        //interrupt->SetLevel(oldLevel);
        //currentThread->Yield();
    }
    semaforo.P();
    //IntStatus oldLevel = interrupt->SetLevel(IntOff);
    printf(">>> Thread %s has finished\n", threadName);
    semaforo.V();
    //interrupt->SetLevel(oldLevel);
}

void canibal();
void misionero();

void pasoRio(void* name){
    char* quien = (char*) name;
    if( 0 == (rand()%2) ){
        printf("%s va a crear un Canibal. uy que miedo!\n", quien);
        canibal();
    }else{
        printf("%s va a crear un Misionero. Conviertelos a todos campeon\n", quien);
        misionero();
    }
}

void canibal(){
    semaforo.P();
    // Caso 1: Hay 3 canibales
    if(count_canibal > 1) {
        count_canibal -= 2;
        printf("El barco se va con 3 canibales\n");
        semaforo.V();
        semCanibal.V();
        semCanibal.V();
    }else if (count_misionero > 1) { // Caso 2: Hay 2 misioneros
        count_misionero -= 2;
        printf("El barco se va con 2 misioneros y un canibal\n");
        semaforo.V();
        semMisionero.V();
        semMisionero.V();
    }
    else {
        ++count_canibal;
        semaforo.V();
        semCanibal.P();
    }
}

void misionero() {
    semaforo.P();
    // Caso 1: Hay 3 misioneros
    if(count_misionero > 1) {
        count_misionero -= 2;
        printf("El barco se va con 3 misioneros\n");
        semaforo.V();
        semMisionero.V();
        semMisionero.V();
    }
    else if (count_misionero > 0 && count_canibal > 0) { // Caso 2: Hay 2 misioneros & un canibal
        --count_misionero;
        --count_canibal;
        printf("El barco se va con 2 misioneros y un canibal\n");
        semaforo.V();
        semMisionero.V();
        semCanibal.V();
    }
    else {
        ++count_misionero;
        semaforo.V();
        semMisionero.P();
    }
}

//----------------------------------------------------------------------
// ThreadTest
// 	Set up a ping-pong between several threads, by launching
//	ten threads which call SimpleThread, and finally calling 
//	SimpleThread ourselves.
//----------------------------------------------------------------------

void ThreadTest(){
    Thread * Ph;

    DEBUG('t', "Entering SimpleTest");

    /*
    //para crear agua
    for(int i=0; i<22; ++i){
        char* nombreHilo = new char[100];
        sprintf(nombreHilo, "Hilo %d", i);
        Thread* nuevoThread = new Thread(nombreHilo);
        nuevoThread->Fork(agua, (void*) nombreHilo);
    }*/

    /*
    //canibales y misioneros
    for(int i=0; i<22; ++i){
        char* nombreHilo = new char[100];
        sprintf(nombreHilo, "Hilo %d", i);
        Thread* nuevoHilo = new Thread(nombreHilo);
        nuevoHilo->Fork(pasoRio, (void*)nombreHilo);
    }*/

    /*
    dp = new DinningPh();

    for ( long k = 0; k < 5; k++ ) {
        Ph = new Thread( "dp" );
        Ph->Fork( Philo, (void *) k );
    }

    return;

    for ( int k=1; k<=7; k++) {
      char* threadname = new char[100];
      sprintf(threadname, "Hilo %d", k);
      Thread* newThread = new Thread (threadname);
      newThread->Fork (SimpleThread, (void*)threadname);
    }
    SimpleThread( (void*)"Hilo 0");
    */

    //puente
    for(int i=1; i<=NUM_HILOS; ++i){
        char* threadname = new char[100];
        sprintf(threadname, "Hilo %d", i);
        Thread* newThread = new Thread (threadname);
        newThread->Fork (generaCarros, (void*)threadname);
    }

}

