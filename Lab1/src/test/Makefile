sudoku_solve: main.o neighbor.o sudoku_basic.o sudoku_dancing_links.o  
	g++ main.o neighbor.o sudoku_basic.o sudoku_dancing_links.o -lpthread -O2 -o sudoku_solve

sudoku_test:main_test.o neighbor.o sudoku_basic.o sudoku_dancing_links.o
	g++ main1.o neighbor.o sudoku_basic.o sudoku_dancing_links.o -lpthread -O2 -o sudoku_test

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

clean_sudoku_test:
	rm sudoku_test