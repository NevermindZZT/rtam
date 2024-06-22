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

#define RTAM_VERSION "0.1.0"

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

#define RTAPP_FLAG_AUTO_START   (1 << 0)
#define RTAPP_FLAG_SERVICE      (1 << 1)
#define RTAPP_FLAG_BACKGROUND   (1 << 2)


#define RTAPP_STATUS_STARTED    (1 << 0)
#define RTAPP_STATUS_RESUMING   (1 << 1)


#define RTAPP_GET_RUN_STATUS(_status) ((_status) & 0x0F)

#define RTAPP_EXPORT(_name, _interface, _flags, _dependencies, _info) \
    const RtApp RTAM_SECTION("rtApp") RTAM_USED rtApp##_name = { \
        .name = #_name, \
        .flags.value = _flags, \
        .interface = _interface, \
        .dependencies = _dependencies, \
        .info = _info, \
    }

typedef enum {
    RTAM_FAIL = -1,
    RTAM_OK = 0,
    RTAM_PROCESSING = 1,
    RTAM_NOT_SUPPORT = 2,
} RtAppErr;

typedef struct app_interface {
    int (*start)(void);
    int (*stop)(void);
    int (*suspend)(void);
    int (*resume)(void);
} RtAppInterface;

typedef struct app_dependencies {
    const char **required;
    const char **conflicted;
} RtAppDependencies;

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
            unsigned int background: 1;
        };
        size_t value;
    } flags;
    const struct app_interface *interface;
    const struct  app_dependencies *dependencies;
    const void *info;
} RtApp;


/**
 * @brief process define
 * 
 */
typedef struct rtam_process_def {
    struct app_def *app;
    union {
        struct {
            unsigned int started: 1;
            unsigned int resuming: 1;
            unsigned int processing: 1;
        };
        size_t value;
    } status;
    struct rtam_process_def *next;
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
    RtProcess *processes;                           /**< process list */
} RtManager;

void rtamInit(void);
RtAppErr rtamLaunch(const char *name);
RtAppErr rtamTerminate(const char *name);
RtAppErr rtamExit(const char *name);
RtAppErr rtamSetStatus(const char *name, size_t status, bool set);
int rtamGetApps(RtApp **apps);

#endif
