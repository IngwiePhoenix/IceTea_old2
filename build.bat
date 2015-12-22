@echo off

REM Assuming that cl.exe is in path...

REM make output folder
mkdir out

REM Compile incbin
cl incbin/incbin.c /Fe:incbin

REM Render source files
incbin src\sources.rc -o out\sources.cpp

REM Compile IceTea
cl src\*.cpp out\sources.cpp /Fe:out\icetea.speed

REM Compile IceTea using IceTea!
out\icetea.speed
