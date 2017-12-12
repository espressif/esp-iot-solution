/**
 * Suggested tool for generating images : http://code.google.com/p/lcd-image-converter, 
 * The below images are 16bit 565RGB colors, using "const" to save RAM and store images in flash memory
**/

#ifndef _AWS_IOT_IMAGE_H_
#define _AWS_IOT_IMAGE_H_
#include <stdint.h>

extern const uint16_t esp_logo[137 * 26];
extern const uint16_t aws_logo[77*31];

#endif
