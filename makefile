all: clear output

test: clear debug

output: np_simple.cpp np_single_proc.cpp
	g++ -Werror -Wall -O3 np_simple.cpp -o np_simple
	g++ -Werror -Wall -O3 np_single_proc.cpp -o np_single_proc

debug: np_simple.cpp np_single_proc.cpp
	g++ -Werror -Wall -O3 -D DEBUG np_simple.cpp -o np_simple
	g++ -Werror -Wall -O3 np_single_proc.cpp -o np_single_proc

exec: 
	./np_simple
clear:
	rm -f np_simple
	rm -f np_single_proc