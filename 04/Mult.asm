// Multiplies R0 and R1 and stores the result in R2.
// (R0, R1, R2 refer to RAM[0], RAM[1], and RAM[2], respectively.)
// The algorithm is based on repetitive addition.

    // initialise loop counter to be value in R0
    @0
    D=M
    @loop_counter
    M=D

    // store product in R2, we add R1 to R2, R0 times
    @2
    M=0
(LOOP)
    // check if loop counter is 0, exit if so
    @loop_counter
    D=M
    M=M-1
    @END
    D;JEQ

    // add contents of R1 to accumulated product in R2
    @1
    D=M
    @2
    M=D+M

    // repeat
    @LOOP
    0;JMP
(END)
