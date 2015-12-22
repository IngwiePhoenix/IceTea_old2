@echo off

REM Assuming that cl.exe is in path...

REM make output folder
mkdir out

REM Build IncBin
cl incbin/incbin.c /Feincbin

Build the source container
incbin src/scripts.rc -o src/scripts.cpp

REM Build IceTea
cl /EHsc src/*.cpp /Feout\icetea
