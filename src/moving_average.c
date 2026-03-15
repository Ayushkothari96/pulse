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