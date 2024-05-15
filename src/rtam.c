/**
 * @file rtam.c
 * @author Letter (NevermindZZT@gmail.com)
 * @brief rtos application manager
 * @version 0.1
 * @date 2023-10-14
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "rtam.h"
#include "rtam_cfg_user.h"
#include "string.h"

#if RTAM_WITH_LETTER_SHELL == 1
    #include "shell.h"
#endif /* RTAM_WITH_LETTER_SHELL == 1 */


#if defined(__CC_ARM) || (defined(__ARMCC_VERSION) && __ARMCC_VERSION >= 6000000)
    extern const size_t rtApp$$Bas;
    extern const sizt_t rtApp$$Limit;
#elif defined(__ICCARM__) || defined(__ICCRX__)
    #pragma section="rtApp"
#elif defined(__GNUC__)
    extern const size_t _rt_app_start;
    extern const size_t _rt_app_end;
#endif

/**
 * @brief single instance of manager
 * 
 */
static RtManager rtManager = {0};


static void rtamLaunchAutoStart(void);


/**
 * @brief get manager instance
 * 
 */
void rtamInit(void) {
#if defined(__CC_ARM) || (defined(__ARMCC_VERSION) && __ARMCC_VERSION >= 6000000)
    rtManager.apps.base = (RtApp *)(&rtApp$$Base);
    rtManager.apps.count = ((unsigned int)(&rtApp$$Limit)
                            - (unsigned int)(&rtApp$$Base))
                            / sizeof(RtApp);
#elif defined(__ICCARM__) || defined(__ICCRX__)
    rtManager.apps.base = (RtApp *)(__section_begin("rtApp"));
    rtManager.apps.count = ((unsigned int)(__section_end("rtApp"))
                            - (unsigned int)(__section_begin("rtApp")))
                            / sizeof(RtApp);
#elif defined(__GNUC__)
    rtManager.apps.base = (RtApp *)(&_rt_app_start);
    rtManager.apps.count = ((unsigned int)(&_rt_app_end)
                            - (unsigned int)(&_rt_app_start))
                            / sizeof(RtApp);
#else
    #error not supported compiler
#endif

    rtamLaunchAutoStart();
}


static int rtamStart(RtApp *app) {
    if (app->getStatus && RTAPP_GET_RUN_STATUS(app->getStatus()) != RTAPP_STATUS_STOPPED) {
        RTAM_PRINT("app %s is already running", app->name);
        return -1;
    }
    if (app->start) {
        if (app->required != NULL) {
            for (size_t i = 0; app->required[i] != NULL; i++) {
                if (rtamLunchAndWait(app->required[i], -1) != 0) {
                    RTAM_PRINT("app %s required app %s not running", app->name, app->required[i]);
                    return -1;
                }
            }
        }
        RTAM_PRINT("start app %s", app->name);
        return app->start();
    } else {
        return -1;
    }
}


static int rtamStop(RtApp *app) {
    if (app->stop) {
        app->stop();
        return 0;
    } else {
        return -1;
    }
}


static void rtamLaunchAutoStart(void) {
    for (size_t i = 0; i < rtManager.apps.count; i++) {
        if (rtManager.apps.base[i].flags.autoStart) {
            rtamStart(&rtManager.apps.base[i]);
        }
    }
}


int rtamLunchAndWait(const char *name, size_t timeout) {
    for (size_t i = 0; i < rtManager.apps.count; i++) {
        if (strcmp(rtManager.apps.base[i].name, name) == 0) {
            int (*getStatus)(void) = rtManager.apps.base[i].getStatus;
            if (getStatus && RTAPP_GET_RUN_STATUS(getStatus()) == RTAPP_STATUS_RUNNING) {
                return 0;
            }
            if ((getStatus && getStatus() == RTAPP_STATUS_STARING)
                || rtamStart(&rtManager.apps.base[i]) == 0) {
                int time = 0;
                RTAM_PRINT("wait for app %s start", name);
                while ((timeout > 0 && time < timeout) || timeout == -1) {
                    if (getStatus == NULL 
                        || RTAPP_GET_RUN_STATUS(getStatus()) == RTAPP_STATUS_RUNNING) {
                        return 0;
                    }
                    RTAM_DELAY(10);
                    time += 10;
                }
            }
        }
    }
    return -1;
}


int rtamLaunch(const char *name) {
    for (size_t i = 0; i < rtManager.apps.count; i++) {
        if (strcmp(rtManager.apps.base[i].name, name) == 0) {
            return rtamStart(&rtManager.apps.base[i]);
        }
    }
    RTAM_PRINT("app %s not found", name);
    return -1;
}
#if RTAM_WITH_LETTER_SHELL == 1
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
rtamLaunch, rtamLaunch, launch application\nrtamLaunch [name]);
#endif


int rtamTerminate(const char *name) {
    for (size_t i = 0; i < rtManager.apps.count; i++) {
        if (strcmp(rtManager.apps.base[i].name, name) == 0) {
            return rtamStop(&rtManager.apps.base[i]);
        }
    }
    return -1;
}
#if RTAM_WITH_LETTER_SHELL == 1
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
rtamTerminate, rtamTerminate, terminate application\nrtamTerminate [name]);
#endif


int rtamList(void) {
    #if RTAM_WITH_LETTER_SHELL == 1
    Shell *shell = shellGetCurrent();
    if (shell != NULL) {
        shellPrint(shell, "%-12s %-8s %s\n", "name", "status", "auto");
        shellPrint(shell, "%-12s %-8s %s\n", "----", "------", "----");
    } else {
    #endif /* RTAM_WITH_LETTER_SHELL == 1 */
    #if RTAM_WITH_LETTER_SHELL == 1
        RTAM_PRINT("%-12s %-8s %s", "name", "status", "auto");
        RTAM_PRINT("%-12s %-8s %s", "----", "------", "----");
    }
    #endif /* RTAM_WITH_LETTER_SHELL == 1 */


    for (size_t i = 0; i < rtManager.apps.count; i++) {
        int (*getStatus)(void) = rtManager.apps.base[i].getStatus;
        const char *status = "stopped";
        if (getStatus) {
            switch (RTAPP_GET_RUN_STATUS(getStatus())) {
                case RTAPP_STATUS_STARING:
                    status = "starting";
                    break;
                case RTAPP_STATUS_RUNNING:
                    status = "running";
                    break;
                case RTAPP_STATUS_STOPPING:
                    status = "stopping";
                    break;
                default:
                    break;
            }
        } else {
            status = "NaN";
        }
    #if RTAM_WITH_LETTER_SHELL == 1
        if (shell != NULL) {
            shellPrint(shell,
                       "%-12s %-8s %s\n",
                       rtManager.apps.base[i].name,
                       status,
                       rtManager.apps.base[i].flags.autoStart ? "auto" : "manual");
        } else {
    #endif /* RTAM_WITH_LETTER_SHELL == 1 */
            RTAM_PRINT("%-12s %-8s %s",
                       rtManager.apps.base[i].name,
                       status,
                       rtManager.apps.base[i].flags.autoStart ? "auto" : "manual");
    #if RTAM_WITH_LETTER_SHELL == 1
        }
    #endif /* RTAM_WITH_LETTER_SHELL == 1 */
    }
    return 0;
}
#if RTAM_WITH_LETTER_SHELL == 1
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC)|SHELL_CMD_DISABLE_RETURN,
rtamList, rtamList, list all applications);
#endif /* RTAM_WITH_LETTER_SHELL == 1 */


int rtamGetApps(RtApp **apps) {
    *apps = rtManager.apps.base;
    return rtManager.apps.count;
}
