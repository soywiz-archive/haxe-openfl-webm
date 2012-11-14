@echo off
rd /S /Q project\obj 2> NUL
del /Q project\all_objs 2> NUL
del /Q project\vc110.pdb 2> NUL
REM rd /S /Q ndll\Windows 2> NUL