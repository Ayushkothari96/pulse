#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#include <zephyr/kernel.h>
#include "NanoEdgeAI.h"

// NanoEdge AI expects DATA_INPUT_USER samples, with AXIS_NUMBER values per sample
// For interleaved format: 256 samples × 3 axes = 768 total values
// But our accel_data struct stores each axis separately, so we need 256 samples per axis
#define ACCEL_BUFFER_SIZE (DATA_INPUT_USER)

struct accel_data {
    float x[ACCEL_BUFFER_SIZE];  // 256 samples for X axis
    float y[ACCEL_BUFFER_SIZE];  // 256 samples for Y axis
    float z[ACCEL_BUFFER_SIZE];  // 256 samples for Z axis
    uint16_t samples;  // Current number of samples in buffer (should be 256 when full)
};

struct accel_config {
    uint16_t sample_rate_hz;
    void (*data_ready_cb)(const struct accel_data *buffer);
};

int accelerometer_init(const struct accel_config *config);
void accelerometer_start(void);
void accelerometer_stop(void);

#endif /* ACCELEROMETER_H */