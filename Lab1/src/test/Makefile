sudoku_solve: main.o neighbor.o sudoku_basic.o sudoku_dancing_links.o  
	g++ main.o neighbor.o sudoku_basic.o sudoku_dancing_links.o -lpthread -O2 -o sudoku_solve

sudoku_test:main_test.o neighbor.o sudoku_basic.o sudoku_dancing_links.o
	g++ main_test.o neighbor.o sudoku_basic.o sudoku_dancing_links.o -lpthread -O2 -o sudoku_solve

v2:main_v2.o neighbor.o sudoku_basic.o sudoku_dancing_links.o
	g++ main_v2.o neighbor.o sudoku_basic.o sudoku_dancing_links.o -lpthread -O2 -o sudoku_solve

v3:main_v3.o neighbor.o sudoku_basic.o sudoku_dancing_links.o
	g++ main_v3.o neighbor.o sudoku_basic.o sudoku_dancing_links.o -lpthread -O2 -o sudoku_solve

v4:main_v4.o neighbor.o sudoku_basic.o sudoku_dancing_links.o
	g++ main_v4.o neighbor.o sudoku_basic.o sudoku_dancing_links.o -lpthread -O2 -o sudoku_solve

basic:main_original.cc neighbor.cc sudoku_basic.cc sudoku_min_arity.cc sudoku_min_arity_cache.cc sudoku_dancing_links.cc
	g++ -O2 -o $@ $^

clean: 
	rm *.o sudoku_solve

test:
	g++ test.cpp -o test

clean_test:
	rm test

clean_basic:
	rm basic
