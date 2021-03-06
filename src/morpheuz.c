/* 
 * Morpheuz Sleep Monitor
 *
 * Copyright (c) 2013-2015 James Fowler
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "pebble.h"
#include "morpheuz.h"
#include "language.h"
#include "analogue.h"

static uint16_t biggest_movement_in_one_minute = 0;

/*
 * Set the on-screen status text
 */
EXTFN void set_smart_status() {
  static char status_text[TIME_RANGE_LEN];
  copy_alarm_time_range_into_field(status_text, sizeof(status_text));
  set_smart_status_on_screen(get_config_data()->smart, status_text);
  analogue_set_smart_times();
}

/*
 * Do something with samples every minute
 */
EXTFN uint16_t every_minute_processing() {
  uint16_t last_biggest = biggest_movement_in_one_minute;
  power_nap_check(biggest_movement_in_one_minute);
  server_processing(biggest_movement_in_one_minute);
  biggest_movement_in_one_minute = 0;
  return last_biggest;
}

/*
 * Store our samples from each group until we have two minute's worth
 */
static void store_sample(uint16_t biggest) {
  revive_clock_on_movement(biggest);
  if (biggest > biggest_movement_in_one_minute)
    biggest_movement_in_one_minute = biggest;
}

/*
 * Can't be bothered to play with negative numbers
 */
static uint16_t scale_accel(int16_t val) {
  int16_t retval = 4000 + val;
  if (retval < 0)
    retval = 0;
  return retval;
}

/*
 * Process deviation for an access
 */
static void do_axis(int16_t val, uint16_t *biggest, uint32_t avg) {
  uint16_t val_scale = scale_accel(val);
  if (val_scale < avg)
    val_scale = avg - val_scale;
  else
    val_scale -= avg;
  if (val_scale > *biggest)
    *biggest = val_scale;
}

/*
 * Process accelerometer data
 */
static void accel_data_handler(AccelData *data, uint32_t num_samples) {

  // Average the data
  uint32_t avg_x = 0;
  uint32_t avg_y = 0;
  uint32_t avg_z = 0;
  AccelData *dx = data;
  for (uint32_t i = 0; i < num_samples; i++, dx++) {
    // If vibe went off then discount everything - we're only loosing a 2.5 second set of samples, better than an
    // unwanted spike
    if (dx->did_vibrate) {
      return;
    }
    avg_x += scale_accel(dx->x);
    avg_y += scale_accel(dx->y);
    avg_z += scale_accel(dx->z);
  }

  avg_x /= num_samples;
  avg_y /= num_samples;
  avg_z /= num_samples;

  // Work out deviations
  uint16_t biggest = 0;
  AccelData *d = data;
  for (uint32_t i = 0; i < num_samples; i++, d++) {
    do_axis(d->x, &biggest, avg_x);
    do_axis(d->y, &biggest, avg_y);
    do_axis(d->z, &biggest, avg_z);
  }

  store_sample(biggest);
}

/*
 * Initialise comms and accelerometer
 */
EXTFN void init_morpheuz() {

  open_comms();

  // Accelerometer
  accel_data_service_subscribe(25, accel_data_handler);
  accel_service_set_sampling_rate(ACCEL_SAMPLING_10HZ);

  // Set the smart status
  set_smart_status();
}



