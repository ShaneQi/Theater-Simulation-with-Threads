#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <semaphore.h>
#include <unistd.h>

/* Declare variables. */
char movies_name[100][100];
int movies_tickets[100];
int movies_count;
int movie_to_watch[100];
int sell_result[100];
int ticket_taker_serving;
int concession_worker_serving;
int current_customer_order;

/* Box office queue. */
int box_office_queue[100];
int box_office_queue_head;
int box_office_queue_tail;

/* Declare threads' routes. */
void* customer(void* arg);
void* box_office_agent(void* arg);
void* ticket_taker(void* arg);
void* concession_worker(void* arg);

/* Declare semaphores. */
sem_t box_office;
sem_t box_office_queue_mutex;
sem_t customer_ready;
sem_t tickets_mutex;
sem_t sell_ticket_finished[100];
sem_t ready_for_tearing_ticket;
sem_t ticket_taker_ready;
sem_t ticket_torn[100];
sem_t finish_ordering;
sem_t concession_worker_ready;
sem_t order_filled[100];

/* Semaphores that are used to check if init finished. */
sem_t inited;

/* Parse customer's order from int to String. 
 * 1 => Popcorn
 * 2 => Soda
 * 3 => Popcorn and Soda */
char* orderParse(int order) {
	switch (order) {
	case 1:
		return "Popcorn";
		break;
	case 2:
		return "Soda";
		break;
	default:
		return "Popcorn and Soda";
		break;
	}
}

int main() {
	/* Set random seeds. */
	srand(time(NULL));

	/* Setup queue pointers. */
	box_office_queue_head = 0;
	box_office_queue_tail = 0;

	/* Semaphore init. */
	sem_init(&box_office, 0, 0);
	sem_init(&box_office_queue_mutex, 0, 1);
	sem_init(&customer_ready, 0, 0);
	sem_init(&tickets_mutex, 0, 1);
	for (int i = 0; i < 100; i++) {
		sem_init(&sell_ticket_finished[i], 0, 0);
	}
	sem_init(&ready_for_tearing_ticket, 0, 0);
	sem_init(&ticket_taker_ready, 0, 0);
	for (int i = 0; i < 100; i++) {
		sem_init(&ticket_torn[i], 0, 0);
	}
	sem_init(&finish_ordering, 0, 0);
	sem_init(&concession_worker_ready, 0, 0);
	for (int i = 0; i < 100; i++) {
		sem_init(&order_filled[i], 0, 0);
	}

	sem_init(&inited, 0, 0);

	/* Load movie list. */
	FILE* fr;
	fr = fopen("movies.txt", "r");
	char line[1024];
	movies_count = 0;
	while (fgets(line, 1024, fr) != NULL) {
		int i = 0;
		while (line[i] != '\t') {
			movies_name[movies_count][i] = line[i];
			i++;
		}
		int k = i + 1;
		char movie_count[sizeof(int)];
		while (line[k] != '\n') {
			movie_count[k-i-1] = line[k];
			k++;
		}
		movies_tickets[movies_count] = atoi(movie_count);
		movies_count++;
	}

	/* Init box office agents. */
	pthread_t box_office_agents[2];
	for (int i = 0; i < 2; i++) {
		int* agent_number = (int *)malloc(sizeof(int));
		*agent_number = i;
		pthread_create(&box_office_agents[*agent_number], NULL, box_office_agent, agent_number);
	}

	/* Init ticket taker. */
	pthread_t ticket_taker_t;
	pthread_create(&ticket_taker_t, NULL, ticket_taker, NULL);

	/* Init consession worker. */
	pthread_t consession_worker_t;
	pthread_create(&consession_worker_t, NULL, concession_worker, NULL);

	/* Theater doesn't open until init is finished. */
	sem_wait(&inited);	
	sem_wait(&inited);	
	sem_wait(&inited);	
	sem_wait(&inited);	

	/* Theater opens. */
	printf("Theater is open.\n");

	/* Create customers. */
	pthread_t customers[50];
	for (int i = 0; i < 50; i++) {
		int* customer_number = (int*)malloc(sizeof(int));
		*customer_number = i;
		pthread_create(&customers[*customer_number], NULL, customer, customer_number);
	}

	/* Wait for all customers finish. */
	for (int i = 0; i < 50; i++) {
		pthread_join(customers[i], NULL);
		printf("Joined customer %d.\n", i);
	}
}

void* customer(void* arg) {
	/* Assign customer number. */
	int customer_number = * (int*) arg;
	/* Decide which movie to watch. */
	int movie = rand()%movies_count;
	movie_to_watch[customer_number] = movie;

	/* Assign customer to agent. */
	sem_wait(&box_office);
	sem_wait(&box_office_queue_mutex);
	/* Enqueue to box office queue. */
	box_office_queue[box_office_queue_tail++] = customer_number;
	sem_post(&customer_ready);
	printf("Customer %d created, buying ticket to %s.\n" ,customer_number, movies_name[movie]);
	sem_post(&box_office_queue_mutex);
	/* Wait for finish selling ticket. */
	sem_wait(&sell_ticket_finished[customer_number]);
	if (sell_result[customer_number] == 0) {
		/* If failed buying ticket, leave theater. */
		printf("Customer %d left theater(disappointed).\n", customer_number);
		return NULL;
	}

	/* If bought ticket successfully, go on to see ticket taker. */
	printf("Customer %d in line to see ticket taker.\n", customer_number);
	sem_wait(&ticket_taker_ready);
	ticket_taker_serving = customer_number;
	sem_post(&ready_for_tearing_ticket);
	sem_wait(&ticket_torn[customer_number]);

	/* Decide if go to concession. 
	 * 1 => go
	 * 0 => not go */
	int will_goto_concession = rand()%2;
	if (will_goto_concession) {
		/* Decide order.
		 * 1 => popcorn 
		 * 2 => soda
		 * 0 => both */
		int customer_order = rand()%3;
		printf("Customer %d in line to buy %s.\n", customer_number, orderParse(customer_order));
		sem_wait(&concession_worker_ready);
		concession_worker_serving = customer_number;
		current_customer_order = customer_order;
		sem_post(&finish_ordering);
		sem_wait(&order_filled[customer_number]);
		printf("Customer %d receives %s.\n", customer_number, orderParse(customer_order));
	}

	printf("Customer %d enters theater to see %s.\n", customer_number, movies_name[movie]);
	return NULL;
}

void* box_office_agent(void* arg) {
	/* Init the box office agent. */
	int agent_number = * (int*) arg;
	int serve_customer;
	printf("Box office agent %d created.\n", agent_number);
	sem_post(&inited);

	while (1) {
		sem_post(&box_office);
		sem_wait(&customer_ready);
		sem_wait(&box_office_queue_mutex);

		/* Dequeue customer. */
		serve_customer = box_office_queue[box_office_queue_head++];

		/* Begin serving. */
		printf("Box office agent %d serving customer %d.\n", agent_number, serve_customer);
		sem_post(&box_office_queue_mutex);
		sem_wait(&tickets_mutex);

		/* Check whether the ticket is sold out. */
		if (movies_tickets[movie_to_watch[serve_customer]] > 0) {
			movies_tickets[movie_to_watch[serve_customer]]--;
			sem_post(&tickets_mutex);
			/* If not sold out, sell it. This tooks 90s. */
			sleep(90/60);
			printf("Box office agent %d sold ticket for %s to customer %d.\n", agent_number, movies_name[movie_to_watch[serve_customer]], serve_customer);
			/* sell_result: 1 -> success
											0 -> fail			 */
			sell_result[serve_customer] = 1;
		} else {
			sem_post(&tickets_mutex);
			/* If sold out, communicate to customer without 90s to sell ticket. */
			printf("Box office agent %d can't sell ticket to customer %d, because tickets for %s are sold out.\n", agent_number, serve_customer, movies_name[movie_to_watch[serve_customer]]);
			sell_result[serve_customer] = 0;
		}

		/* Finish serving a customer. */
		sem_post(&sell_ticket_finished[serve_customer]);
	}
	return NULL;
}

void* ticket_taker(void* arg) {
	int serve_customer;
	printf("Ticket taker created.\n");
	sem_post(&inited);
	while (1) {
		sem_post(&ticket_taker_ready);
		/* Get the customer number which the ticket taker is serving. */
		sem_wait(&ready_for_tearing_ticket);
		serve_customer = ticket_taker_serving;
		/* Tear ticket. */
		sleep(15/60);
		printf("Ticket taken from customer %d.\n", serve_customer);
		sem_post(&ticket_torn[serve_customer]);
	}
}

void* concession_worker(void* arg) {
	int serve_customer;
	int fill_order;
	printf("Concession stand worker created.\n");
	sem_post(&inited);
	while (1) {
		sem_post(&concession_worker_ready);
		/* Get the customer number which the concession worker is serving. */
		sem_wait(&finish_ordering);
		serve_customer = concession_worker_serving;
		fill_order = current_customer_order;
		printf("Order for %s taken from customer %d.\n", orderParse(fill_order), serve_customer);
		/* Fill order. */
		sleep(180/60);
		printf("%s given to customer %d.\n", orderParse(fill_order), serve_customer);
		sem_post(&order_filled[serve_customer]);
	}
}
