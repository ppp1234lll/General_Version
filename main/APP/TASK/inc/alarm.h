#ifndef _ALARM_H_
#define _ALARM_H_

#include "./SYSTEM/sys/sys.h"


void alarm_task_function(void);
void alarm_elec_collection_param(void);			  
void alarm_net_collection_param(void);	 
void alarm_sensor_collection_param(void); 
 
uint8_t alarm_get_vlot_protec_status(void);
uint8_t alarm_get_current_protec_status(void);
uint8_t alarm_get_miu_protec_status(void);
uint8_t alarm_get_mcb_protec_status(void);

#endif
