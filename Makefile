all:
	# default
	gcc flexIQdata2.c -o flexIQdata2 -lm -lfftw3 -lfftw3f

debug:
	# add debug symbols for gdb
	gcc -g -D DEBUG flexIQdata2.c -o flexIQdata2 -lm -lfftw3 -lfftw3f

datadump:
	# dump incoming IQ samples on console
	gcc -g -D DEBUG -D DATADUMP flexIQdata2.c -o flexIQdata2 -lm -lfftw3 -lfftw3f
