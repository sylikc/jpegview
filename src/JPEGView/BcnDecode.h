/*
 * 
 *
 */

#pragma once

#include <cstdint>

// Contains metadata for BCN decoding
typedef struct DecodeState {
    int xsize; // Width
    int ysize; // Height
    int xoff; // X Offset
    int yoff; // Y Offset
    
    int ystep = 0;
    
    int n; // BCN format
    char* pixel_format;
    int inNumChannels;
    int outNumChannels;
    int bytesPerBlock;

    int x; // Current x position
    int y; // Current y position
} DecodeState;

int ImagingBcnDecode(const uint8_t* src, uint8_t* dst, DecodeState *state, uint32_t bytes);