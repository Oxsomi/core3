echo \)./OxC3
./OxC3

# Test rand

echo \)./OxC3 rand ?
./OxC3 rand ?

echo \)./OxC3 rand key -n 5
./OxC3 rand key -n 5

echo \)./OxC3 rand char -n 5 -l 32 --alphanumeric
./OxC3 rand char -n 5 -l 32 --alphanumeric

echo \)./OxC3 rand num -l 64 -b 32
./OxC3 rand num -l 64 -b 32

echo \)./OxC3 rand data -l 256
./OxC3 rand data -l 256

# test hash

echo \)./OxC3 hash ?
./OxC3 hash ?

echo "Hello world" > mytest.tmp.txt
echo \)./OxC3 hash file -f CRC32C -i mytest.tmp.txt
./OxC3 hash file -f CRC32C -i mytest.tmp.txt

echo \)./OxC3 hash file -f SHA256 -i mytest.tmp.txt
./OxC3 hash file -f SHA256 -i mytest.tmp.txt

echo \)./OxC3 hash string -f CRC32C -i "Hello world"
./OxC3 hash string -f CRC32C -i "Hello world"

echo \)./OxC3 hash string -f SHA256 -i "Hello world"
./OxC3 hash string -f SHA256 -i "Hello world"

# test file

echo \)./OxC3 file ?
./OxC3 file ?

echo \)./OxC3 file encr -i mytest.tmp.txt -o mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE
./OxC3 file encr -i mytest.tmp.txt -o mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE

echo \)./OxC3 file decr -o mytest1.tmp.txt -i mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE
./OxC3 file decr -o mytest1.tmp.txt -i mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE

if ! [ cmp -s "mytest.tmp.txt" "mytest1.tmp.txt" ] ; then
	echo Failed encryption test!
	rm mytest.tmp.txt
	rm mytest1.tmp.txt
	rm mytest.tmp.txt.enc
	exit 1
fi

echo \)./OxC3 file header -i mytest.tmp.txt.enc
./OxC3 file header -i mytest.tmp.txt.enc

echo \)./OxC3 file data -i mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE
./OxC3 file data -i mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE

echo \)./OxC3 file data -i mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE -e 000
./OxC3 file data -i mytest.tmp.txt.enc -aes CD00324F4CBAAE3467924E0578012F155A8F573AA066652DDDB8C2E1F76AF7FE -e 000

rm mytest.tmp.txt
rm mytest1.tmp.txt
rm mytest.tmp.txt.enc

echo Test reached end. Please double check output to ensure everything is correct.