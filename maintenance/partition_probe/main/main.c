// SPDX-License-Identifier: MIT
// Read-only maintenance firmware. No flash/partition/NVS/SD writes.
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include "esp_flash.h"
#include "esp_app_desc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
static char json[16000];
static size_t used;
static void add(const char *format, ...){va_list args;va_start(args,format);int n=vsnprintf(json+used,sizeof(json)-used,format,args);va_end(args);if(n>0&&n<(int)(sizeof(json)-used))used+=(size_t)n;}
static void safe(char *out,const char *in,size_t maximum){size_t i;for(i=0;i<maximum&&in[i];++i){unsigned char c=in[i];out[i]=isalnum(c)||c=='_'||c=='-'||c=='.'?(char)c:'_';}out[i]=0;}
static void report(void){
 uint32_t bytes=0;esp_flash_get_size(esp_flash_default_chip,&bytes);const esp_partition_t *running=esp_ota_get_running_partition();const esp_partition_t *next=esp_ota_get_next_update_partition(NULL);char name[33],label[17],version[33];used=0;
 safe(label,running?running->label:"none",16);add("{\"format\":1,\"flash_size\":%lu,\"running\":\"%s\",\"running_address\":%lu,",(unsigned long)bytes,label,(unsigned long)(running?running->address:0));safe(label,next?next->label:"none",16);add("\"next_ota\":\"%s\",\"partitions\":[",label);
 esp_partition_iterator_t it=esp_partition_find(ESP_PARTITION_TYPE_ANY,ESP_PARTITION_SUBTYPE_ANY,NULL);unsigned count=0;
 while(it&&count<32){const esp_partition_t *p=esp_partition_get(it);safe(label,p->label,16);name[0]=0;version[0]=0;if(p->type==ESP_PARTITION_TYPE_APP){esp_app_desc_t desc;if(esp_ota_get_partition_description(p,&desc)==ESP_OK){safe(name,desc.project_name,32);safe(version,desc.version,32);}}
  add("%s{\"label\":\"%s\",\"type\":%u,\"subtype\":%u,\"address\":%lu,\"size\":%lu,\"encrypted\":%s,\"project\":\"%s\",\"version\":\"%s\"}",count?",":"",label,p->type,p->subtype,(unsigned long)p->address,(unsigned long)p->size,p->encrypted?"true":"false",name,version);++count;it=esp_partition_next(it);
 }
 if(it)esp_partition_iterator_release(it);add("]}");printf("\nTAB5_LAYOUT_JSON=%s\n",json);fflush(stdout);
}
void app_main(void){for(;;){report();vTaskDelay(pdMS_TO_TICKS(10000));}}
