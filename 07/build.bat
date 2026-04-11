@ECHO off

REM had to pull file_names.c out of main.c because windows.h would
REM cause multiply defined symbols like ERROR, TokenType, sigh...
clang main.c file_names.c -o vmtran.exe -g -std=c2x
