set PATH=%PATH%;c:\users\ci\bin;c:\users\ci\workspace\dcpomatic\bin
set DCPOMATIC_TEST_PRIVATE=c:\users\ci\dcpomatic-test-private
xcopy ..\libdcp\tags build\tags\
copy ..\openssl\apps\openssl.exe build\test\
xcopy fonts build\fonts\
build\test\unit-tests.exe %1 %2
