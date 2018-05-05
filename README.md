# Neurosky Hacking Star Wars Force Trainer
Arduino code for processing data from Neurosky devices, such
as Star Wars Force Trainer.
Written by Brianna Roskind and Jim Roskind

Transforms the "toy" headset into an EEG display that can
graph any/all of the 8 EEG powerbands, plus Meditation and
Attention signals, in real-time. Pushbuttons allow for
selection of signals to be plotted.  Uses Arduino IDE
Serial Plotter to display results, and 10 pushbuttons
to control the selection of displayed signals.

Demonstrates techniques for cooperative multitasking,
where input signal (57,600 baud serial input) MUST be
processed in real time, and additional code MUST NOT
block or delay that processing. Includes instrumentation
to measure the effectiveness of the approach (which
allows others to modify the code, and debug problems
that might be added by additional (unplanned?) delays).

Requires electrical connection to "T" terminal (and
ground) inside the Force Trainer headset (see comments
in ForceTrainer.ino file).
