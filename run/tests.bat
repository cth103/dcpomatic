set PATH=%PATH%;c:\users\ci\bin_v2.20.x;c:\users\ci\workspace\dcpomatic\bin;c:\users\ci\workspace\dcpomatic\lib
set DCPOMATIC_TEST_PRIVATE=c:\users\ci\dcpomatic-test-private
xcopy ..\libdcp\tags build\test\tags\
copy ..\libdcp\ratings build\test\
copy ..\openssl\apps\openssl.exe build\test\
copy fonts\fonts.conf.windows fonts\fonts.conf
build\test\unit-tests.exe --log_level=test_suite %1 %2
