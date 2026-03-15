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

#ifndef MOVING_AVERAGE_H
#define MOVING_AVERAGE_H

#include <stdint.h>
#include <stdbool.h>

#define MA_WINDOW_SIZE 15u

struct moving_average {
    uint8_t buffer[MA_WINDOW_SIZE];
    uint32_t sum;
    uint8_t index;
    bool is_filled;
};

void moving_average_init(struct moving_average *ma);
float moving_average_update(struct moving_average *ma, uint8_t new_value);
float moving_average_get(const struct moving_average *ma);

#endif /* MOVING_AVERAGE_H */