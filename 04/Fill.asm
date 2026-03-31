// Runs an infinite loop that listens to the keyboard input.
// When a key is pressed (any key), the program blackens the screen,
// i.e. writes "black" in every pixel. When no key is pressed,
// the screen should be cleared.

(LOOP)
    // load the keyboard input
    @KBD
    D=M
    @colour
    M=0
    @FILL
    D;JGT       // if no keyboard input jump to filling the screen
    @colour
    M=-1

(FILL)
    // store the start of screen memory for writing to
    @SCREEN
    D=A
    @write_address
    M=D
(FILL_LOOP)
    // loop through 8k of screen memory and write 16 pixels at a time
    @colour
    D=M
    @write_address
    A=M
    M=D

    // see if we've hit end of screen memory
    @12288
    D=A         // bit of a hack but screen starts at 4k and is 8k long, i.e. ends at 12k == 12288
    @write_address
    M=M+1
    D=D-M
    @FILL_LOOP
    D;JLT

    @LOOP
    0;JMP
