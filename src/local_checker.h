
#pragma once

#include <stdbool.h>

#include "options.h"

void local_checker_init(struct options* options);
void local_checker_end();
int local_checker_run();
