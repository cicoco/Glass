#pragma once

#include <Arduino.h>
#include "app_state.h"

void input_handler_begin();
void input_handler_poll(InputCommandFlags *flags);

