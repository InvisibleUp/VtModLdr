@ECHO OFF
for /f %%x in ('dir /b /s *.c') do ^
echo int Test_%%~nx(void);