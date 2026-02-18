#ifndef BUTTON_BSP_H
#define BUTTON_BSP_H
#include <stdio.h>
#include <stdbool.h>  
#include "buttons/multi_button.h"
#include "board/board_pins.h"


#define BOOT_KEY_PIN     BOARD_BUTTON_BOOT

#define Button_PIN1   BOOT_KEY_PIN

extern PressEvent BOOT_KEY_State;    

void button_Init(void);

#endif
