#include "accelerometer.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/sensor.h>

LOG_MODULE_REGISTER(accel, CONFIG_LOG_DEFAULT_LEVEL);

#define ACCEL_STACK_SIZE 1024
#define ACCEL_PRIORITY 5

static const struct device *i2c_dev;
static struct accel_config driver_config;
static struct accel_data buffer_storage[2];  // Static buffer allocation
static struct accel_data *active_buffer;      // Currently being filled
static struct accel_data *process_buffer;     // Being processed
static uint16_t buffer_index;
static atomic_t running;
K_MUTEX_DEFINE(buffer_mutex);  // Single declaration using Zephyr macro

K_THREAD_STACK_DEFINE(accel_stack, ACCEL_STACK_SIZE);
static struct k_thread accel_thread_data;

/**
 * @brief Swaps the active and process buffers.
 * @warning This function is not thread-safe and must be called from within a locked context.
 */
static void swap_buffers_unsafe(void)
{
    struct accel_data *temp = active_buffer;
    active_buffer = process_buffer;
    process_buffer = temp;
}

static int read_lis2dh12_data(struct accel_data *data)
{
    struct sensor_value accel[3];
    int ret;

    ret = sensor_sample_fetch(i2c_dev);
    if (ret < 0) {
        return ret;
    }

    ret = sensor_channel_get(i2c_dev, SENSOR_CHAN_ACCEL_XYZ, accel);
    if (ret < 0) {
        return ret;
    }

    data->x[buffer_index] = sensor_value_to_float(&accel[0]);
    data->y[buffer_index] = sensor_value_to_float(&accel[1]);
    data->z[buffer_index] = sensor_value_to_float(&accel[2]);

    return 0;
}

static void accel_thread(void *p1, void *p2, void *p3)
{
    bool buffer_is_full = false;

    while (1) {
        if (atomic_get(&running)) {
            // Lock the mutex to safely access shared resources
            k_mutex_lock(&buffer_mutex, K_FOREVER);

            if (0 == read_lis2dh12_data(active_buffer)) {
                buffer_index++;
            }

            // Check if the buffer is full
            if (buffer_index >= ACCEL_BUFFER_SIZE) {
                active_buffer->samples = buffer_index;
                swap_buffers_unsafe(); // Swap buffers while still locked
                buffer_index = 0;
                buffer_is_full = true;
            }

            // Always unlock the mutex after the critical section
            k_mutex_unlock(&buffer_mutex);

            // If a buffer was filled, call the callback AFTER releasing the lock
            if (buffer_is_full) {
                driver_config.data_ready_cb(process_buffer);
                buffer_is_full = false;
            }

            // Calculate delay between samples
            // Use K_USEC for better precision, especially at high sample rates
            uint32_t delay_us = 1000000 / driver_config.sample_rate_hz;
            if (delay_us >= 1000) {
                // If delay is >= 1ms, use millisecond sleep
                k_msleep(delay_us / 1000);
            } else {
                // For high sample rates (>1000 Hz), use microsecond sleep
                k_usleep(delay_us);
            }
        } else {
            k_sleep(K_MSEC(100));
        }
    }
}


int accelerometer_init(const struct accel_config *config)
{
    if (!config || !config->data_ready_cb) {
        return -EINVAL;
    }

    i2c_dev = DEVICE_DT_GET(DT_ALIAS(accel0));
    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("Accelerometer device not ready");
        return -ENODEV;
    }

    driver_config = *config;
    
    // Initialize static buffers
    active_buffer = &buffer_storage[0];
    process_buffer = &buffer_storage[1];
    buffer_index = 0;
    atomic_set(&running, false);

    k_thread_create(&accel_thread_data, accel_stack,
                    K_THREAD_STACK_SIZEOF(accel_stack),
                    accel_thread,
                    NULL, NULL, NULL,
                    K_PRIO_PREEMPT(10), 0, K_NO_WAIT);

    LOG_INF("LIS2DH12 accelerometer initialized");
    return 0;
}

void accelerometer_start(void)
{
    atomic_set(&running, true);
}

void accelerometer_stop(void)
{
    atomic_set(&running, false);
}