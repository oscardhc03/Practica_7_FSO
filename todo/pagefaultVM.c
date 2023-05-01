#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <mmu.h>

#define NUMPROCS 4
#define PAGESIZE 4096
#define PHISICALMEMORY 12*PAGESIZE 
#define TOTFRAMES PHISICALMEMORY/PAGESIZE
#define RESIDENTSETSIZE PHISICALMEMORY/(PAGESIZE*NUMPROCS)

extern char *base;
extern int framesbegin;
extern int idproc;
extern int systemframetablesize;
extern int ptlr;

extern struct SYSTEMFRAMETABLE *systemframetable;
extern struct PROCESSPAGETABLE *ptbr;


int getfreeframe();
int searchvirtualframe();
int getfifo();

int pagefault(char *vaddress)
{
    int i;
    int frame, vframe = -1; // vframe inicializado a -1
    long pag_a_expulsar;
    char buffer[PAGESIZE];
    int pag_del_proceso;

    // Calculamos la página a partir de la dirección que provocó el fallo
    pag_del_proceso = (long) vaddress >> 12;

    // Buscamos si la página está en un marco virtual del disco
    for (i = 0; i < ptlr->numpages; i++)
    {
        // Si encontramos la página en un marco virtual
        if (ptlr->ptentry[i].virtualframe == pag_del_proceso)
        {
            // Leemos el marco virtual al buffer
            vframe = ptlr->ptentry[i].virtualframe;
            readblock(buffer, vframe);
            
            // Marcamos la página como no presente y liberamos el marco virtual
            ptlr->ptentry[i].presente = 0;
            ptlr->ptentry[i].virtualframe = -1;
            break;
        }
    }

    // Contamos los marcos asignados al proceso
    i = countframesassigned();

    // Si el proceso ya ocupó todos sus marcos
    if(i >= RESIDENTSETSIZE)
    {
        // Buscamos una página para expulsar usando la política FIFO
        pag_a_expulsar = getfifo();

        // Marcamos la página como no presente
        ptlr->ptentry[pag_a_expulsar].presente = 0;

        // Si la página fue modificada, la guardamos en disco
        if (ptlr->ptentry[pag_a_expulsar].modificado)
        {
            saveframe(ptlr->ptentry[pag_a_expulsar].frame);
            ptlr->ptentry[pag_a_expulsar].modificado = 0;
        }

        // Buscamos un marco virtual en memoria secundaria
        vframe = searchvirtualframe();
        
        // Si no hay marcos virtuales en memoria secundaria, regresamos error
        if(vframe == -1)
        {
            return -1;
        }

        // Copiamos el marco a memoria secundaria, actualizamos la tabla de páginas y liberamos el marco de la memoria principal
        copyframe(ptlr->ptentry[pag_a_expulsar].frame, vframe);
        ptlr->ptentry[pag_a_expulsar].virtualframe = vframe;
        freeframe(ptlr->ptentry[pag_a_expulsar].frame);
    }

    // Buscamos un marco físico libre en el sistema
    frame = getfreeframe();
    
    // Si no hay marcos físicos libres en el sistema, regresamos error
    if(frame == -1)
    {
        return -1; // Regresar indicando error de memoria insuficiente
    }

    // Si la página estaba en memoria secundaria (vframe != -1)
    if (vframe != -1)
    {
        // Copiamos el contenido del buffer (marco virtual) al marco físico libre encontrado
        memcpy(systemframetable[frame].paddress, buffer, PAGESIZE);
    }

    // Poner el bit de presente en 1 en la tabla de páginas y asignar el marco físico
    ptlr->ptentry[pag_del_proceso].presente = 1;
    ptlr->ptentry[pag_del_proceso].frame = frame;

    return 1; // Regresar todo bien
}
