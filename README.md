# Wreath

A multifaceted looper for the Daisy platform.

BUGS

1) clicks when going backwards or at a speed different than the write one
2) in pendulum sometimes a flanger-like effect is produced (check feedback and (1))
3) when modifying the loop start point while frozen, if the start point "pushes" the read position smearing occurs
7) fade is broken when frozen across the boundary

TODO

- modulation matrix
- sample rate reduction
- mimeo: when clocked, there should be a way to clock the looper, speed is a divisor/multiplier
- clocked, envelope
- extend initial buffer
- link loopers (eg. play in series)
- stutter (stops and goes, repeats), drag, jitter
- more play heads