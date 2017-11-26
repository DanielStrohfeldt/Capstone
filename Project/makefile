main : main.o
	gcc -o main main.o -lpthread -l bcm2835 

main.o : main.c
	gcc -c main.c
	
spi : spi.o
	gcc -o spi spi.o -l bcm2835

spi.o : spi.c
	gcc -c spi.c

clean :
	rm spi.o spi main.o main
