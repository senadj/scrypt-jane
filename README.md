# yacutil 

Construct a linear, no-fork, best version of the blockchain; description taken from bitcoin contrib module [linearize](https://github.com/bitcoin/bitcoin/tree/master/contrib/linearize)
yacutil is also a cross platform tool for making blockchain file backup.
It is written in C++ using [wxWidgets](https://www.wxwidgets.org/) framework (no gui components are needed).


It is created as a fork of [floodyberry's scrypt-jane](https://github.com/floodyberry/scrypt-jane)


First I removed unneeded files:

* rm code/\*salsa\* code/\*blake\* code/\*skei\* code/\*sha\* code/scrypt-conf.h test-speed.sh test.sh scrypt-jane-test.c scrypt-jane-speed.c example.c


... and added some:

* scrypt-mine.h
* scrypt-mine.cpp
* yacutil.cpp




## Build on linux

Here is how I build it from source on ubuntu 12.04. 
For this experiment on ubuntu linux you will need build environment:


	sudo apt-get install build-essential wget tar git


	cd $HOME
	git clone https://github.com/senadj/scrypt-jane
	cd scrypt-jane

	wget https://sourceforge.net/projects/wxwindows/files/3.0.2/wxWidgets-3.0.2.tar.bz2
	bunzip2 wxWidgets-3.0.2.tar.bz2
	tar xvf wxWidgets-3.0.2.tar
	rm wxWidgets-3.0.2.tar
	mkdir wxWidgets-3.0.2/noGui
	cd wxWidgets-3.0.2/noGui
	../configure --prefix=$HOME/scrypt-jane/wx302 --disable-gui --disable-debug_flag
	make
	make install
	cd $HOME/scrypt-jane
	export PATH=./wx302/bin:$PATH


	gcc -msse2 -c scrypt-jane.c -O3 -DSCRYPT_CHACHA -DSCRYPT_KECCAK512 -DSCRYPT_CHOOSE_COMPILETIME
	`wx-config --cxx` -msse2 -c scrypt-mine.cpp -I. `wx-config --cxxflags`
	`wx-config --cxx` -c yacutil.cpp -I. `wx-config --cxxflags`
	`wx-config --cxx` -o yacutil scrypt-jane.o scrypt-mine.o yacutil.o `wx-config --libs base`

	cp ./wx302/lib/libwx_baseu-3.0.so.0 .

optional (smaller size)

	strip yacutil
	strip libwx_baseu-3.0.so.0


To run it, you will need your environment variable LD_LIBRARY_PATH set to directory containing libwx_baseu-3.0.so.0

	export LD_LIBRARY_PATH=.
	./yacutil


You could also sudo copy shared library to /usr/lib/libwx_baseu-3.0.so.0 and executable to /usr/bin/yacutil
All other files and directories are not needed and you can delete them.

In order to make a backup of *.blk file you need hashlist file. Here is how to create one in bash with yacoin daemon running:

	for i in {0..1000}; do yacoind getblockhash $i; done > sort.lst



## Usage

Let's say you have hashlist file sorted.lst with first 10 block hashes.
This is how you got them:

	yacoind getblockhash 0
	...
	yacoind getblockhash 9


Here is how it looks:

	0000060fc90618113cde415ead019a1052a9abc43afcccff38608ff8751353e5
	000000f260c39629d99355c5476d710d46ca0d35b3d962b44054b9ff943fe622
	000008d60d927a984d19b852348711f8d21c261b1247e61b44f307580f2ca6b6
	00000ddf62481bdcf37837e69945229b0c75b2b86983fff40b9f9bc353ac0dfc
	0000093de07e692058713c34e38ca1d5d062a77dcd1a037539e2e37a59d67969
	00000adac61d877a00131c2e6040819d065a088489f9cd10fa6e1e86a48a84ce
	000001a0560e06e02c11fd825c769275ac4f81235f75f590e5fec47470b51603
	0000013dec87e9d57ce8a0cb36611279bdf3e97d0a3c14172b7e7eccab5ce5c5
	0000007c03b70cf3004270ff01974c0e71e9b52e5676a056eb614499cb79c531
	000003f7840b36b38dacd4fdbca3c7574f3d324a6efa35d341cf6a662b92ffdb


You want to dump first 5 blocks from original blk0001.dat file to file named sorted.out
Those blocks must be somewhere at the beginning of file, so you command to terminate scan after 100 blocks:

	cd .yacoin
	yacutil blk0001.dat sort.lst sorted.out 1 5 -t 100


Now you realize that you would also like to include genesis block:
	
	yacutil blk0001.dat sort.lst sorted.out 0 5 -t 100
	

After you manage to export 500.000 hashes to sort.lst file, you want to backup 100 last of those blocks.
You are sure those blocks are somewhere after first third of file (33%), so you want to start searching after that.
Time is short and you will not scan blockchain file after 70000 blocks from there on. 
You might not find all those blocks now but what the hell ...

	
	yacutil blk0001.dat sort.lst sorted.out 499900 500000 33% -t 70000



