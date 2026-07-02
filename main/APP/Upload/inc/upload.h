#ifndef _UPLOAD_H_
#define _UPLOAD_H_

#include "./SYSTEM/sys/sys.h"

typedef enum
{
    UPLOAD_MODE_NULL = 0,
    UPLOAD_MODE_LWIP = 1,
    UPLOAD_MODE_GPRS = 2,
} upload_mode_t;

#define UPLOAD_CHUNK_SIZE                   1024U
#define UPLOAD_DEFAULT_URL                  "/fnwlw/oss/deviceLog"

typedef struct
{
    uint8_t mode;
    uint8_t ip[4];
    char host[64];
    uint32_t port;
    char url[128];
} upload_param_t;

void upload_set_upload_mode(uint8_t mode);
uint8_t upload_get_mode_function(void);
void upload_set_server_config(const uint8_t *ip, const char *host, uint16_t port, const char *url);



#endif


