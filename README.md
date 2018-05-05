# Neurosky Hacking Star Wars Force Trainer
Cooperative multitasking code for Arduino processing of
high-speed serial data from Neurosky devices, such
as Star Wars Force Trainer, using Arduino Uno.

Written by Brianna Roskind and Jim Roskind

The code transforms the "toy" headset that drives a game,
into an EEG system that can graph any/all of the 8 EEG 
powerbands, plus Meditation and Attention signals, in 
real-time. Pushbuttons allow for selection of signals to 
be plotted.  Uses Arduino IDE Serial Plotter to display 
results, and 10 pushbuttons to control the selection of 
displayed signals.

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

The protocol definition can be reviewed in Jan 2017 doc:
http://developer.neurosky.com/docs/doku.php?id=thinkgear_communications_protocol

or a May 2015 doc:
http://developer.neurosky.com/docs/lib/exe/fetch.php?media=mindset_communications_protocol.pdf

or a Jun 2010 doc:
http://wearcam.org/ece516/mindset_communications_protocol.pdf

There have been small changes and additions to the 
protocol (in the above docs) over time, and you may 
need to make slight modifications if you're using an 
especially old or new(er) version of a chip.
