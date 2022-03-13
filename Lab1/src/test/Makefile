sudoku_solve: main.o sudoku_dancing_links.o  
	g++ main.o sudoku_dancing_links.o -lpthread -O2 -o sudoku_solve

v1:main1.o sudoku_dancing_links.o
	g++ main1.o sudoku_dancing_links.o -lpthread -O2 -o sudoku_solve

basic:main_original.o sudoku_basic.o sudoku_min_arity.o sudoku_min_arity_cache.o sudoku_dancing_links.o
	g++ main_original.o sudoku_basic.o sudoku_min_arity.o sudoku_min_arity_cache.o sudoku_dancing_links.o -O2 -o sudoku_solve

sudoku:main_original.cc neighbor.cc sudoku_basic.cc sudoku_min_arity.cc sudoku_min_arity_cache.cc sudoku_dancing_links.cc
	g++ -O2 -o $@ $^

clean: 
	rm *.o sudoku_solve

test:
	g++ test.cpp -o test

clean_test:
	rm test