/**
 * @file rtam.h
 * @author Letter (NevermindZZT@gmail.com)
 * @brief rtos application manager
 * @version 0.1
 * @date 2023-10-14
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef __RTAM_H__
#define __RTAM_H__

#include "stddef.h"
#include "rtam_cfg.h"

#define RTAM_VERSION "0.0.1"

#ifndef RTAM_SECTION
    #if defined(__CC_ARM) || defined(__CLANG_ARM)
        #define RTAM_SECTION(x)                __attribute__((section(x), aligned(1)))
    #elif defined (__IAR_SYSTEMS_ICC__)
        #define RTAM_SECTION(x)                @ x
    #elif defined(__GNUC__)
        #define RTAM_SECTION(x)                __attribute__((section(x), aligned(1)))
    #else
        #define RTAM_SECTION(x)
    #endif
#endif

#ifndef RTAM_USED
    #if defined(__CC_ARM) || defined(__CLANG_ARM)
        #define RTAM_USED                      __attribute__((used))
    #elif defined (__IAR_SYSTEMS_ICC__)
        #define RTAM_USED                      __root
    #elif defined(__GNUC__)
        #define RTAM_USED                      __attribute__((used))
    #else
        #define RTAM_USED
    #endif
#endif

#define RTAPP_FLAGS_AUTO_START  (1 << 0)
#define RATPP_FLAGS_SERVICE     (1 << 1)


#define RTAPP_STATUS_STOPPED    0
#define RTAPP_STATUS_STARING    1
#define RTAPP_STATUS_RUNNING    2
#define RTAPP_STATUS_STOPPING   3

#define RTAPP_GET_RUN_STATUS(_status) ((_status) & 0x0F)

#define RTAPP_EXPORT(_name, _start, _stop, _get_status, _flags, _required, _info) \
    const RtApp RTAM_SECTION("rtApp") RTAM_USED rtApp##_name = { \
        .name = #_name, \
        .flags.value = _flags, \
        .start = _start, \
        .stop = _stop, \
        .getStatus = _get_status, \
        .required = _required, \
        .info = _info, \
    }



#define RTAPP_EXPORT_SIMPLE(_name, _flags) \
    const RtApp RTAM_SECTION("rtApp") RTAM_USED rtApp##_name = { \
        .name = #_name, \
        .flags.value = _flags, \
        .start = _name##Start, \
        .stop = _name##Stop, \
        .getStatus = _name##GetStatus, \
    }


/**
 * @brief application define
 * 
 */
typedef struct app_def {
    const char *name;
    union {
        struct {
            unsigned int autoStart: 1;
            unsigned int service: 1;
        };
        size_t value;
    } flags;
    int (*start)(void);
    int (*stop)(void);
    int (*getStatus)(void);
    const char **required;
    void *info;
} RtApp;


/**
 * @brief process define
 * 
 */
typedef struct rtam_process_def {
    struct app_def *app;
} RtProcess;


/**
 * @brief realtime os application manager, manager instance
 * 
 */
typedef struct rtam_manager {
    struct {
        RtApp *base;                                /**< application base address */
        size_t count;                               /**< application count */
    } apps;                                         /**< application list */
} RtManager;

void rtamInit(void);
int rtamLunchAndWait(const char *name, size_t timeout) ;
int rtamLaunch(const char *name);
int rtamTerminate(const char *name);
int rtamGetApps(RtApp **apps);

#endif
