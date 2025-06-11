CFLAGS = -w

all:	cook waiter customer customers_file

customers_file:
	gcc $(CFLAGS) -o gencustomers gencustomers.c
	./gencustomers > customers.txt

cook:
	gcc $(CFLAGS) -o cook cook.c

waiter:	
	gcc $(CFLAGS) -o waiter waiter.c

customer:	
	gcc $(CFLAGS) -o customer customer.c

clean:
	rm -f cook waiter customer gencustomers
