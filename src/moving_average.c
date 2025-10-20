#include "moving_average.h"

void moving_average_init(struct moving_average *ma)
{
    ma->sum = 0;
    ma->index = 0;
    ma->is_filled = false;
    
    for (int i = 0; i < MA_WINDOW_SIZE; i++) {
        ma->buffer[i] = 0;
    }
}

float moving_average_update(struct moving_average *ma, uint8_t new_value)
{
    // Subtract the oldest value from sum
    ma->sum -= ma->buffer[ma->index];
    
    // Add new value
    ma->buffer[ma->index] = new_value;
    ma->sum += new_value;
    
    // Update index
    ma->index = (ma->index + 1) % MA_WINDOW_SIZE;
    
    // Check if buffer is filled
    if (ma->index == 0) {
        ma->is_filled = true;
    }
    
    return moving_average_get(ma);
}

float moving_average_get(const struct moving_average *ma)
{
    uint8_t divisor = ma->is_filled ? MA_WINDOW_SIZE : ma->index;
    if (divisor == 0) {
        return 0.0f;
    }
    return (float)ma->sum / divisor;
}