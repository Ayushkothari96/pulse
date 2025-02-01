#include <zephyr/kernel.h>
#include <zephyr/drivers/usb/usb_dc.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include "accelerometer.h"
#include "NanoEdgeAI.h"


LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#define ML_BUFFER_SIZE (256u)
#define LEARNING_ITERATIONS (40u)

#define LED_PORT "GPIOB"  // Make sure the label is correct
#define LED_PIN 4         // Ensure this pin exists on your board


enum ml_state {
    ML_STATE_INIT,
    ML_STATE_TRAINING,
    ML_STATE_INFERENCING,
    ML_STATE_IDLE
};

enum ml_events {
    EVT_DATA_READY = 0x1,
    EVT_TRAINING_COMPLETE = 0x2,
    EVT_ERROR = 0x4,
    EVT_STOP = 0x8
};

static float ml_buffer[DATA_INPUT_USER];
static struct k_event ml_event;
static struct k_mutex ml_mutex;
static enum ml_state current_state = ML_STATE_INIT;

void ml_process_data(struct accel_data *buffer, size_t size);
enum ml_state ml_get_state(void);
void ml_init(void);

static void state_machine_step(uint32_t events)
{
    static uint8_t trainigIteration = 0;
    uint8_t similarity = 0;
    enum neai_state error_code = NEAI_OK;

    switch (current_state) {
        case ML_STATE_INIT:
                LOG_INF("Entering training state");
                error_code = neai_anomalydetection_init();
                if (error_code != NEAI_OK) {
                    /* This happens if the library works into a not supported board. */
                    LOG_ERR("The library failed to initialize, error code: %d", error_code);
                    current_state = ML_STATE_IDLE;
                } else {
                    current_state = ML_STATE_TRAINING;
                    LOG_INF("Engine analyzer started in IDLE state");
                }
            break;

        case ML_STATE_TRAINING:
            if (events & EVT_DATA_READY) {
                k_mutex_lock(&ml_mutex, K_FOREVER);
                if (trainigIteration < LEARNING_ITERATIONS) {
                    neai_anomalydetection_learn(ml_buffer);
                    trainigIteration++;
                    LOG_INF("Training iteration %d", trainigIteration);
                } else {
                    current_state = ML_STATE_INFERENCING;
                    LOG_INF("Training complete, entering inferencing");
                }
                k_mutex_unlock(&ml_mutex);
            }
            break;

        case ML_STATE_INFERENCING:
            if (events & EVT_DATA_READY) {
                k_mutex_lock(&ml_mutex, K_FOREVER);
                error_code = neai_anomalydetection_detect(ml_buffer, &similarity);
                if (error_code != NEAI_OK) {
                    LOG_ERR("Error in inferencing, error code: %d", error_code);
                } else {
                    LOG_INF("Similarity: %d", similarity);
                }
                k_mutex_unlock(&ml_mutex);
            }
            if (events & EVT_ERROR) {
                current_state = ML_STATE_IDLE;
                LOG_ERR("Error in inferencing, entering idle state");
            }
            break;

        case ML_STATE_IDLE:
            if (events & EVT_DATA_READY) {
                LOG_DBG("Ignoring data in idle state");
            }
            break;
    }
}

static void data_ready_handler(const struct accel_data *buffer)
{
    k_mutex_lock(&ml_mutex, K_FOREVER);
    // Copy data to ml_buffer
    for(uint16_t i = 0; i < DATA_INPUT_USER/3; i++) {
        ml_buffer[i*3] = buffer->x[i];
        ml_buffer[i*3 + 1] = buffer->y[i];
        ml_buffer[i*3 + 2] = buffer->z[i];
    }
    k_event_post(&ml_event, EVT_DATA_READY);
    k_mutex_unlock(&ml_mutex);
}

void main(void)
{
    k_event_init(&ml_event);
    k_mutex_init(&ml_mutex);

    struct accel_config config = {
        .sample_rate_hz = 100,
        .data_ready_cb = data_ready_handler
    };
    const struct device *led_dev;
    int ret;
    led_dev = device_get_binding(LED_PORT);
    if (led_dev == NULL) {
        LOG_ERR("Failed to bind to LED device");
        return;
    }

ret = gpio_pin_configure(led_dev, LED_PIN, GPIO_OUTPUT_LOW);
    if (ret < 0) {
        LOG_ERR("Failed to configure LED pin");
        return;
    }

    // Blink LED for 5 seconds
    for (int i = 0; i < 100; i++) {
        gpio_pin_toggle(led_dev, LED_PIN);
        k_sleep(K_MSEC(500));
    }

     ret = usb_enable(NULL);
    if (ret != 0) {
        LOG_ERR("Failed to enable USB");
        return;
    }

    if (accelerometer_init(&config) != 0) {
        LOG_ERR("Failed to initialize accelerometer");
        return;
    }

    accelerometer_start();
    LOG_INF("Engine analyzer started in INIT state");

    while (1) {
        uint32_t events = k_event_wait(&ml_event, 
            EVT_DATA_READY | EVT_TRAINING_COMPLETE | EVT_ERROR | EVT_STOP,
            false, K_FOREVER);
        
        state_machine_step(events);
    }
}