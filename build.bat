@echo off

REM Assuming that cl.exe is in path...

REM make output folder
mkdir out

REM Build IncBin
cl incbin/incbin.c /Feincbin

REM Build the source container
incbin src/scripts.rc -o out/os-scripts.cpp

REM Build IceTea
cl /EHsc src\*.cpp out\os-scripts.cpp /Feout\icetea.flat

REM Build it using ourself.
.\out\icetea.flat --it-optimize
