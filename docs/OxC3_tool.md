# OxC3 tool

The OxC3 tool is intended to handle all operations required for Oxsomi core3. This includes:

- Calculating hashes.
- Generating random numbers or keys.
- Conversions between file formats.
- **TODO:** Packaging a project.
- Inspecting a file (printing the header and other important information).
- Encryption.
- **TODO**: Compression.

And might include more functionality in the future.

To get info about a certain category or operation you can type `-? or ?` or any unrecognized command after it. Example; `OxC3 ?` will show all categories. `OxC3 file ?` will show all file operations. `OxC3 file to ?` will show either all formats (if the operation supports formats) or all supported flags/arguments. If the operation supports formats then `OxC3 file to -f <format> ?` could be used.

## Calculating hashes

A hash can be calculated as following:

`OxC3 hash file -f SHA256 -i myDialog.txt`

Where the format can either be `CRC32C` or `SHA256`.

A hash from a string can be calculated like so:

`OxC3 hash string -f CRC32C -i "This is my input string"`

## Random

Random number generation is handy for multiple things. CSPRNG (cryptographically secure PRNG) is chosen by default. To generate multiple entries; use `-n <count>`. To output to a file use `-o <file>`.

`OxC3 rand key`

Generates a CSPRNG key that can be used for AES256 encryption. You can use `-l <lengthInBytes>` to customize byte count; defaulted to 32.

`OxC3 rand char`

Generates random chars; 32 by default. `-l <charCount>` can be used to customize length. The included characters by default are viable ASCII characters (<0x20, 0x7F>). In the future --utf8 will be an option, but not for now (**TODO**:). `-c <chars>` can be used to pick between characters; ex. `-c 0123456789` will create a random number. This also allows picking the same character multiple times (e.g. -c011 will have 2x more chance to pick 1 instead of 0). Some helpful flags: --alpha (A-Za-z), --numbers (0-9), --alphanumeric (0-9A-Za-z), --lowercase (a-z), --uppercase (A-Z), --symbols (everything excluding alphanumeric that's ASCII). If either of these flags are specified, it'll not use the valid ascii range but rather combine it (so -c ABC --number would be ABC0123456789). E.g. --uppercase --number can be used to generate 0-9A-Z. If for example --alphanumeric --alpha is used, it will cancel out.

`OxC3 rand num`

Is just shorthand for `OxC3 rand char -c <numberKeyset>`. If --hex is used, it'll use 0-9A-Z, if --nyto is used it'll use 0-9a-zA-Z_$, if --oct is used it'll use 0-7, if --bin is used it'll use 0-1. Decimal is the default (0-9). `-l <charCount>` can be used to set a limit by character count and `-b <bitCount>` can be used to limit how many bits the number can have (for decimal output this can only be used with 64-bit numbers and below). 

`OxC3 rand data -l 16 -o myFile.bin`

Allows to output random bytes to the binary. Alias to `OxC3 rand key` but to a binary file. -l can be used to tweak number of bytes. -n will added multiple generations appended in the same file. Without -o it will output a hexdump.

## Convert

`OxC3 file` is the category that is used to convert between file formats. The keywords `from` and `to` can be used to convert between native and non native files. For example:

`OxC3 file to -f oiDL -i myDialog.txt -o myDialog.oiDL --ascii`

Will convert the enter separated string in myDialog into a DL file (where each entry is a separate string). This can also combine multiple files into one DL file:

`OxC3 file to -f oiDL -i myFolder -o myFolder.oiDL`

This will package all files from myFolder into a nameless archive file. These files can be accessed by file id. 

To unpackage this (losing the file names of course):

`OxC3 file from -f oiDL -i myFolder.oiDL -o myFolder`

### Common arguments

The following flags are commonly used in any format:

- --sha256: Includes 256-bit hashes instead of 32-bit ones into file if applicable.
  - If a file is compressed or encrypted, a hash is used to detect if it hasn't been corrupted or tempered with. A CRC32 (if this option is turned off) is insufficient if dealing with smart intermediates instead of uncompression/unencryption errors. 256-bit hash is sufficient at minimizing this risk (possible issue with quantum computers in the future).
- --uncompressed: Keep the data uncompressed (default is compressed).
  - By default, the data stored in the native format is compressed. If you're testing a custom file parser or want to inspect the data generated, it could be nice to test it like this.
- --fast-compress: Picks the compression algorithm that takes the least time to compress.
  - If a lot of files are compressed and need to be available quickly, then this option can be used. It's off by default, because this does impact how compact the file will be. Normally it optimizes for storage rather than speed. For example offline asset baking is when you don't want this turned on. If this is not present, it'll use brotli:11, otherwise brotli:1 will be used.
- --not-recursive: If folder is selected, blocks recursive file searching. Can be handy if only the direct directory should be included.
- **TODO**: --v: Verbose.
- **TODO**: --q: Quiet.

The following parameters are commonly used in any format:

- `-f <fileFormat>`: File format
  - Specifies the file format that has to be converted from / to. It doesn't detect this from file because this allows you to supply .bin files or other custom extensions.
- `-i <inputPath>`: Input file/folder (relative)
  - Specifies the input path. This is relative to the current working directory. You can provide an absolute path, but this will have to be located inside the current working directory. Otherwise you'll get an unauthorized error. Depending on the format, this can be either a file or a folder. Which will have to be one of the supported types. This format is detected based on the magic number or file extension (if magic number isn't applicable).
- `-o <outputPath`>: Output file/folder (relative)
  - See -i.
- `-aes <key>`: Encryption key (32-byte hex)
  - A key should be generated using a good key generator. This key could for example be specified as: `0x00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000` in hex (64 characters). Can include spaces or tabs, as long as after that it's a valid hex number.

### oiDL format

Since a DL (Data List) file can include either binary files or strings, you'll have to specify what.

- --ascii: Indicates the input files should be threated as ascii. If a file; splits by enter, otherwise 1 entry/file.
- --utf8: Indicates the input files should be threated as UTF8. If a file; splits by enter, otherwise 1 entry/file.
- `-split <string>`: If the input is a string, allows you to split the file by a different string than an endline character / string.

If these are absent; it'll use binary format by default. When either ascii or utf8 is used, file(s) should be encoded using the proper format that is requested. If only one file is used, multiple strings will be created by splitting by the newline character(s) or the split string if overriden. Recombining an ascii oiDL will resolve to a .txt if split is available or the final destination ends with .txt.

*Example usage:*

`OxC3 file to -f oiDL -i myDialog0.txt -o myDialog.oiDL --ascii`

`OxC3 file from -f oiDL -o myDialog1.txt -i myDialog.oiDL --ascii`

### oiCA format

A CA (Compressed Archive) file can include either no, partial or full timestamps, you'll have to specify if you want that:

- --full-date: Includes full file timestamp (Ns).
  - Creates a full 64-bit timestamp in nanoseconds of the file (in the resolution that the underlying filesystem supports).
- --date: Includes MS-DOS timestamp (YYYY-MM-dd HH-mm-ss (each two seconds)).
  - Creates a 2x 16-bit (32-bit) timestamp with 2s accuracy. This is nice for optimization, but keep in mind that the resolution is limited. This date will run out in 2107 and can only support 1980 and up. Full 64-bit has up to nanosecond precision and can support 1970-2553.

These are left out by default, because often, file timestamps aren't very important when dealing with things like packaging for apps. The option is still left there for specific use cases.

*Example usage:*

`OxC3 file to -f oiCA -i myFolder -o myFolder.oiCA --full-date`

## TODO: Packaging a project

## File inspect

Two operations constitute as file inspection: `file header` and `file data`.

Header is useful to know what the header says. It also allows to inspect the header of certain subsections of the data. This only works on Oxsomi formats.

Data allows you to actually inspect the data section of certain parts of the file.

`file header -i test.oiCA` would print the information about the file header.

`file data -i test.oiCA` would tell about the file table for example. File data also needs to provide `-aes` if the source is encrypted. If entry is absent, it will provide a general view of the file.

With `-e <offset or path>` a specific entry can be viewed. If an entry is specified, the `-o` can be used to extract that one entry into a single file (or folder). If this is not specified, it will show the file as either a hexdump or plain text (if it's ascii) or the folder's subdirectories. 

`file data` also allows the `-l` specifier for how many entries are shown. Normally in the log it limits to 64 lines (so for data that'd mean 64 * 64 / 2 (hex) = 2KiB per view). The `-s` argument can be used to set an offset of what it should show. An example: we have an oiCA with 128 entries but want to show the last 32; `file data -i our.oiCA -s 96 -l 32`. For a file entry, it would specify the byte offset and length (length is defaulted to 2KiB). If `-o` is used, it will binary dump the entire remainder of the file if `-l` is not specified (the remainder is the entire file size if `-s` is not specified). 

## Encrypt

`OxC3 file encr -i <file> -k <key in hex> (optional: -o output)`  

`OxC3 file decr -i <file> -o <file> -k <key in hex> `

Generates an encrypted oiCA file. Works on files and folders.

## TODO: Compress

`OxC3 file pack -f <file> (optional: --fast-compress, --k <key in hex>)`

`OxC3 file unpack -f <file> (optional: --k <key in hex>)`

Generates a compressed oiCA file. Can be encrypted. Works on files and folders.
