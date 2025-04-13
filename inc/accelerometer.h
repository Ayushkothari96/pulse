#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#include <zephyr/kernel.h>
#include "NanoEdgeAI.h"

struct accel_sample {
    float x;
    float y;
    float z;
};

struct accel_config {
    uint16_t sample_rate_hz;
    void (*data_ready_cb)(const struct accel_sample *buffer);
};

int accelerometer_init(const struct accel_config *config);
void accelerometer_start(void);
void accelerometer_stop(void);

#endif /* ACCELEROMETER_H */