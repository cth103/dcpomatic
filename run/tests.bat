set PATH=%PATH%;c:\users\ci\bin;c:\users\ci\workspace\dcpomatic\bin;c:\users\ci\workspace\dcpomatic\lib
set DCPOMATIC_TEST_PRIVATE=c:\users\ci\dcpomatic-test-private
xcopy ..\libdcp\tags build\tags\
copy ..\openssl\apps\openssl.exe build\test\
xcopy fonts build\fonts\
build\test\unit-tests.exe --log_level=test_suite %1 %2
