/*
 * firing_table.h - Linearisation table for phase-control dimming, 50 Hz mains.
 *
 * Index: dimming value (0..255, i.e. the DMX value).
 * Entry: firing slot (0..254) within the half cycle, which is split in 256
 *        slots (SLOTS in board_config.h). Smaller = fires earlier = brighter.
 * Entry 0 is unused: value 0 never fires (NEVER_FIRE_SLOT = 255 in dimmer.c).
 *
 * Goal: delivered RMS power proportional to the dimming value, P = value/255.
 * For a resistive load fired at angle alpha (0..pi over the half cycle):
 *
 *     P(alpha) = 1 - alpha/pi + sin(2*alpha) / (2*pi)
 *
 * For each value, alpha is the solution of P(alpha) = value/255 (found by
 * bisection). The firing slot of a given slot s happens at (s+1)/256 of the
 * half cycle, because the first compare match comes one slot after the window
 * start (ZC falling edge). So:
 *
 *     entry = round(alpha/pi * 256 - 1),  clamped to 0..254
 *
 * (value 255 -> slot 0, full power; 254 is the latest allowed slot, since
 * slot 255 is the end of the window and never fires).
 *
 * Max error vs. the ideal linear power: 0.37 percentage points (quantisation
 * only). Assumes a resistive load (lamps) and a 10 ms half cycle: for 60 Hz the
 * angles are the same, but SLOT_TICKS in board_config.h must follow MAINS_HZ.
 * Latency of the ZC edge / ISR is not compensated.
 */
#ifndef FIRING_TABLE_H
#define FIRING_TABLE_H

static const unsigned char firing_map[256] = {
    255, 233, 228, 224, 220, 218, 215, 213, 211, 209, 208, 206, 204, 203, 202, 200,
    199, 198, 197, 195, 194, 193, 192, 191, 190, 189, 188, 187, 186, 186, 185, 184,
    183, 182, 181, 180, 180, 179, 178, 177, 177, 176, 175, 174, 174, 173, 172, 172,
    171, 170, 170, 169, 168, 168, 167, 166, 166, 165, 164, 164, 163, 163, 162, 161,
    161, 160, 160, 159, 158, 158, 157, 157, 156, 155, 155, 154, 154, 153, 153, 152,
    152, 151, 150, 150, 149, 149, 148, 148, 147, 147, 146, 146, 145, 145, 144, 144,
    143, 142, 142, 141, 141, 140, 140, 139, 139, 138, 138, 137, 137, 136, 136, 135,
    135, 134, 134, 133, 133, 132, 132, 131, 131, 130, 130, 129, 129, 128, 128, 127,
    127, 126, 126, 125, 125, 124, 124, 123, 123, 122, 122, 121, 121, 120, 120, 119,
    119, 118, 118, 117, 117, 116, 116, 115, 115, 114, 114, 113, 113, 112, 112, 111,
    110, 110, 109, 109, 108, 108, 107, 107, 106, 106, 105, 105, 104, 104, 103, 102,
    102, 101, 101, 100, 100, 99, 99, 98, 97, 97, 96, 96, 95, 94, 94, 93,
    93, 92, 91, 91, 90, 90, 89, 88, 88, 87, 86, 86, 85, 84, 84, 83,
    82, 82, 81, 80, 80, 79, 78, 77, 77, 76, 75, 74, 74, 73, 72, 71,
    70, 69, 68, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 57, 56, 55,
    54, 52, 51, 50, 48, 46, 45, 43, 41, 39, 36, 34, 30, 26, 21, 0
};

#endif
