/**
 * @file rtam_cfg.h
 * @author Letter (NevermindZZT@gmail.com)
 * @brief rtos application manager config
 * @version 0.1
 * @date 2023-10-22
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef __RTAM_CFG_H__
#define __RTAM_CFG_H__

#ifdef RTAM_CFG_USER
#include RTAM_CFG_USER
#endif

#ifndef RTAM_PRINT
#define RTAM_PRINT(...)                 printf(__VA_ARGS__)
#endif

#ifndef RTAM_WITH_LETTER_SHELL
#define RTAM_WITH_LETTER_SHELL          0
#endif

#ifndef RTAM_DELAY
#define RTAM_DELAY(ms)                  sleep(ms)
#endif

#endif
