/*
MIT License

Copyright (c) 2026 Ayush Kothari

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

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