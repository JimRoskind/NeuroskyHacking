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
// graphing of any/all of the 8 power bands (plus Attention,
// Meditation).
// 
// Note that if all the values are disabled (by toggling using the
// pushbuttons), then the "Signal Strength" metric will automatically
// be graphed. This can be used to see if the headset is properly
// worn.
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
// only enabled (re: shown as non-zero) when all the other 10 values
// are disabled (displaying as zeros).
//
// If there are any errors (loss of sync? unexpected data?), then
// error counts will be printed on the terminal.
//
// To see the data with tags (rather than a vector of 11 integers) in
// the Serial Terminal window, hold down any two pushbottons at the
// same time, and the program with toggle into a text mode, listing
// names and values.
//
// To see additional performance statistics, depress any 3 buttons at
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
// TBD: We may change other state based on pressing multiple buttons!
// Currently, pressing 2 buttons at same time causes stots to print
// Pressing 3 buttons at same time taggles Plot vs Terminal output
// modes.
uint8_t button_pressed_count = 0;  // Turn on LED if any are pressed.

//------------------------------------------------------
// 8-bit Data extracted from payload messages.
// 256 means no new value.
uint16_t signal_strength = 256;
uint16_t attention = 256;
uint16_t meditation = 256;
uint16_t heart_rate = 256;
uint16_t raw_wave_8_bit = 256;
uint16_t zero_raw_marker = 256;  // Always reads zero.

#define EEG_VALUE_COUNT 8  // Number of EEG power values.
// When plotting, we add "attention" and "meditation" to the plotted
// values, and use switches to enable/disable plotting all the 12
// values. If user disables all 12, when we plot the value of
// signal_strength.
#define PLOTTING_VALUE_COUNT (2 + EEG_VALUE_COUNT)
// Define what index is used for the extra two values.
#define MEDITATION_INDEX (EEG_VALUE_COUNT)
#define ATTENTION_INDEX (EEG_VALUE_COUNT + 1)
uint32_t eeg_values[PLOTTING_VALUE_COUNT] = {0};

// Signal when there are new values in eeg_values to print/display.
bool eeg_values_loaded_for_print = false;
bool eeg_values_loaded_for_plot = false;

// Control the plotting with an array of bools, toggled by
// corresponding pushbuttons.
bool need_to_plot_eeg_value[PLOTTING_VALUE_COUNT] = {false};  // Selected data to plot.

//------------------------------------------------------
// We can't afford the time it takes to print errors as we see them,
// but we can count them, and print a summary later.
// List of byte processing errors.
uint16_t discard_byte_count = 0;  // Total ignored for all reasons.
uint8_t missing_sync_count = 0;  // Characters before SYN.
uint8_t bad_length_count = 0;  // Payload length error.
uint8_t bad_checksum_count = 0;  // Mismatched checksum.
// List of payload processing errors.
uint8_t eeg_bad_length_count = 0;  // Power band length wasn't 24 == 3 * 8.
uint8_t raw_payload_too_big_count = 0;  // During payload processing.
uint8_t unrecognized_code_count = 0;  // During payload processing.
uint8_t read_past_end_count = 0;   // At end of payload processing.

//------------------------------------------------------
// Performance statistics.  Push any 3 buttons to cause them to be
// printed (though you'll only see them in a Serial Monitor.
uint8_t max_available_bytes_to_process = 0;  // Worry when near 63!
uint16_t max_duration_between_byte_processing = 0;
uint32_t payload_process_count = 0;
uint32_t payload_byte_count = 0;
uint32_t raw_code_count = 0;  // Raw wave data in payload.
uint32_t start_time_millis = 0;
uint8_t optional_stats_print_requested = 0;
//------------------------------------------------------

void ProcessPayload(uint8_t payload[], uint8_t payload_size) {
  // Process the bytes in the payload buffer, assuming the WHOLE
  // payload has been collected, and it had a perfect checksum.

  payload_process_count++;
  payload_byte_count += payload_size;
  for (uint8_t i = 0; i < payload_size; i++) {
    uint8_t current = payload[i];
    switch(current) {
    case 0x2:
      signal_strength = payload[++i];
      break;

    case 0x3:
      heart_rate = payload[++i];
      break;

    case 0x4:
      attention = payload[++i];
      break;

    case 0x5:
      meditation = payload[++i];
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

    case 0x83: // ASIC_EEG_POWER_INT
      {
        // Must be <= 24, to supply <= 8 eeg values, and must be
        // multiple of 3.
        uint8_t byte_count = payload[++i];
        uint8_t future_bytes = i + byte_count;
        if ((byte_count > 24) || (byte_count % 3 != 0) ||
            (future_bytes > payload_size)) {
          // Serial.println("Insufficient payload for eeg");
          eeg_bad_length_count++;
          // Discard code byte + byte_count byte + rest of payload
          discard_byte_count += 2 + (payload_size - i);
          return;  // abort processing bytes.
        }

        for (int j = 0; j < byte_count / 3; j++) {
          // First concatenate the bits in the bytes into a longer
          // int.  Assume low bits are zero (arduino is little endian)
          uint32_t value = (uint32_t)payload[++i] << 16;
          value |= (uint32_t)payload[++i] << 8;
          value |= (uint32_t)payload[++i];
          eeg_values[j] = value;
        }
        eeg_values_loaded_for_plot = true;
        eeg_values_loaded_for_print = true;
        break;
      }

    case 0x80:
      // "Raw Wave" data. Not sure how to use, so we'll discard it.
      {
        raw_code_count++;
        // Codes above 0x7f have a length field.
        uint8_t byte_count = payload[++i];
        if (i + byte_count > payload_size) {
          // Serial.println("Insufficient payload for raw");
          raw_payload_too_big_count++;
          // Discard code byte + byte_count byte + rest of payload
          discard_byte_count += 2 + (payload_size - i);
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

    default:
      // Serial.write("Position ");
      // Serial.print(i);
      // Serial.write(" of payload ");
      // Serial.print(expected_payload_size);
      // Serial.write(" Unknown field 0x");
      // Serial.println(current, HEX);
      unrecognized_code_count++;
      // Discard code byte + length byte + rest of payload
      discard_byte_count += 2 + (payload_size - i);
      return; // abort scanning of payload.
    }
    if (i > payload_size) {
      // Code 2, 3, 4, 5, or 0x16 must have "tricked" us to read too far.
      // Serial.println("Read past end of payload");
      read_past_end_count++;
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
    switch(device_read_state) {
    case WAITING_FOR_FIRST_SYNC:
      if (data == SYNC) {
        device_read_state = WAITING_FOR_SECOND_SYNC;
      } else {
        // Serial.write("Unexpected 0x");
        // Serial.println(data, HEX);
        missing_sync_count++;
        discard_byte_count++;
      }
      continue;  // loop to get more bytes

    case WAITING_FOR_SECOND_SYNC:
      if (data == SYNC) {
        // Serial.println("Got SYNCs");
        device_read_state = WAITING_FOR_LENGTH;
      } else {
        // Serial.println("Discarding SYNC and following byte");
        missing_sync_count++;
        discard_byte_count += 2;  // Two bytes discarded.
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
        bad_length_count++;
        discard_byte_count++;  // The first SYNC character.
        if (data != SYNC) {
          // Serial.write("bad length ");
          // Serial.println(data);
          discard_byte_count += 2;  // The second SYNC plus length
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
        bad_checksum_count++;
        // Discarded 2 syncs, 1 length, payload bytes, 1 checksum,
        discard_byte_count += 4 + payload_read;
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
  // to processing input bytes ASAP, since they won't wait, and can be
  // lost if the UART buffer overruns. When called again, we'll
  // continue printing where we left off (e.g., printing the value at
  // the next index, or printing yet another piece of data).

  if (Serial.availableForWrite() < 35) {
    return true;  // We were blocked from printing.
  }

  // For printing of EEG values, we can't afford to print them all at
  // once, so we just print one each time we have room to print in the
  // Serial output buffer.
  static uint8_t print_index = 0;  // Which item is printed next.

  // We work to print a full line of EEG power values (eventually),
  // and if we have time, we print other data (when we find new data).

  if (eeg_values_loaded_for_print) {
    static const char* const eeg_names[EEG_VALUE_COUNT] = {
      "Delta",
      "Theta",
      "L-alpha",
      "H-alpha",
      "L-beta",
      "H-beta",
      "L-gamma",
      "M-gamma"
    };
    if (0 == print_index) {
      Serial.write("EEG: ");  // Starting a new line.
    }
    Serial.write(eeg_names[print_index]);
    Serial.write("=");
    Serial.print(eeg_values[print_index]);
    Serial.write("  ");
    if (++print_index >= EEG_VALUE_COUNT) {
      Serial.println();  // Carriage return
      eeg_values_loaded_for_print = false; // Don't print until next update.
      print_index = 0;  // Start at beginning, when asked to print.
    }
    return true;  // We printed something.
  }  // endif printing eeg values.

  if (256 > signal_strength) {
    Serial.write("signal_strength=");
    Serial.println(signal_strength);
    signal_strength = 256;  // Real data is ALWAYS less than this.
    return true;
  }

  if (256 > attention) {
    Serial.write("attention=");
    Serial.println(attention);
    attention = 256;  // Real data is ALWAYS less than this.
    return true;
  }

  if (256 > meditation) {
    Serial.write("meditation=");
    Serial.println(meditation);
    meditation = 256;  // Real data is ALWAYS less than this.
    return true;
  }

  if (256 > heart_rate) {
    Serial.write("heart_rate=");
    Serial.println(heart_rate);
    heart_rate = 256;  // Real data is ALWAYS less than this.
    return true;
  }

  if (256 > raw_wave_8_bit) {
    Serial.write("raw_wave_8_bit=");
    Serial.println(raw_wave_8_bit);
    raw_wave_8_bit = 256;  // Real data is ALWAYS less than this.
    return true;
  }

  if (256 > zero_raw_marker) {
    Serial.write("zero_raw_marker=");
    Serial.println(zero_raw_marker);
    zero_raw_marker = 256;  // Real data is ALWAYS less than this.
    return true;
  }

  return false;  // Nothing printed.
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
  if (!eeg_values_loaded_for_plot ||
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
  static uint32_t eeg_values_old[PLOTTING_VALUE_COUNT] = {0};  // For interpolation.
  const uint8_t LAST_PLOT_INTERPOLATION = 10;  // Extra points emitted.
  static uint8_t plot_interpolations = 1;  // Which of N are we working on now.

  if (0 != plot_index) {
    Serial.write('\t');  // We're continuing a line.
  } else {
    // Load extra values, if we have anything new.
    if (256 > attention) {
      eeg_values[ATTENTION_INDEX] = attention;
      attention = 256;  // A byte data value is always less than this.
    }
    if (256 > meditation) {
      eeg_values[MEDITATION_INDEX] = meditation;
      meditation = 256;  // A byte data value is always less than this.
    }
  }

  if (!need_to_plot_eeg_value[plot_index]) {
    Serial.print(0);  // Suppress real value on plotter.
  } else {
    // Interpolate a point on the line from old value to new (current)
    // value.
    uint32_t value = ((LAST_PLOT_INTERPOLATION - plot_interpolations) *
                      eeg_values_old[plot_index] +
                      plot_interpolations * eeg_values[plot_index]) /
      LAST_PLOT_INTERPOLATION;
    Serial.print(value);
  }

  if (++plot_index >=PLOTTING_VALUE_COUNT) {  // We're done printing vector values.
    // Test to see if we are plotting anything else currently.
    bool print_signal_strength = true;
    for (uint8_t i = 0; i < PLOTTING_VALUE_COUNT; i++) {
      if (need_to_plot_eeg_value[i]) {
        print_signal_strength = false;
        break;
      }
    }
    Serial.write('\t');
    // We only print the signal_strength if all other values are
    // disabled.
    if (print_signal_strength) {
      Serial.print(signal_strength);
    } else {
      Serial.write('0');
    }
    Serial.write('\n');

    plot_index = 0;  // Start next line at the beginning of eeg_values.
    plot_interpolations++;  // We completed one more intepolated set.
    if (plot_interpolations > LAST_PLOT_INTERPOLATION) {
      plot_interpolations = 1;
      eeg_values_loaded_for_plot = false;
      // Save actual values, for our next interpolation.
      for (uint8_t i = 0; i < PLOTTING_VALUE_COUNT; i++) {
        eeg_values_old[i] = eeg_values[i];
      }
    }
  }
  return true;  // We printed something.
}

void UpdatePlotSelection() {
  // Check to see if the user has pressed any of the buttons, which
  // can toggle whether we display the real data in the plotter
  // output. Techniques used here are modeled after the Arduino
  // Debounce example (except that we have an array of buttons to
  // monitor, and array results to update). Most critically, we don't
  // block or wait to do the debouncing
  static long button_change_millis[PLOTTING_VALUE_COUNT];  // Last time button changed.
  static int button_recent_state[PLOTTING_VALUE_COUNT];  // Last observed state.
  static int button_state[PLOTTING_VALUE_COUNT];  // Debounced official state.
  const unsigned long debounce_delay = 200;  // milliseconds

  uint8_t count = 0;  // Count of buttons on.
  for (uint8_t i = 0; i < PLOTTING_VALUE_COUNT; i++) {
    uint8_t pin = 2 + i;
    int reading = digitalRead(pin);

    if (reading != button_recent_state[i]) {
      button_change_millis[i] = millis();
      button_recent_state[i] = reading;
    }

    if ((millis() - button_change_millis[i]) > debounce_delay) {
      if (reading != button_state[i]) {
        button_state[i] = reading;
        if (reading == HIGH) {
          need_to_plot_eeg_value[i] = !need_to_plot_eeg_value[i];
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

  if (missing_sync_count > 0) {
    Serial.write("missing_sync_count ");
    Serial.println(missing_sync_count);
    missing_sync_count = 0;
  }  else if (bad_length_count > 0) {
    Serial.write("bad_length_count ");
    Serial.println(bad_length_count);
    bad_length_count = 0;
  } else if (bad_checksum_count > 0) {
    Serial.write("bad_checksum_count ");
    Serial.println(bad_checksum_count);
    bad_checksum_count = 0;
  } else if (eeg_bad_length_count > 0) {
    Serial.write("eeg_bad_length_count ");
    Serial.println(eeg_bad_length_count);
    eeg_bad_length_count = 0;
  } else if (raw_payload_too_big_count > 0) {
    Serial.write("raw_payload_too_big_count ");
    Serial.println(raw_payload_too_big_count);
    raw_payload_too_big_count = 0;
  } else if (unrecognized_code_count > 0) {
    Serial.write("unrecognized_code_count ");
    Serial.println(unrecognized_code_count);
    unrecognized_code_count = 0;
  } else if (read_past_end_count > 0) {
    Serial.write("read_past_end_count ");
    Serial.println(read_past_end_count);
    read_past_end_count = 0;
  } else if (discard_byte_count > 0) {
    Serial.write("discard_byte_count ");
    Serial.println(discard_byte_count);
    discard_byte_count = 0;
  }
}

void OptionalPrintStats() {
  if (Serial.availableForWrite() < 50) {
    return;
  }
  switch(optional_stats_print_requested) {
  case 0:
    return;  // We are not asked to print stats yet.

  case 1:
    Serial.write("Max available ");
    Serial.println(max_available_bytes_to_process);
    break;

  case 2:
    Serial.write("Max pause ms ");
    Serial.println(max_duration_between_byte_processing);
    break;

  case 3:
    Serial.write("Payloads ");
    Serial.println(payload_process_count);
    break;

  case 4:
    Serial.write("Payload Bytes ");
    Serial.println(payload_byte_count);
    break;

  case 5:
    Serial.write("Total Bytes ");
    Serial.println(4 * payload_process_count + payload_byte_count);
    break;

  case 6:
    Serial.write("Total ms ");
    Serial.println(raw_code_count);
    break;

  case 7:
    Serial.write("Avg Bytes/s ");
    Serial.println((4 * payload_process_count + payload_byte_count) * 10 /
                   ((millis() - start_time_millis) / 100));
    break;

  case 8:
    Serial.write("Raw codes ");
    Serial.println(millis() - start_time_millis);
    break;

  default:
    optional_stats_print_requested = 0;
    return;  // Don't increment.
  }
  optional_stats_print_requested++;  // Print next stat when next called.
}

void setup() {
  for (uint8_t i = 0; i < PLOTTING_VALUE_COUNT; i++) {
    eeg_values[i] = 0;

    // Default to almost plotting nothing.
    need_to_plot_eeg_value[i] = false;
    pinMode(i + 2, INPUT);  // Pushbuttons that toggle selection.
  }
  // Only plot meditation and attention at startup.
  need_to_plot_eeg_value[MEDITATION_INDEX] = true;
  need_to_plot_eeg_value[ATTENTION_INDEX] = true;

  // Visual feedback when buttons are pressed, which helps to
  // debug wiring.
  pinMode(LED_PIN, OUTPUT);  // Light when button is pressed.
  digitalWrite(LED_PIN, LOW);
  button_pressed_count = 0;

  Serial.begin(57600);  // ForceTrainer defaults to this baud rate.

  while(!Serial ) {  // Until we're ready...
    ;  // do nothing, and wait till Serial is setup.
  }
  start_time_millis = millis();
  // See how much space there is to write, without having to wait
  int MAX_WRITE_BUFFER = Serial.availableForWrite();
  Serial.write("When empty, write buffer has room for: ");
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

