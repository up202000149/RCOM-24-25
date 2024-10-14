// Application layer protocol implementation

#include "application_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
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
