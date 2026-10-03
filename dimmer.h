/*
 * dimmer.h - Digital phase-control dimmer synchronised to mains
 */
#ifndef DIMMER_H
#define DIMMER_H

#include "board_config.h"

/* Sets the dimming levels (0 = off, 255 = full). Linearised to RMS. */
void dimmer_set_levels(const unsigned char *data, unsigned char length);

/* Initialises I/O, zero crossing and timers. Interrupts must be enabled afterwards. */
void dimmer_init(const unsigned char *data, unsigned char length);

/* Call from the high priority ISR */
void dimmer_isr(void);

#endif
