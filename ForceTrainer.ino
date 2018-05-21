// Graph output from Neurosky chip set, including what is provided
// by Star Wars Force Trainer.
// Authors: Brianna Roskind and Jim Roskind
//
// WIRING INSTRUCTIONS
//
// The "T" terminal on the Neurosky board should be connected to the
// "Rx" (Pin 0) on the Auduino board, through a 220 ohm resistor. The
// ground of the Neurosky board should be connected to the ground of
// the Arduino board.
//
// 10 momementary contact (pushbutton) switches should be used to
// provide values to Arduino pins 2 - 11.  We suggest that the signal
// wires should be fully connected to ground via 10K ohm resistors,
// and intermittently "pulled up" to 3.3v via the pushshbotton
// switches. The switches connecting to pins 2-11 should be labeled
// respectively:
//
//      "Delta"
//      "Theta"
//      "L-alpha"
//      "H-alpha"
//      "L-beta"
//      "H-beta"
//      "L-gamma"
//      "M-gamma"
//      "Meditation"
//      "Attention"
//
// PROGRAM EXECUTION INSTRUCTIONS
//
// The program should be run while connected to the Arduino IDE Serial
// Plotter.  By default, it starts up graphing only the Attention and
// Meditation values.  The pushbutton switches can be use to taggle
// graphing of any/all of the 8 power bands (as well as Attention,
// Meditation).
//
//
// DEBUGGING FEATURES AND INSTRUMENTATION
//
// The basic electrical connection from the switches can be validated
// by watching the Arduino on-board LED (connected to Pin 13).  It
// should illuminate when any of the pushbuttons are depressed.
//
// To debug the data printing for the graphics, switch to using the
// Arduino IDE Serial Terminal (instead of the Serial Plotter).  You
// should then see about 10 lines of 11 data values being printed once
// per second. Pressing each of the buttons should cause the
// corresponding value to display, or be replaced by value 0 (which
// graphs invisibly along the X-axis in a Serial Plotter). The last
// interger plotted on each line is the Signal Strength, which is
// always enabled.
//
// If there are any errors (loss of sync? unexpected data?), then
// error counts will be printed on the terminal.
//
// To see the data with tags (rather than a vector of 11 integers) in
// the Serial Terminal window, hold down any three pushbottons at the
// same time, and the program will toggle into a text mode, listing
// names and values, rather than just a vector.
//
// To see additional performance statistics, depress any 2 buttons at
// the same time, and several stats will be printed.  This information
// is most helpful IF you significantly modify the program. The stats
// include the duration of time between calls to ProcessBytes(), which
// MUST be called every 10ms to ensure that no bytes are lost (for
// lack of buffer space).  The stats also show the maximum number of
// bytes ever observed on entry to ProcessBytes().  With a buffer of
// 63 bytes, this metric should always be well under 63, or else there
// is a risk that bytes were lost.  Note that byte-loss almost always
// induces various errors, which are printed whenever you are running
// in Auduino IDE Serial Terminal mode.

//------------------------------------------------------
// Select output for Serial Terminal, vs Serial Plotter.  Toggle
// selection during a run by holding down 3 buttons.
bool plot_it = true;
const int LED_PIN = 13;  // Light when any button is pressed.
// Currently, pressing 2 buttons at same time causes stats to print
// Pressing 3 buttons at same time taggles Plot vs Terminal output
// modes.
#define PUSH_BUTTON_COUNT 10

//------------------------------------------------------
// 8-bit Data extracted from payload messages.
// 256 means no new value. We don't currently plat this data,
// but it is parsed out to simplify extending the code, and any
// values received *ARE* printed in Terminal (non-plotting) mode.
uint16_t heart_rate = 256;
uint16_t raw_wave_8_bit = 256;
uint16_t zero_raw_marker = 256;  // Always reads zero.

//------------------------------------------------------
#define EEG_VALUE_COUNT 8  // Number of EEG power values.
// We keep several other values in the array, after
// the eeg_values. Define what index is used for the
// extra values.
enum extra_values {
  MEDITATION_INDEX = EEG_VALUE_COUNT,
  ATTENTION_INDEX,
  SIGNAL_STRENGTH_INDEX,
  // Add indicies before here---
  DATA_ARRAY_SIZE
};
// We could use PROGMEM for these string arrays, but it is painful
// to manually assign values into the array.  Do it (using F()) if
// you are desperate for "dynamic memory."
const char* const data_value_names[DATA_ARRAY_SIZE] = {
  "Delta",
  "Theta",
  "L-alpha",
  "H-alpha",
  "L-beta",
  "H-beta",
  "L-gamma",
  "M-gamma",
  "Meditation",
  "Attention",
  "Signal Strength",
};
uint32_t data_values[DATA_ARRAY_SIZE] = {0};
// Signal when there are new eeg_values to print/display.
uint8_t eeg_values_loaded_for_print = 0;
uint8_t eeg_values_loaded_for_plot = 0;

//------------------------------------------------------
uint32_t total_plotting_height = (int32_t)100 *
                                 1000 * 1000;
// Scaled values, ready to plot.
uint32_t data_snapshot[DATA_ARRAY_SIZE] = {0};
// Largest and smallest data_values for scaling.
uint32_t one_plus_max_data_snapshot[DATA_ARRAY_SIZE] = {0};
uint32_t min_data_snapshot[DATA_ARRAY_SIZE] = {0};
// Select values to plot with an array of bools, toggled by
// corresponding pushbuttons. (bit array would be more compact).
bool need_to_plot_value[DATA_ARRAY_SIZE] = {false};

//------------------------------------------------------
// We can't afford the time it takes to print errors as we see them,
// but we can count them, and print a summary later.
enum error_indicies {
  DISCARDED_BYTE_ERROR,
  MISSING_SYNC_ERROR,
  BAD_PAYLOAD_LENGTH_ERROR,
  BAD_CHECKSUM_ERROR,
  EEG_BAD_LENGTH_ERROR,
  RAW_PAYLOAD_TOO_BIG_ERROR,
  UNRECOGNIZED_CODE_ERROR,
  READ_PAST_PAYLOAD_ERROR,
  SKIPPED_EEG_DATA_ERROR,
  // Add errors above here.
  ERROR_ARRAY_SIZE
};
uint16_t errors[ERROR_ARRAY_SIZE] = {0};
const char* const error_names[ERROR_ARRAY_SIZE] = {
  // List of byte processing errors.
  "Discarded byte",  // Total ignored for all reasons.
  "Missing Sync",  // Characters before SYN.
  "Bad Payload Length",  // Payload length error.
  "Bad Checksum",  // Mismatched checksum.
  // List of payload processing errors.
  "EEG Bad Data Length",  // Power band length wasn't 24 == 3 * 8.
  "Raw Payload Too Big",  // During payload processing.
  "Urecognized Field Code",  // During payload processing.
  "Read Past Payload End",   // At end of payload processing.
  "Skipped EEG Data",  // Too slow when printing/plotting
};

//------------------------------------------------------
// Performance statistics.  Push any 3 buttons to cause them to be
// printed (though you'll only see them in a Serial Monitor).
uint8_t max_available_bytes_to_process = 0;  // Worry when near 63!
uint16_t max_duration_between_byte_processing = 0;
uint32_t payload_process_count = 0;
uint32_t payload_byte_count = 0;
uint32_t raw_code_count = 0;  // Raw wave data in payload.
uint32_t start_time_millis = 0;
// State counter, for printing the above stats one at a time.
uint8_t optional_stats_print_requested = 0;
//------------------------------------------------------

void ProcessPayload(uint8_t payload[], uint8_t payload_size) {
  // Process the bytes in the payload buffer, assuming the WHOLE
  // payload has been collected, and it had a perfect checksum.

  payload_process_count++;
  payload_byte_count += payload_size;
  for (uint8_t i = 0; i < payload_size; i++) {
    uint8_t current = payload[i];
    switch (current) {
      case 0x2:
        data_values[SIGNAL_STRENGTH_INDEX] = payload[++i];
        break;

      case 0x3:
        heart_rate = payload[++i];
        break;

      case 0x4:
        data_values[ATTENTION_INDEX] = payload[++i];
        break;

      case 0x5:
        data_values[MEDITATION_INDEX] = payload[++i];
        break;

      case 0x6:
        raw_wave_8_bit = payload[++i];
        break;

      case 0x7:
        zero_raw_marker = payload[++i];
        break;

      /*
        // Mentioned in the June 28, 2010 spec, but not in the May 7,
        // 2015 spec We assume it is discontinued, but *some* old
        // chips may support this.
        case 0x16: // Blink Strength
        blink_strength = payload[++i];
        break;
      */

      case 0x80:
        // "Raw Wave" data. Not sure how to use, so we'll discard it.
        {
          raw_code_count++;
          // Codes above 0x7f have a length field.
          uint8_t byte_count = payload[++i];
          if (i + byte_count > payload_size) {
            // Serial.println("Insufficient payload for raw");
            errors[RAW_PAYLOAD_TOO_BIG_ERROR]++;
            // Discard code byte + byte_count byte + rest of payload
            errors[DISCARDED_BYTE_ERROR] += 2 + (payload_size - i);
            return;  // abort processing bytes.
          }
          // Note: First byte *is* a signed byte, so we need to cast it
          // to signed, and then cast it to a larer signed width so that
          // the shift will not discard bits.
          // If we use the value, we should check that byte_count == 2.
          // uint32_t raw = ((int32_t)(int_t)payload[i + 1] << 8) + payload[i + 2];
          i += byte_count;
          break;
        }

      case 0x83: // ASIC_EEG_POWER_INT
        {
          // Must be <= 24, to supply <= 8 eeg values, and must be
          // multiple of 3.
          uint8_t byte_count = payload[++i];
          uint8_t future_bytes = i + byte_count;
          if ((byte_count > 24) || (byte_count % 3 != 0) ||
              (future_bytes > payload_size)) {
            // Serial.println("Insufficient payload for eeg");
            errors[EEG_BAD_LENGTH_ERROR]++;
            // Discard code byte + byte_count byte + rest of payload
            errors[DISCARDED_BYTE_ERROR] += 2 + (payload_size - i);
            return;  // abort processing bytes.
          }

          for (int j = 0; j < byte_count / 3; j++) {
            // First concatenate the bits in the bytes into a longer
            // int.  Assume low bits are zero (arduino is little endian)
            uint32_t value = (uint32_t)payload[++i] << 16;
            value |= (uint32_t)payload[++i] << 8;
            value |= (uint32_t)payload[++i];
            data_values[j] = value;
          }
          eeg_values_loaded_for_plot++;
          eeg_values_loaded_for_print++;
          break;
        }

      default:
        // Serial.write("Position ");
        // Serial.print(i);
        // Serial.write(" of payload ");
        // Serial.print(expected_payload_size);
        // Serial.write(" Unknown field 0x");
        // Serial.println(current, HEX);
        errors[UNRECOGNIZED_CODE_ERROR]++;
        // Discard code byte + length byte + rest of payload
        errors[DISCARDED_BYTE_ERROR] += 2 + (payload_size - i);
        return; // abort scanning of payload.
    }
    if (i > payload_size) {
      // Code 2, 3, 4, 5, or 0x16 must have "tricked" us to read too far.
      // Serial.println("Read past end of payload");
      errors[READ_PAST_PAYLOAD_ERROR]++;
      return;
    }
  }
  return;  // Valid payload was parsed.
}

void ProcessAllReceiveBufferBytes() {
  // Read bytes from the force trainer, fill up the payload_buffer,
  // and then call for payload processing.  Bytes come in VERY
  // quickly, and we can't afford to drop any, so read until the UART
  // buffer is empty, and THEN return.

  // Maintain the device_read_state we are in as we try to parse bytes
  // received from the headset. We stop when we run out of bytes to
  // processes, but we need to resume in the state where we "left off"
  // previously.
  static enum {
    WAITING_FOR_FIRST_SYNC  = 0,
    WAITING_FOR_SECOND_SYNC = 1,
    WAITING_FOR_LENGTH      = 2,
    READING_PAYLOAD         = 3,
  } device_read_state = WAITING_FOR_FIRST_SYNC;
  const uint8_t MAX_PAYLOAD_LENGTH = 169;
  static uint8_t payload[MAX_PAYLOAD_LENGTH];  // Buffer to hold payload.
  static uint8_t expected_payload_size;
  static uint8_t payload_read;  // Count of bytes of payload already loaded.
  static uint8_t payload_checksum;  // Sum of loaded payload bytes.
  const uint8_t SYNC = 0xAA;

  static long exit_time_millis = 0;
  if (exit_time_millis) {
    long duration = millis() - exit_time_millis;
    if (duration > max_duration_between_byte_processing) {
      max_duration_between_byte_processing = duration;
    }
  }
  while (true) {
    int available = Serial.available();
    if (!available) {
      break;
    }
    if (available > max_available_bytes_to_process) {
      max_available_bytes_to_process = available;  // Update stat.
    }
    uint8_t data = Serial.read();
    switch (device_read_state) {
      case WAITING_FOR_FIRST_SYNC:
        if (data == SYNC) {
          device_read_state = WAITING_FOR_SECOND_SYNC;
        } else {
          // Serial.write("Unexpected 0x");
          // Serial.println(data, HEX);
          errors[MISSING_SYNC_ERROR]++;
          errors[DISCARDED_BYTE_ERROR]++;
        }
        continue;  // loop to get more bytes

      case WAITING_FOR_SECOND_SYNC:
        if (data == SYNC) {
          // Serial.println("Got SYNCs");
          device_read_state = WAITING_FOR_LENGTH;
        } else {
          // Serial.println("Discarding SYNC and following byte");
          errors[MISSING_SYNC_ERROR]++;
          errors[DISCARDED_BYTE_ERROR] += 2;  // Two bytes discarded.
          device_read_state = WAITING_FOR_FIRST_SYNC;
        }
        continue;  // loop to get more bytes

      case WAITING_FOR_LENGTH:
        if (data <= MAX_PAYLOAD_LENGTH) {
          expected_payload_size = data;
          payload_read = 0;
          payload_checksum = 0;
          // Serial.write("Got length ");
          // Serial.println(data);
          device_read_state = READING_PAYLOAD;
        } else {
          errors[BAD_PAYLOAD_LENGTH_ERROR]++;
          errors[DISCARDED_BYTE_ERROR]++;  // The first SYNC character.
          if (data != SYNC) {
            // Serial.write("bad length ");
            // Serial.println(data);
            errors[DISCARDED_BYTE_ERROR] += 2;  // The second SYNC plus length
            device_read_state = WAITING_FOR_FIRST_SYNC;
          }
        }
        continue;  // loop to get more bytes

      case READING_PAYLOAD:
        if (payload_read < expected_payload_size) {
          // Load another byte.
          payload[payload_read] = data;
          payload_checksum += data;
          payload_read++;
          continue;  // loop to get more bytes.
        }
        // Time to validate checksum;
        uint8_t valid_checksum = ~data;
        if (valid_checksum == payload_checksum) {
          // Serial.println("Validated checksum");
          ProcessPayload(payload, payload_read);  // Finally a payload to process!
        } else {
          errors[BAD_CHECKSUM_ERROR]++;
          // Discarded 2 syncs, 1 length, payload bytes, 1 checksum,
          errors[DISCARDED_BYTE_ERROR] += 4 + payload_read;
          // We could check to see if that mistaken checksum was a SYNC,
          // but we won't bother.
        }
        device_read_state = WAITING_FOR_FIRST_SYNC;
        continue;  // loop to get more bytes
    }  // end switch on state
  }  // end while loop reading bytes
  exit_time_millis = millis();  // Help measure time till we return.
}

bool PrintMostRecentData() {
  // Return false if we don't print anything (so errors can be
  // printed).  Do the LEAST possible unit of work, so that we return
  // to processing input bytes ASAP, since input won't wait, and can be
  // lost if the UART buffer overruns. When called again, we'll
  // continue printing where we left off (e.g., printing the value at
  // the next index, or printing yet another piece of data).

  if (Serial.availableForWrite() < 35) {
    // Assume we wanted to print.
    return true;  // We might block if we tried to print.
  }

  // For printing of EEG values, we can't afford to print them all at
  // once, so we just print one each time we have room to print in the
  // Serial output buffer.
  static uint8_t print_index = 0;  // Which item is printed next.

  // We work to print a full line of EEG power values (eventually),
  // and if we have time, we print other data (when we find new data).

  if (eeg_values_loaded_for_print > 0) {
    if (0 == print_index) {
      Serial.write("EEG:");  // Starting a new line.
    }
    if (print_index  < EEG_VALUE_COUNT) {
      Serial.write("  ");
      Serial.write(data_value_names[print_index]);
      Serial.write("=");
      Serial.print(data_values[print_index]);
      print_index++;
    } else {
      // Watch for the augmented values, but only print
      // them (at most) once (each on a newline).
      while (print_index < DATA_ARRAY_SIZE) {
        if (data_values[print_index] < 256) {
          // New value arrived.
          Serial.println();  // Put on separate line.
          Serial.write(data_value_names[print_index]);
          Serial.write("=");
          Serial.print(data_values[print_index]);
          data_values[print_index++] = 256;  // Prevent reprint.
          break;  // come back later to print more..
        }  // End printing augmentation
        print_index++;
      }  // No more augmented values either.
    }
    if (print_index >= DATA_ARRAY_SIZE) {
      Serial.println();  // Carriage return
      if (eeg_values_loaded_for_print > 1) {
        errors[SKIPPED_EEG_DATA_ERROR]++;
      }
      eeg_values_loaded_for_print = 0; // Don't print array until next update.
      print_index = 0;  // Start at beginning, when asked to print.
    }
    return true;  // We printed something.
  }  // end of printing loaded augmened_eeg_values.

  // We've never seen these print, so they're not in the above
  // array, but are interesting to watch out for.
  if (256 > heart_rate) {
    Serial.print(F("heart_rate="));
    Serial.println(heart_rate);
    heart_rate = 256;  // Real data is ALWAYS less than this.
    return true;
  }

  if (256 > raw_wave_8_bit) {
    Serial.print(F("raw_wave_8_bit="));
    Serial.println(raw_wave_8_bit);
    raw_wave_8_bit = 256;  // Real data is ALWAYS less than this.
    return true;
  }

  if (256 > zero_raw_marker) {
    Serial.print(F("zero_raw_marker="));
    Serial.println(zero_raw_marker);
    zero_raw_marker = 256;  // Real data is ALWAYS less than this.
    return true;
  }
  return false;  // Nothing printed.
}

void SnapshotData() {
  for (uint8_t i = 0; i < DATA_ARRAY_SIZE; i++) {
    data_snapshot[i] = data_values[i];
    if (data_values[i] >= one_plus_max_data_snapshot[i]) {
      one_plus_max_data_snapshot[i] = data_values[i] + 1;
    }
    if (data_values[i] < min_data_snapshot[i]) {
      min_data_snapshot[i] = data_values[i];
    }
  }
}

void ScaleDataSnapshot() {
  // Map each piece of data into a separate stripe, by scaling, and
  // then shifting to keep each groph from overlapping (with a little gap).
  uint8_t stripe_count = 0;
  for (int i = 0; i < DATA_ARRAY_SIZE; i++) {
    if (need_to_plot_value[i]) {
      stripe_count++;
    }
  }
  uint32_t gap_divisor = 10;  // 1/10th = 10%
  uint32_t usable_plotting_height = (total_plotting_height / gap_divisor) * (gap_divisor - 1);
  uint8_t next_stripe = 0;
  uint32_t stripe_height = total_plotting_height / stripe_count;
  uint32_t stripe_gap = stripe_height / gap_divisor;  // 10% gap
  for (uint8_t i = 0; i < DATA_ARRAY_SIZE; i++) {
    if (!need_to_plot_value[i]) {
      data_snapshot[i] = 0;
      continue;
    }
    // First scale data to just fit in usable plotting height, as though
    // this data alone will be plotted.
    // Graph_height is < 2^24;  Total height is sufficiently less.
    uint32_t graph_height = one_plus_max_data_snapshot[i] - min_data_snapshot[i];
    uint32_t scale = usable_plotting_height / graph_height;
    data_snapshot[i] = scale * (data_snapshot[i] - min_data_snapshot[i]);

    // Shrink height to fit in a stripe, and shift to the height of the stripe.
    data_snapshot[i] /= stripe_count;
    data_snapshot[i] += next_stripe * stripe_height + stripe_gap / 2;
    next_stripe++;  // Avoid overlap with something that we plot..cuhjjicebcbpxkuiethkjhhdxhjcpinngejehgkg..h

  }
}


bool PlotMostRecentData() {
  // Be sure to use the Arduino "Serial Plotter" instead of the
  // "Serial Terminal" if you want to see this graph!  We do a minimal
  // amount of work (print at most one value, just like the
  // PrintMostRecentData() does) so we can get back to
  // receiving/processing bytes ASAP. Return true iff we are printing
  // a line, and don't want to be interrupted, or if we printed data.

  // For plotting of EEG values, we can't afford to print them all at
  // once, so we just emit one each time we are called.
  static uint8_t plot_index = 0;

  // Ensure there is room in the Serial output buffer, so there is NO
  // CHANCE that we block here.
  if (eeg_values_loaded_for_plot == 0 ||
      Serial.availableForWrite() < 16) {  // We only need space for one number.
    return (plot_index > 0);  // Indicate if there is a line partially printed.
  }

  // EEG values only change about 1 time per second, which makes the
  // plotting values really slow and muddied. As a result, we
  // interpolate intermediate values so that plotting scrolls faster
  // (and old data quickly scrolls off the screen, allowing it to
  // auto-scale up the recent more recent data), making the remaining
  // data clearer.  A larger number produces more intermediate points,
  // moving the graph faster, but if you go too fast, you'll write too
  // much data, and miss the start of displaying the next set of eeg
  // values (so the graph will look strange/choppy).
  static uint32_t prev_snapshot[DATA_ARRAY_SIZE] = {0};  // For interpolation.
  const uint8_t LAST_PLOT_INTERPOLATION = 5;  // Extra interpolated points emitted.
  static uint8_t plot_interpolations = 1;  // Which of N are we working on now.

  if (plot_index >= DATA_ARRAY_SIZE) { // We're done printing vector values.
    Serial.write("\t");
    Serial.print(0);
    Serial.write("\t");
    Serial.println(total_plotting_height);
    plot_index = 0;  // Start next line at the beginning of data_snapshot.
    plot_interpolations++;  // We completed one more intepolated set.
    if (plot_interpolations > LAST_PLOT_INTERPOLATION) {
      // We completed interpolation to the new point.  Prepare to start again.
      plot_interpolations = 1;
      if (eeg_values_loaded_for_plot > 1) {
        errors[SKIPPED_EEG_DATA_ERROR]++;
      }
      eeg_values_loaded_for_plot = 0;  // Wait for fresh values.
      // Save plotted values, for our next interpolation.
      for (uint8_t i = 0; i < DATA_ARRAY_SIZE; i++) {
        prev_snapshot[i] = data_snapshot[i];
      }
    }
    return false;  // Errror messages won't interrupt anything.
  }

  if (plot_index > 0) {
    Serial.write('\t');  // We're continuing a line.
  } else {
    // We are starting a new vector.
    if (plot_interpolations == 1) {
      // We need to gather fresh data.
      SnapshotData();  // Get the newest data.
      ScaleDataSnapshot();
    }
  }

  if (!need_to_plot_value[plot_index]) {
    Serial.print(0);  // Suppress real value on plotter.
  } else {
    // Interpolate a point on the line from old value to new (current)
    // value.
    uint32_t value = ((LAST_PLOT_INTERPOLATION - plot_interpolations) *
                      prev_snapshot[plot_index] +
                      plot_interpolations * data_snapshot[plot_index]) /
                     LAST_PLOT_INTERPOLATION;
    Serial.print(value);
  }

  plot_index++;
  return true;  // We printed something.
}

void UpdatePlotSelection() {
  // Check to see if the user has pressed any of the buttons, which
  // can toggle whether we display the real data in the plotter
  // output. Techniques used here are modeled after the Arduino
  // Debounce example (except that we have an array of buttons to
  // monitor, and array of results to update). Most critically, we
  // don't block or wait to do the debouncing
  static long button_change_millis[DATA_ARRAY_SIZE];  // Last time button changed.
  static int button_recent_state[DATA_ARRAY_SIZE];  // Last observed state.
  static int button_state[DATA_ARRAY_SIZE];  // Debounced official state.
  static uint8_t button_pressed_count = 0;  // Turn on LED if any are pressed.
  const unsigned long debounce_delay = 100;  // milliseconds

  uint8_t count = 0;  // Count of buttons on.
  for (uint8_t i = 0; i < PUSH_BUTTON_COUNT; i++) {
    uint8_t pin = 2 + i;
    int reading = digitalRead(pin);

    if (reading != button_recent_state[i]) {
      button_change_millis[i] = millis();
      button_recent_state[i] = reading;
    }

    if ((millis() - button_change_millis[i]) > debounce_delay) {
      if (reading != button_state[i]) {
        button_state[i] = reading;  // State has changed officially.
        if (reading == HIGH) {
          // We must change a state and total count.
          need_to_plot_value[i] = !need_to_plot_value[i];
        }
      }
    }
    if (button_state[i] == HIGH) {
      count++;
    }
  }
  if (count != button_pressed_count) {
    if (count > button_pressed_count) {  // Rising count
      if (count == 2) {
        optional_stats_print_requested = 1;  // Start printing stats.
      }
      if (count == 3) {
        plot_it = !plot_it;  // Toggle plotting mode vs displaying data.
      }
    }
    // Push count to global variable that persists.
    button_pressed_count = count;
    digitalWrite(LED_PIN, (button_pressed_count > 0) ? HIGH : LOW);
  }
}

void PrintNewErrorCounts() {
  // Return true iff we printed any data.

  // Avoid blocking, by ONLY printing if there is plenty of room to
  // print, without having to wait for space to buffer up our printed
  // lines.  We use a high limit, to ensure that IF there is data to
  // print, it will print before we get a chance, and errors will just
  // accumulate for a while (and not slow data emission).
  if (Serial.availableForWrite() < 50) {
    return;
  }

  // Only print the first problem that we find, so we won't print more
  // than about 20 characters (and we just checked that there was room
  // for them).  Only print when we really see errors, since we're
  // trying not to print too much too often.  Reset each error count
  // after we report it.
  for (uint8_t i = 0; i < ERROR_ARRAY_SIZE; i++) {
    if (errors[i] > 0) {
      Serial.write(error_names[i]);
      Serial.write("=");
      Serial.print(errors[i]);
      Serial.write("\n");
      errors[i] = 0;
      break;
    }
  }
}

void OptionalPrintStats() {
  if (Serial.availableForWrite() < 50) {
    return;
  }
  switch (optional_stats_print_requested) {
    case 0:
      return;  // We are not asked to print stats yet.

    case 1:
      Serial.print(F("Max available "));
      Serial.println(max_available_bytes_to_process);
      break;

    case 2:
      Serial.print(F("Max pause ms "));
      Serial.println(max_duration_between_byte_processing);
      break;

    case 3:
      Serial.print(F("Payloads "));
      Serial.println(payload_process_count);
      break;

    case 4:
      Serial.print(F("Payload Bytes "));
      Serial.println(payload_byte_count);
      break;

    case 5:
      Serial.print(F("Total Bytes "));
      Serial.println(4 * payload_process_count + payload_byte_count);
      break;

    case 6:
      Serial.print(F("Total ms "));
      Serial.println(millis() - start_time_millis);
      break;

    case 7:
      Serial.print(F("Avg Bytes/s "));
      Serial.println((4 * payload_process_count + payload_byte_count) * 10 /
                     ((millis() - start_time_millis) / 100));
      break;

    case 8:
      Serial.print(F("Raw codes "));
      Serial.println(raw_code_count);
      break;

    default:
      optional_stats_print_requested = 0;
      return;  // Don't increment.
  }
  optional_stats_print_requested++;  // Print next stat when next called.
}

void setup() {
  for (uint8_t i = 0; i < DATA_ARRAY_SIZE; i++) {
    data_values[i] = 0;
    // Default to plotting everything.
    need_to_plot_value[i] = true;
    if (i < EEG_VALUE_COUNT) {
      min_data_snapshot[i] = (int32_t) - 1; // max unsigned int.
      one_plus_max_data_snapshot[i] = 0;  // min unsigned int.
    } else {
      // Use the better estimate min/max for special values,
      // instead of calculating them during the run.
      min_data_snapshot[i] = 0;
      one_plus_max_data_snapshot[i] = 101;
    }
    if (i < PUSH_BUTTON_COUNT) {
      uint8_t pin = 2 + i;
      pinMode(pin, INPUT);
    }
  }

  // Visual feedback when buttons are pressed, which helps to
  // debug wiring.
  pinMode(LED_PIN, OUTPUT);  // Light when button is pressed.
  digitalWrite(LED_PIN, LOW);

  Serial.begin(57600);  // ForceTrainer defaults to this baud rate.

  while (!Serial ) { // Until we're ready...
    ;  // do nothing, and wait till Serial is setup.
  }
  start_time_millis = millis();
  // See how much space there is to write, without having to wait
  int MAX_WRITE_BUFFER = Serial.availableForWrite();
  Serial.print(F("When empty, write buffer has room for: "));
  Serial.println(MAX_WRITE_BUFFER);  // Should be 63
}

void loop() {
  // Process all the incoming bytes we can, so that we don't "drop"
  // any because there is no room to buffer them up.
  ProcessAllReceiveBufferBytes();
  // We are now guaranteed that the Serial input buffer is empty. AT
  // 57600 baud, we'll get (worst case) about 5,760 bytes per second
  // (byte == 8 bits, but transmission uses an extra two bits, for a
  // total of 10 bits per byte). We can (possibly) overrun the 63
  // character input buffer in as little as 10 ms, so we have to be
  // careful to come back ASAP.  As a result, the remaining functions
  // must NOT block (e.g., wait for room in an output buffer to be
  // created), but can generally push (write) to an output buffer that
  // *has* available room.  Running on the Arduino UNO, we usualy get
  // back to processing bytes before the input buffer can queue up 3
  // packets, with the code that follows.

  // Select whether to output for Serial Plotter vs Serial Monitor.
  // Serial Monitor prints more readable text, including the label for
  // each numeric value, as well as including error counts, and could
  // be used to see/debug what the plotter output looks like in
  // numeric form.  The Serial Plotter needs an unadorned line of 11
  // numbers on each output line, and uses them to create the plat(s).
  UpdatePlotSelection();  // Monitor push buttons.
  bool busy_printing_line = false;
  if (plot_it) {
    busy_printing_line = PlotMostRecentData();
  } else {
    busy_printing_line = PrintMostRecentData();
  }
  if (!busy_printing_line) {  // Avoid interupting a line of output!
    PrintNewErrorCounts();
    OptionalPrintStats();
  }
}
