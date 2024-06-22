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
#include "string.h"

#if RTAM_WITH_LETTER_SHELL == 1
    #include "shell.h"
#endif /* RTAM_WITH_LETTER_SHELL == 1 */

#define RTAM_DEBUG(...) \
    if (RTAM_DEBUG_ENABLE) \
        RTAM_PRINT(__VA_ARGS__)

#if defined(__CC_ARM) || (defined(__ARMCC_VERSION) && __ARMCC_VERSION >= 6000000)
    extern const size_t rtApp$$Bas;
    extern const sizt_t rtApp$$Limit;
#elif defined(__ICCARM__) || defined(__ICCRX__)
    #pragma section="rtApp"
#elif defined(__GNUC__)
    extern const size_t _rt_app_start;
    extern const size_t _rt_app_end;
#endif

#define RTAM_INTERFACE_CHECK(app) \
    if ((app)->interface == NULL) { \
        RTAM_PRINT("app %s interface is NULL", (app)->name); \
        return -1; \
    }

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

    rtManager.processes = NULL;
    rtamLaunchAutoStart();
}


static RtProcess* rtamGetProcess(const char *name) {
    RtProcess *process = rtManager.processes;
    while (process != NULL) {
        if (strcmp(process->app->name, name) == 0) {
            return process;
        }
        process = process->next;
    }
    return NULL;
}


static RtProcess* rtamAddProcess(const char *name) {
    RtProcess *exsited = rtamGetProcess(name);
    if (exsited != NULL) {
        return exsited;
    }
    for (size_t i = 0; i < rtManager.apps.count; i++) {
        if (strcmp(rtManager.apps.base[i].name, name) == 0) {
            RtProcess *process = RTAM_MALLOC(sizeof(RtProcess));
            if (process == NULL) {
                return NULL;
            }
            process->app = &rtManager.apps.base[i];
            process->status.value = 0;
            process->next = rtManager.processes;
            rtManager.processes = process;
            return process;
        }
    }
    return NULL;

}


static void rtamRemoveProcess(const char *name) {
    RtProcess *process = rtManager.processes;
    RtProcess *prev = NULL;
    while (process != NULL) {
        if (strcmp(process->app->name, name) == 0) {
            if (prev == NULL) {
                rtManager.processes = process->next;
            } else {
                prev->next = process->next;
            }
            RTAM_FREE(process);
            return;
        }
        prev = process;
        process = process->next;
    }
}


static RtAppErr rtamStart(RtProcess *process, int timeout) {
    const RtAppInterface *interface = process->app->interface;
    RTAM_DEBUG("start app %s", process->app->name);
    if (process->status.started) {
        RTAM_PRINT("app %s is already started", process->app->name);
        return RTAM_OK;
    }
    if (interface->start) {
        if (process->status.processing) {
            RTAM_PRINT("app %s is processing", process->app->name);
            return RTAM_PROCESSING;
        }
        process->status.processing = 1;
        const RtAppDependencies *dependencies = process->app->dependencies;
        if (dependencies != NULL && dependencies->required != NULL) {
            for (size_t i = 0; dependencies->required[i] != NULL; i++) {
                RtAppErr err = rtamLaunch(dependencies->required[i]);
                if (err == RTAM_PROCESSING) {
                    RtProcess *requiredProcess = rtamGetProcess(dependencies->required[i]);
                    int time = 0;
                    while ((timeout > 0 && time < timeout) || timeout == -1) {
                        if (requiredProcess->status.started) {
                            break;
                        }
                        RTAM_DELAY(10);
                        time += 10;
                    }
                }
            }
        }
        if (dependencies != NULL && dependencies->conflicted != NULL) {
            for (size_t i = 0; dependencies->conflicted[i] != NULL; i++) {
                RtAppErr err = rtamTerminate(dependencies->conflicted[i]);
                if (err == RTAM_PROCESSING) {
                    RtProcess *conflictedProcess = rtamGetProcess(dependencies->conflicted[i]);
                    int time = 0;
                    while ((timeout > 0 && time < timeout) || timeout == -1) {
                        if (!conflictedProcess->status.started) {
                            break;
                        }
                        RTAM_DELAY(10);
                        time += 10;
                    }
                }
            }
        }
        RTAM_DEBUG("call app start %s", process->app->name);
        RtAppErr err = interface->start();
        if (err == RTAM_OK) {
            process->status.started = 1;
        }
        if (err != RTAM_PROCESSING) {
            process->status.processing = 0;
        }
        return err;
    } else {
        RTAM_PRINT("start interface not implemented for app: %s", process->app->name);
        return RTAM_FAIL;
    }
}


static RtAppErr rtamStop(RtProcess *process) {
    const RtAppInterface *interface = process->app->interface;
    if (!process->status.started) {
        RTAM_PRINT("app %s is already stoped", process->app->name);
        return RTAM_OK;
    }
    if (interface->stop) {
        if (process->status.processing) {
            RTAM_PRINT("app %s is processing", process->app->name);
            return RTAM_PROCESSING;
        }
        process->status.processing = 1;
        RtProcess *head = rtManager.processes;
        for (; head != NULL;) {
            const RtAppDependencies *dependencies = head->app->dependencies;
            if (dependencies != NULL && dependencies->required != NULL) {
                for (size_t i = 0; dependencies->required[i] != NULL; i++) {
                    if (strcmp(process->app->name, dependencies->required[i]) == 0) {
                        rtamStop(head);
                    }
                }
            }
            head = head->next;
        }
        RTAM_DEBUG("call app stop %s", process->app->name);
        RtAppErr err = interface->stop();
        if (err == RTAM_OK) {
            process->status.started = 0;
        }
        if (err != RTAM_PROCESSING) {
            process->status.processing = 0;
        }
        return err;
    } else {
        RTAM_PRINT("stop interface not implemented for app: %s", process->app->name);
        return RTAM_FAIL;
    }
}


static RtAppErr rtamSuspend(RtProcess *process) {
    const RtAppInterface *interface = process->app->interface;
    if (!process->status.resuming) {
        RTAM_PRINT("app %s is already suspend", process->app->name);
        return RTAM_OK;
    }
    if (interface->suspend) {
        if (process->status.processing) {
            RTAM_PRINT("app %s is processing", process->app->name);
            return RTAM_PROCESSING;
        }
        process->status.processing = 1;
        RTAM_DEBUG("call app suspend %s", process->app->name);
        RtAppErr err = interface->suspend();
        if (err == RTAM_OK) {
            process->status.resuming = 0;
        }
        if (err != RTAM_PROCESSING) {
            process->status.processing = 0;
        }
        return err;
    } else {
        process->status.resuming = 0;
        return RTAM_NOT_SUPPORT;
    }
}


static RtAppErr rtamResume(RtProcess *process) {
    const RtAppInterface *interface = process->app->interface;
    if (process->status.resuming) {
        RTAM_PRINT("app %s is already resuming", process->app->name);
        return RTAM_OK;
    }
    if (interface->resume) {
        if (process->status.processing) {
            RTAM_PRINT("app %s is processing", process->app->name);
            return RTAM_PROCESSING;
        }
        process->status.processing = 1;
        RTAM_DEBUG("call app resume %s", process->app->name);
        RtAppErr err = interface->resume();
        if (err == RTAM_OK) {
            process->status.resuming = 1;
        }
        if (err != RTAM_PROCESSING) {
            process->status.processing = 0;
        }
        return err;
    } else {
        process->status.resuming = 1;
        return RTAM_NOT_SUPPORT;
    }
}


static void rtamLaunchAutoStart(void) {
    for (size_t i = 0; i < rtManager.apps.count; i++) {
        if (rtManager.apps.base[i].flags.autoStart) {
            rtamLaunch(rtManager.apps.base[i].name);
        }
    }
}


RtAppErr rtamLaunch(const char *name) {
    RtProcess *process = rtamAddProcess(name);
    if (process == NULL) {
        RTAM_PRINT("app %s not found", name);
        return RTAM_FAIL;
    }
    RTAM_INTERFACE_CHECK(process->app);
    RtAppErr err = rtamStart(process, -1);
    if (err == RTAM_OK) {
        return rtamResume(process);
    }
    return err;
}
#if RTAM_WITH_LETTER_SHELL == 1
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
rtamLaunch, rtamLaunch, launch application\nrtamLaunch [name]);
#endif


RtAppErr rtamTerminate(const char *name) {
    RtProcess *process = rtamGetProcess(name);
    if (process == NULL) {
        RTAM_PRINT("app %s not running", name);
        return RTAM_FAIL;
    }
    rtamSuspend(process);
    RtAppErr err = rtamStop(process);
    if (err == RTAM_OK) {
        rtamRemoveProcess(name);
        return RTAM_OK;
    }
    return RTAM_FAIL;
}
#if RTAM_WITH_LETTER_SHELL == 1
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
rtamTerminate, rtamTerminate, terminate application\nrtamTerminate [name]);
#endif


RtAppErr rtamExit(const char *name) {
    RtProcess *process = rtamGetProcess(name);
    if (process == NULL) {
        RTAM_PRINT("app %s not running", name);
        return RTAM_FAIL;
    }
    if (process->app->flags.background) {
        return rtamSuspend(process);
    } else {
        RTAM_PRINT("app %s not support suspend, terminate it", name);
        return rtamStop(process);
    }
}
#if RTAM_WITH_LETTER_SHELL == 1
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0)|SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC),
rtamExit, rtamExit, exit application\nrtamExit [name]);
#endif


RtAppErr rtamSetStatus(const char *name, size_t status, bool set) {
    RtProcess *process = rtamGetProcess(name);
    if (process == NULL) {
        return RTAM_FAIL;
    }
    if (set) {
        process->status.value |= status;
    } else {
        process->status.value &= ~status;
    }
    process->status.processing = 0;
    return RTAM_OK;
}


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

    RtProcess *head = rtManager.processes;
    for (; head != NULL; head = head->next) {
    #if RTAM_WITH_LETTER_SHELL == 1
        if (shell != NULL) {
            shellPrint(shell,
                       "%-12s %-1s|%-1s|%-4s %s\n",
                       head->app->name,
                       head->status.started ? "R" : "D",
                       head->status.resuming ? "R" : "S",
                       head->status.processing ? "P" : "N",
                       head->app->flags.autoStart ? "auto" : "manual");
        } else {
    #endif /* RTAM_WITH_LETTER_SHELL == 1 */
            RTAM_PRINT("%-12s %-1s|%-1s|%-4s %s\n",
                       head->app->name,
                       head->status.started ? "R" : "D",
                       head->status.resuming ? "R" : "S",
                       head->status.processing ? "P" : "N",
                       head->app->flags.autoStart ? "auto" : "manual");
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
