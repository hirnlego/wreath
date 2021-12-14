# Wreath

A multifaceted looper for the Daisy platform.

- sample  rate reduction
- mimeo: when clocked, there should be a way to clock the looper, speed is a divisor/multiplier
- clocked, envelope
- extend initial buffer
- link loopers (eg. play in series)
- stutter (stops and goes, repeats), drag, jitter
- more play heads





- all variables and objects should preferably be declared inside the function in which they are used
- Do not make variables global if you can avoid it. It may be useful to make a variable global if it is accessed by several different functions and you want to avoid the overhead of transferring the variable as function parameter. But it may be a better solution to make the functions that access the saved variable members of the same class and store the shared variable inside the class.
- It is preferable to declare a lookup table static and constant.
- Multiplication and division take longer time.
- It may be advantageous to put the operand that is most often true last in an && expression,
or first in an || expression.
- In some cases it is possible to replace a poorly predictable branch by a table lookup. For example:
// Example 7.29a
float a; bool b;
a = b ? 1.5f : 2.6f;
The ?: operator here is a branch. If it is poorly predictable then replace it by a table lookup:
45
// Example 7.29b
float a; bool b = 0;
const float lookup[2] = {2.6f, 1.5f};
a = lookup[b];
- Avoid unnecessary functions
- Inlining a function is advantageous if the function is small or if it is called only from one place in the program
- A function that is used only within the same module (i.e. the current .cpp file) should be made local. Add the keyword static to the function declaration.
-