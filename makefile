all: dev serv

dev: dev.o
	 	gcc -Wall dev.o -o dev

serv: serv.o
	  	gcc -Wall serv.o -o serv

clear:
	rm *o dev serv 
	
