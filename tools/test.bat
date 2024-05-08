@echo off

echo )OxC3
OxC3

REM Test rand

echo )OxC3 rand ?
OxC3 rand ?

echo )OxC3 rand key -count 5
OxC3 rand key -count 5

echo )OxC3 rand char -count 5 -length 32 --alphanumeric
OxC3 rand char -count 5 -length 32 --alphanumeric

echo )OxC3 rand num -length 64 -bits 32
OxC3 rand num -length 64 -bits 32

echo )OxC3 rand data -length 256
OxC3 rand data -length 256

REM test hash

echo )OxC3 hash ?
OxC3 hash ?

echo|set /p="Hello world"> mytest.tmp.txt
echo )OxC3 hash file -format CRC32C -input mytest.tmp.txt
OxC3 hash file -format CRC32C -input mytest.tmp.txt

echo )OxC3 hash file -format SHA256 -input mytest.tmp.txt
OxC3 hash file -format SHA256 -input mytest.tmp.txt

echo )OxC3 hash string -format CRC32C -input "Hello world"
OxC3 hash string -format CRC32C -input "Hello world"

echo )OxC3 hash string -format SHA256 -input "Hello world"
OxC3 hash string -format SHA256 -input "Hello world"

REM test file

echo )OxC3 file ?
OxC3 file ?

echo )OxC3 file encr -input mytest.tmp.txt -output mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE
OxC3 file encr -input mytest.tmp.txt -output mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE

echo )OxC3 file decr -output mytest1.tmp.txt -input mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE
OxC3 file decr -output mytest1.tmp.txt -input mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE

fc mytest.tmp.txt mytest1.tmp.txt > nul
if errorlevel 1 goto error

echo )OxC3 file header -input mytest.tmp.txt.enc
OxC3 file header -input mytest.tmp.txt.enc

echo )OxC3 file data -input mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE
OxC3 file data -input mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE

echo )OxC3 file data -input mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE -entry 000
OxC3 file data -input mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE -entry 000

del /F mytest.tmp.txt
del /F mytest1.tmp.txt
del /F mytest.tmp.txt.enc

echo Test reached end. Please double check output to ensure everything is correct.
goto :eof

error:
echo Failed encryption test!