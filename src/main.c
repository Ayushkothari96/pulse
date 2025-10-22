#include <zephyr/kernel.h>
#include <zephyr/drivers/usb/usb_dc.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/debug/thread_analyzer.h>
#include <zephyr/fatal.h>
#include <zephyr/sys/printk.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/arch/cpu.h>
#include <string.h>
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

// Statistics for shell access
static uint8_t training_iteration = 0;
static uint8_t last_similarity = 0;
static bool similarity_valid = false;
static uint16_t accel_sample_rate = 500;

// Deferred work for ML reinitialization
static struct k_work ml_reinit_work;
static volatile bool ml_reinit_success = false;
static volatile bool ml_reinit_done = false;

// USB Console thread
#define CONSOLE_STACK_SIZE 512
K_THREAD_STACK_DEFINE(console_stack, CONSOLE_STACK_SIZE);
static struct k_thread console_thread;

// Forward declarations
void ml_process_data(struct accel_data *buffer, size_t size);
enum ml_state ml_get_state(void);
void ml_init(void);
static void usb_console_thread(void *p1, void *p2, void *p3);

/**
 * @brief Deferred work handler to reinitialize NanoEdge AI library
 * 
 * This runs in the system work queue context, not in the shell thread,
 * so it won't block the USB interface.
 */
static void ml_reinit_work_handler(struct k_work *work)
{
    LOG_INF("Starting NanoEdge AI reinitialization...");
    
    enum neai_state error_code = neai_anomalydetection_init();
    
    if (error_code == NEAI_OK) {
        k_mutex_lock(&ml_mutex, K_FOREVER);
        training_iteration = 0;
        similarity_valid = false;
        current_state = ML_STATE_TRAINING;  // Automatically start training
        k_mutex_unlock(&ml_mutex);
        
        ml_reinit_success = true;
        LOG_INF("NanoEdge AI reinitialized successfully - training started");
    } else {
        ml_reinit_success = false;
        LOG_ERR("NanoEdge AI reinitialization failed: %d", error_code);
    }
    
    ml_reinit_done = true;
}

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
                
                if (training_iteration < MINIMUM_ITERATION_CALLS_FOR_EFFICIENT_LEARNING) {
                    error_code = neai_anomalydetection_learn(ml_buffer);
                    
                    if (error_code == NEAI_MINIMAL_RECOMMENDED_LEARNING_DONE) {
                        // Training complete early - model already trained
                        LOG_WRN("Model already trained! Power cycle to retrain from scratch.");
                        current_state = ML_STATE_INFERENCING;
                        LOG_INF("Training complete (AI satisfied), entering inferencing");
                    } else if (error_code == NEAI_NOT_ENOUGH_CALL_TO_LEARNING) {
                        // Continue training
                        training_iteration++;
                        LOG_INF("Training iteration %d", training_iteration);
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
                    last_similarity = similarity;
                    similarity_valid = true;
                    LOG_INF("Similarity: %d%%", similarity);
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
    
    // Initialize deferred work for ML reinitialization
    k_work_init(&ml_reinit_work, ml_reinit_work_handler);
   
    k_msleep(100);

    // Wait for USB to initialize
    LOG_INF("Waiting for system initialization...");
    k_msleep(2000);

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

    LOG_INF("Current state: %d", current_state);

    // Initialize accelerometer
    LOG_INF("Initializing accelerometer...");
    struct accel_config config = {
        .sample_rate_hz = 500,
        .data_ready_cb = data_ready_handler
    };

    if (accelerometer_init(&config) != 0) {
        LOG_ERR("Failed to initialize accelerometer");
        LOG_ERR("System will continue without accelerometer data");
        // Don't return - allow shell to work for debugging
        current_state = ML_STATE_IDLE;
    } else {
        accelerometer_start();
        accel_sample_rate = config.sample_rate_hz;
        LOG_INF("Accelerometer started successfully");
    }

    LOG_INF("Pulse device ready, current_state: %d", current_state);
    
    // Start USB console command thread
    // Priority 10 (lower than accelerometer priority 10, runs when idle)
    k_tid_t console_tid = k_thread_create(&console_thread, console_stack,
                                          K_THREAD_STACK_SIZEOF(console_stack),
                                          usb_console_thread,
                                          NULL, NULL, NULL,
                                          K_PRIO_PREEMPT(12), 0, K_NO_WAIT);
    k_thread_name_set(console_tid, "usb_console");

    while (1) {
        uint32_t events = k_event_wait(&ml_event, 
            EVT_DATA_READY | EVT_TRAINING_COMPLETE | EVT_ERROR | EVT_STOP,
            true, K_FOREVER);
        
        state_machine_step(events);
    }
}

/* ============================================================================
 * Simple USB Command Handler (Minimal Memory Footprint)
 * ============================================================================ */

static const char *ml_state_to_string(enum ml_state state)
{
    switch (state) {
        case ML_STATE_IDLE: return "IDLE";
        case ML_STATE_TRAINING: return "TRAINING";
        case ML_STATE_INFERENCING: return "INFERENCING";
        default: return "UNKNOWN";
    }
}

/**
 * Command: STATUS
 * Returns all device information and current state
 */
static void cmd_status(void)
{
    printk("\n=== PULSE DEVICE STATUS ===\n");
    printk("Device:      Pulse v1.0.0\n");
    printk("Board:       nucleo_l412rb_p\n");
    printk("Uptime:      %lld ms\n", k_uptime_get());
    printk("\n");
    printk("ML State:    %s\n", ml_state_to_string(current_state));
    printk("Training:    %d/%d iterations\n", 
           training_iteration, MINIMUM_ITERATION_CALLS_FOR_EFFICIENT_LEARNING);
    
    if (similarity_valid) {
        printk("Similarity:  %d%%", last_similarity);
        if (last_similarity >= 90) {
            printk(" (NORMAL)\n");
        } else if (last_similarity >= 70) {
            printk(" (WARNING)\n");
        } else {
            printk(" (ANOMALY!)\n");
        }
    } else {
        printk("Similarity:  N/A\n");
    }
    
    printk("\n");
    printk("Accel Rate:  %d Hz\n", accel_sample_rate);
    printk("Accel Buf:   %d samples\n", ACCEL_BUFFER_SIZE);
    printk("===========================\n\n");
}

/**
 * Command: RESET
 * Reinitializes the ML model for fresh training
 */
static void cmd_reset(void)
{
    printk("\nReinitializing ML model...\n");
    
    // Reset flags
    ml_reinit_done = false;
    ml_reinit_success = false;
    
    // Submit work to system work queue
    k_work_submit(&ml_reinit_work);
    
    // Wait for completion (with timeout)
    int timeout_ms = 5000;
    int elapsed_ms = 0;
    
    while (!ml_reinit_done && elapsed_ms < timeout_ms) {
        k_msleep(100);
        elapsed_ms += 100;
    }
    
    if (!ml_reinit_done) {
        printk("ERROR: Timeout waiting for reinitialization!\n\n");
        return;
    }
    
    if (ml_reinit_success) {
        printk("SUCCESS: ML model reinitialized\n");
        printk("Training started automatically\n");
        printk("Monitor progress with STATUS command\n\n");
    } else {
        printk("ERROR: Reinitialization failed\n\n");
    }
}

/**
 * USB Console Command Parser Thread
 * Handles simple text commands from USB console
 */
static void usb_console_thread(void *p1, void *p2, void *p3)
{
    const struct device *uart_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
    char cmd_buf[32];
    int idx = 0;
    uint8_t c;
    
    if (!device_is_ready(uart_dev)) {
        printk("Console device not ready!\n");
        return;
    }
    
    // Wait a bit for USB to enumerate
    k_msleep(1000);
    
    printk("\n\n");
    printk("=======================================\n");
    printk("  PULSE DEVICE - USB CONSOLE\n");
    printk("=======================================\n");
    printk("Commands:\n");
    printk("  STATUS - Show device status\n");
    printk("  RESET  - Reset ML model for retraining\n");
    printk("=======================================\n\n");
    
    while (1) {
        // Poll for character
        if (uart_poll_in(uart_dev, &c) == 0) {
            // Handle backspace
            if (c == '\b' || c == 0x7F) {
                if (idx > 0) {
                    idx--;
                    printk("\b \b");  // Erase character on screen
                }
                continue;
            }
            
            // Echo character
            if (c >= 32 && c < 127) {
                printk("%c", c);
                if (idx < sizeof(cmd_buf) - 1) {
                    cmd_buf[idx++] = c;
                }
            }
            
            // Handle enter
            if (c == '\r' || c == '\n') {
                printk("\n");
                cmd_buf[idx] = '\0';
                
                // Skip empty commands
                if (idx == 0) {
                    continue;
                }
                
                // Convert to uppercase for case-insensitive matching
                for (int i = 0; i < idx; i++) {
                    if (cmd_buf[i] >= 'a' && cmd_buf[i] <= 'z') {
                        cmd_buf[i] -= 32;
                    }
                }
                
                // Parse and execute command
                if (strcmp(cmd_buf, "STATUS") == 0) {
                    cmd_status();
                } else if (strcmp(cmd_buf, "RESET") == 0) {
                    cmd_reset();
                } else {
                    printk("Unknown command: %s\n", cmd_buf);
                    printk("Type STATUS or RESET\n\n");
                }
                
                // Reset for next command
                idx = 0;
            }
        } else {
            // No data, sleep briefly to avoid busy loop
            k_msleep(10);
        }
    }
}
