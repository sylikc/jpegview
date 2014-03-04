#pragma once

#ifdef _WIN64
const unsigned int MAX_IMAGE_PIXELS = 1024 * 1024 * 300;
#else
const unsigned int MAX_IMAGE_PIXELS = 1024 * 1024 * 100;
#endif

// That must not be bigger than 65535 due to internal limitations
const unsigned int MAX_IMAGE_DIMENSION = 65535;