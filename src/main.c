#include <zephyr/kernel.h>
#include <zephyr/drivers/usb/usb_dc.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/debug/thread_analyzer.h>
#include <zephyr/fatal.h>
#include <zephyr/sys/printk.h>
#include "accelerometer.h"
#include "NanoEdgeAI.h"


LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

// Early boot marker - if this doesn't print, panic is during C static init
static int __attribute__((constructor)) early_boot_test(void)
{
    // This runs BEFORE main() during C++ static initialization
    // If we don't see this, panic is even earlier
    return 0;
}

#define ML_BUFFER_SIZE (256u)
#define LEARNING_ITERATIONS (40u)

#define LED_PORT "GPIOB"  // Make sure the label is correct
#define LED_PIN 4         // Ensure this pin exists on your board


enum ml_state {
    ML_STATE_IDLE,
    ML_STATE_TRAINING,
    ML_STATE_INFERENCING
};

enum ml_events {
    EVT_DATA_READY = 0x1,
    EVT_TRAINING_COMPLETE = 0x2,
    EVT_ERROR = 0x4,
    EVT_STOP = 0x8
};



// Move ml_buffer to static memory to avoid stack overflow (256 floats = 1024 bytes)
static float ml_buffer[DATA_INPUT_USER*AXIS_NUMBER] __attribute__((aligned(4)));
static struct k_event ml_event;
static struct k_mutex ml_mutex;
static enum ml_state current_state = ML_STATE_IDLE;
static const struct accel_data *pending_buffer = NULL; // Pointer to accelerometer buffer

void ml_process_data(struct accel_data *buffer, size_t size);
enum ml_state ml_get_state(void);
void ml_init(void);

/**
 * @brief Copy and interleave accelerometer data into ML buffer
 * 
 * Takes data from the accelerometer buffer (separate X, Y, Z arrays) and
 * interleaves it into the ML buffer in the format expected by NanoEdge AI:
 * x1,y1,z1, x2,y2,z2, ..., x256,y256,z256
 * 
 * @note This function does NOT lock the mutex. Caller must ensure thread safety.
 * @return true if data was copied successfully, false if buffer is NULL
 */
static bool copy_accel_to_ml_buffer(void)
{
    // Capture pointer locally to avoid race condition with callback
    const struct accel_data *local_buffer = pending_buffer;
    
    if (local_buffer == NULL) {
        return false;
    }
    
    // Interleave the axes: for each sample, write x, y, z
    for (uint16_t i = 0; i < DATA_INPUT_USER; i++) {
        ml_buffer[i * 3 + 0] = local_buffer->x[i];
        ml_buffer[i * 3 + 1] = local_buffer->y[i];
        ml_buffer[i * 3 + 2] = local_buffer->z[i];
    }
    
    return true;
}

static void state_machine_step(uint32_t events)
{
    static uint8_t trainingIteration = 0;
    uint8_t similarity = 0;
    enum neai_state error_code = NEAI_OK;

    switch (current_state) {
        case ML_STATE_TRAINING:
            if (events & EVT_DATA_READY) {
                // Lock mutex for entire critical section (copy + ML processing)
                k_mutex_lock(&ml_mutex, K_FOREVER);
                
                // Copy data from accelerometer buffer to ML buffer
                if (!copy_accel_to_ml_buffer()) {
                    LOG_ERR("Failed to copy accelerometer data (NULL buffer)");
                    k_mutex_unlock(&ml_mutex);
                    break;
                }
                
                if (trainingIteration < MINIMUM_ITERATION_CALLS_FOR_EFFICIENT_LEARNING) {
                    error_code = neai_anomalydetection_learn(ml_buffer);
                    
                    if (error_code == NEAI_MINIMAL_RECOMMENDED_LEARNING_DONE) {
                        // Training complete early
                        current_state = ML_STATE_INFERENCING;
                        LOG_INF("Training complete (AI satisfied), entering inferencing");
                    } else if (error_code == NEAI_NOT_ENOUGH_CALL_TO_LEARNING) {
                        // Continue training
                        trainingIteration++;
                        LOG_INF("Training iteration %d", trainingIteration);
                    } else {
                        // Error occurred
                        LOG_ERR("Training error: %d", error_code);
                        current_state = ML_STATE_IDLE;
                    }
                } else {
                    current_state = ML_STATE_INFERENCING;
                    LOG_INF("Training complete (max iterations), entering inferencing");
                }
                
                k_mutex_unlock(&ml_mutex);
            }
            break;

        case ML_STATE_INFERENCING:
            if (events & EVT_DATA_READY) {
                // Lock mutex for entire critical section (copy + ML processing)
                k_mutex_lock(&ml_mutex, K_FOREVER);
                
                // Copy data from accelerometer buffer to ML buffer
                if (!copy_accel_to_ml_buffer()) {
                    LOG_ERR("Failed to copy accelerometer data (NULL buffer)");
                    k_mutex_unlock(&ml_mutex);
                    break;
                }
                
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
    // Fast, non-blocking callback - just signal that data is ready
    // The buffer pointer remains valid until next callback (double buffering in accelerometer.c)
    
    if (buffer == NULL) {
        LOG_ERR("Invalid buffer: NULL pointer");
        k_event_post(&ml_event, EVT_ERROR);
        return;
    }
    
    if (buffer->samples != ACCEL_BUFFER_SIZE) {
        LOG_ERR("Invalid buffer samples: %u (expected %u)", buffer->samples, ACCEL_BUFFER_SIZE);
        k_event_post(&ml_event, EVT_ERROR);
        return;
    }
    
    // Store pointer and signal main thread - no mutex, no copy, just atomic pointer write
    pending_buffer = buffer;
    
    // Signal event - main thread will do the heavy lifting
    k_event_post(&ml_event, EVT_DATA_READY);
}

// Custom fault handler to capture detailed fault information
void k_sys_fatal_error_handler(unsigned int reason, const struct arch_esf *esf)
{
    // Use printk for early boot panics (before logging is ready)
    printk("\n\n======================================\n");
    printk("FATAL ERROR OCCURRED!\n");
    printk("======================================\n");
    printk("Reason code: %u\n", reason);
    
    LOG_ERR("======================================");
    LOG_ERR("FATAL ERROR OCCURRED!");
    LOG_ERR("======================================");
    
    // Decode the reason
    LOG_ERR("Reason code: %u", reason);
    switch (reason) {
        case K_ERR_KERNEL_OOPS:
            LOG_ERR("Reason: K_ERR_KERNEL_OOPS");
            break;
        case K_ERR_KERNEL_PANIC:
            LOG_ERR("Reason: K_ERR_KERNEL_PANIC");
            break;
        case K_ERR_STACK_CHK_FAIL:
            LOG_ERR("Reason: STACK CHECK FAIL - Stack overflow detected!");
            break;
        case K_ERR_CPU_EXCEPTION:
            LOG_ERR("Reason: K_ERR_CPU_EXCEPTION - Hardware fault!");
            break;
        default:
            LOG_ERR("Reason: Unknown (%u)", reason);
            break;
    }
    
    // Print thread information
    struct k_thread *current = k_current_get();
    if (current) {
        LOG_ERR("Current thread: %p", current);
        #ifdef CONFIG_THREAD_NAME
        const char *name = k_thread_name_get(current);
        if (name) {
            LOG_ERR("Thread name: %s", name);
        } else {
            LOG_ERR("Thread name: (unnamed)");
        }
        #endif
    }
    
    LOG_ERR("======================================");
    
    // Flush logs to ensure they're sent over USB
    k_msleep(200);
    
    // Call default handler
    CODE_UNREACHABLE;
}

void main(void)
{
    /* Ensure main thread has FP context */
    arch_float_enable(true);
    enum neai_state error_code;
    

    k_event_init(&ml_event);
    k_mutex_init(&ml_mutex);
   
    k_msleep(100);

    //Initialize NanoEdge AI first, before starting accelerometer
    error_code = neai_anomalydetection_init();
        
    if (error_code != NEAI_OK) {
        LOG_ERR("NanoEdge AI initialization failed, error code: %d", error_code);
        LOG_ERR("System will continue in IDLE state without ML functionality");
        current_state = ML_STATE_IDLE;
    } else {
        LOG_INF("NanoEdge AI initialized successfully");
        current_state = ML_STATE_TRAINING;
    }
     k_msleep(500); // Give time for logs to flush

    LOG_INF("Current state: %d", current_state);

    struct accel_config config = {
        .sample_rate_hz = 500,
        .data_ready_cb = data_ready_handler
    };

    if (accelerometer_init(&config) != 0) {
        LOG_ERR("Failed to initialize accelerometer");
        return;
    }

    accelerometer_start();
    LOG_INF("Pulse started, current state: %d", current_state);

    while (1) {
        uint32_t events = k_event_wait(&ml_event, 
            EVT_DATA_READY | EVT_TRAINING_COMPLETE | EVT_ERROR | EVT_STOP,
            true, K_FOREVER);
        
        state_machine_step(events);
    }
}