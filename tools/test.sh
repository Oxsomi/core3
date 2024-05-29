
if [ "$(expr substr $(uname -s) 1 10)" == "MINGW64_NT" ]; then
	executableName=./OxC3.exe
else 
	executableName=./OxC3
fi

echo \)./OxC3
$executableName

# Test rand

echo \)./OxC3 rand ?
$executableName rand ?

echo \)./OxC3 rand key -count 5
$executableName rand key -count 5

echo \)./OxC3 rand char -count 5 -length 32 --alphanumeric
$executableName rand char -count 5 -length 32 --alphanumeric

echo \)./OxC3 rand num -length 64 -bits 32
$executableName rand num -length 64 -bits 32

echo \)./OxC3 rand data -length 256
$executableName rand data -length 256

# test hash

echo \)./OxC3 hash ?
$executableName hash ?

echo "Hello world" > mytest.tmp.txt
echo \)./OxC3 hash file -format CRC32C -input mytest.tmp.txt
$executableName hash file -format CRC32C -input mytest.tmp.txt

echo \)./OxC3 hash file -format SHA256 -input mytest.tmp.txt
$executableName hash file -format SHA256 -input mytest.tmp.txt

echo \)./OxC3 hash string -format CRC32C -input "Hello world"
$executableName hash string -format CRC32C -input "Hello world"

echo \)./OxC3 hash string -format SHA256 -input "Hello world"
$executableName hash string -format SHA256 -input "Hello world"

# test file

echo \)./OxC3 file ?
$executableName file ?

echo \)./OxC3 file encr -input mytest.tmp.txt -output mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE
$executableName file encr -input mytest.tmp.txt -output mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE

echo \)./OxC3 file decr -output mytest1.tmp.txt -input mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE
$executableName file decr -output mytest1.tmp.txt -input mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE

if ! cmp -s "mytest.tmp.txt" "mytest1.tmp.txt" ; then
	echo Failed encryption test!
	rm mytest.tmp.txt
	rm mytest1.tmp.txt
	rm mytest.tmp.txt.enc
	exit 1
fi

echo \)./OxC3 file header -input mytest.tmp.txt.enc
$executableName file header -input mytest.tmp.txt.enc

echo \)./OxC3 file data -input mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE
$executableName file data -input mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE

echo \)./OxC3 file data -input mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE -entry 000
$executableName file data -input mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE -entry 000

rm mytest.tmp.txt
rm mytest1.tmp.txt
rm mytest.tmp.txt.enc

echo Test reached end. Please double check output to ensure everything is correct.