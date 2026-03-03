.PHONY: build run

build:
	gcc -g -Ilibtdmm main.c libtdmm/tdmm.c -o hw7
	@echo "build done"
run:
	./hw7
valgrind:
	valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all ./hw7