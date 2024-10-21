// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <string.h>

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer parameters;
    strcpy(parameters.serialPort, serialPort);
    if(!strcmp(role,"rx")){
        parameters.role = LlRx;
    }else if(!strcmp(role,"tx")){
        parameters.role = LlTx;
    }
    parameters.baudRate = baudRate;
    parameters.nRetransmissions = nTries;
    parameters.timeout = timeout;
    

    llopen(parameters);
    /* TODO: 
    open connection 
    if write
        start packet
        llwrite
        open filename
            create packets
            llwrite
        end packet
        llwrite
    if read
        llread
    close connection
    */
}
