#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#include <zephyr/kernel.h>
#include "NanoEdgeAI.h"

#define ACCEL_BUFFER_SIZE (DATA_INPUT_USER/3)

struct accel_data {
    float x[ACCEL_BUFFER_SIZE];
    float y[ACCEL_BUFFER_SIZE];
    float z[ACCEL_BUFFER_SIZE];
    uint16_t samples;  // Current number of samples in buffer
};

struct accel_config {
    uint16_t sample_rate_hz;
    void (*data_ready_cb)(const struct accel_data *buffer);
};

int accelerometer_init(const struct accel_config *config);
void accelerometer_start(void);
void accelerometer_stop(void);

#endif /* ACCELEROMETER_H */