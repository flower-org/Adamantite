#include "log.h"
#include <stdio.h>
#include <stdarg.h>
#include "usb_fs.h"

void Log_Printf(const char *format, ...)
{
    char buffer[256];
    va_list args;
    
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (len > 0) {
        if (len > sizeof(buffer)) {
            len = sizeof(buffer);
        }
        USB_FS_EnqueueMessage((const uint8_t *)buffer, (uint16_t)len);
    }
}