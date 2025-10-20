#ifndef BUZZER_H
#define BUZZER_H

#include <zephyr/kernel.h>

int buzzer_init(void);
int buzzer_set_frequency(uint32_t freq_hz);
void buzzer_stop(void);

#endif /* BUZZER_H */