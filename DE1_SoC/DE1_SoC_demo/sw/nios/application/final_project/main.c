//  main.c
//  rtes_project

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include "io.h"
#include <stdio.h>
#include "system.h"
#include "altera_avalon_performance_counter.h"

//DEFINES TO INTERACT WITH THE CODE
int SAVE_LAST_STATES = 3; //3 saves the last (2^3)-1 states --> 7
#define FIRST_LENGTH 32
#define ACCELERATOR_FOR_PLOTTING 32
#define MAX_RAND 1000
//#define WRONG_ORDER_DIE
int first_run = 0;

//hard coded addresses to communicate with HPS
//REMEMBER THAT THE STATES ARE SHORT VARIABLES
#define ADDR_LENGTH			0x577DC		//int					IN
#define ADDR_BASE_STATES 	0x577E0		//pointer SHORT			OUT
#define ADDR_STATES_LENGTH 	0x577E4		//int					OUT
#define ADDR_SPEED_0v16 	0x577E8 	//float					OUT
#define ADDR_SPEED_16v32 	0x577EC 	//float					OUT
#define ADDR_SPEED_0v32 	0x577F0 	//float					OUT
#define ADDR_TIME_00 		0x577F4 	//float in ms			OUT
#define ADDR_TIME_16 		0x577F8 	//float in ms			OUT
#define ADDR_TIME_32 		0x577FC 	//float in ms			OUT

#define ON 1
#define OFF 0
#define RESET 1
#define KEEP 0
#define LED_START 0
#define LED_RUNNING 1
#define LED_FINISH 2
#define LED_PLOTMODE 3
#define LED_ASYM 4
#define LED_CLOCK50 5
#define LED_PLOT_ACCEL 6

//INPUT VARIABLES
char PLOT_MODE;
int LENGTH;
char ACCELERATE_PLOT;
char ASYMMETRIC_SORT;
char CLOCK_50;

int filter; //we save the state every FILTER calls
int* array = NULL;
short* states = NULL;
int states_count;
int expected_states;
int counter_recursion; //we should have n-1 calls (because the n=1 nodes don't count)
					   //the total number of calls is 2(n-1)+1

void print_array_s(short* arr, int dim){
	int i;
	printf("{");
	for (i=0; i<dim-1; i++){
		printf("%d,",*(arr+i));
	}
	printf("%d}\n",*(arr+dim-1));
}
void print_array_i(int* arr, int dim){
	int i;
	printf("{");
	for (i=0; i<dim-1; i++){
		printf("%d,",*(arr+i));
	}
	printf("%d}\n",*(arr+dim-1));
}

void swap(int* a, int* b){
	int c = *a;
	*a = *b;
	*b = c;
}

//insertion sort
void fix_order_i(int* array, int dim){
	int i;
	for (i=1; i<dim; i++){
		if (*(array+i-1)>*(array+i)){
			swap(array+i-1, array+i);
			int j = i-1;
			char ordered = 0;
			while (!ordered && j > 0){
				if (*(array+j-1)>*(array+j)){
					swap(array+j-1, array+j);
					j--;
				}
				else
					ordered = 1;
			}
		}
	}
}

void test_ordered_i(int* array, int dim){
	int i;
	for (i=1; i<dim; i++){
		if (*(array+i-1)>*(array+i)){
			//print_array_i(array,dim);
			printf("%d>%d ",*(array+i-1),*(array+i));
			printf("WRONG ORDER\n");
#ifdef WRONG_ORDER_DIE
			exit(-1);
#endif
		}
	}
}

void test_ordered_s(short* array, int dim){
	int i;
	for (i=1; i<dim; i++){
		if (*(array+i-1)>*(array+i)){
			//print_array_s(array,dim);
			printf("%d>%d ",*(array+i-1),*(array+i));
			printf("WRONG ORDER\n");
#ifdef WRONG_ORDER_DIE
			exit(-1);
#endif
		}
	}
}

/**
 * Read the leds and put the result in an array
 *
 * @param array - array where to put the result
 *
 * @return decimal value of leds
 *
 **/
int get_leds(short array[]) {
	int value = IORD_32DIRECT(MUTEX_LEDS_BASE,0); //read the leds register
	int carry = value;
	int remainder;
	short i = 0;

	while(carry != 0) {
		remainder = carry%2;
		carry = carry/2;
		array[i] = remainder;
		i++;
	}

	while(i < MUTEX_LEDS_DATA_WIDTH){ //fill the remaining fields
		array[i]= 0;
		i++;
	}
	return value;
}

/**
 * Read the led at the given position
 *
 * @param pos - position of the led
 *
 * @return value (0 or 1)
 *
 **/
short get_led(short pos){
	short temp[MUTEX_LEDS_DATA_WIDTH];
	get_leds(temp);
	return temp[pos];
}

/**
 * Set the led at the given position
 *
 * @param pos   - position of the led
 * @param value - value to set
 *
 **/
void set_led(short pos, short value, short reset){
	int leds_dec = 0; //decimal value of leds
	if (!reset){ //if don t want to reset, read the actual state
		short temp[MUTEX_LEDS_DATA_WIDTH];
		leds_dec = get_leds(temp);
		if (temp[pos] != value){ //write only if needed
			switch (temp[pos]) {
				case 0: //LED OFF, TURN IT ON
					leds_dec += pow(2,pos); //add the value
					break;
				case 1: //LED  ON, TURN IT OFF
					leds_dec -= pow(2,pos); //else substract it
					break;
			}
		}
	}
	else
		leds_dec = value * pow(2,pos);
	//finally write the result in the register
	IOWR_32DIRECT(MUTEX_LEDS_BASE,0,leds_dec);
}

/**
 * Clone n elements from src array to dst array
 *
 * @param src - source array
 * @param dst - destination array
 * @param n   - number of elements to clone
 *
 **/
void clone_i(const int* src, int* dst, const int n){
	int i;
	for (i=0; i<n; i++)
		*(dst+i) = *(src+i);

}
void clone_s(const int* src, short* dst, const int n){
	int i;
	for (i=0; i<n; i++)
		*(dst+i) = (short) *(src+i);

}

void accelerate(int* num, int n){
	if (n == 16) {
		IOWR_32DIRECT(ACCELERATOR_16_0_BASE,0, num);  // RegAddStart
		IOWR_32DIRECT(ACCELERATOR_16_0_BASE, 2*4, 1); // 	Start

		// Active wait for finishing accelerator
		while((IORD_32DIRECT(ACCELERATOR_16_0_BASE, 3*4) & (1<<0)) == 0);// wait for the finish of the calculus
	}
	else if (CLOCK_50 && n == 32) { //UNDERCLOCKED
		IOWR_32DIRECT(ACCELERATOR_32_1_BASE,0, num);  // RegAddStart
		IOWR_32DIRECT(ACCELERATOR_32_1_BASE, 2*4, 1); // 	Start

		// Active wait for finishing accelerator
		while((IORD_32DIRECT(ACCELERATOR_32_1_BASE, 3*4) & (1<<0)) == 0);// wait for the finish of the calculus
	}
	else if (!CLOCK_50 && n == 32) { //NORMAL 100MHZ CLOCK
		IOWR_32DIRECT(ACCELERATOR_32_0_BASE,0, num);  // RegAddStart
		IOWR_32DIRECT(ACCELERATOR_32_0_BASE, 2*4, 1); // 	Start

		// Active wait for finishing accelerator
		while((IORD_32DIRECT(ACCELERATOR_32_0_BASE, 3*4) & (1<<0)) == 0);// wait for the finish of the calculus
	}
	else
		printf("ERROR ACCELERATION: array of %d variables", n);
}

/**
 * Save the actual state of the array only once every FILTER calls
 * Total calls of save_state: N-1
 * It saves by default states with n greater then a certain (big) number
 * It discards small states if the memory is ending
 *
 * @param n is the length of the array that called the saving
 **/
void save_state(int n){
	counter_recursion++; //number of calls, max length-1
	char recovered = 0;
	if(counter_recursion%filter != 0){ //save only one state every FILTER calls
		if(n < LENGTH/pow(2,SAVE_LAST_STATES)){ //save the last 2^X-1 states
			//printf("\n");
			return;
		}
		recovered = 1;
		//printf("RECOVERED ");
	}
	states_count++; //number of states recorded (not counted the first state that is the array raw)
	//printf("state #%d ", states_count);
	if (states_count >= (expected_states - pow(2,SAVE_LAST_STATES)) && filter > 1){ //then save only recovered states
		if (!recovered){
			//printf("DISCARDED\n");
			states_count--;
			return;
		}
	}
	//printf("\n");
	short* next_state = states+states_count*LENGTH;
	clone_s(array, next_state, LENGTH);
}

/**
 * Recursive function, applies mergesort on the passed array
 *
 * @param num - array to sort
 * @param n - lentgh of the array
 * @param depth - threshold to trigger the accelerator
 *
 **/
void merge_sort(int* num, const int n, const int depth){
	//printf("called merge_sort %d\n",n);
	if (n == 1 || n == 0)
		return;
	if (n == depth) //if accelerable, accelerate it
		accelerate(num, n);
	else{
		//divide part
		int l1, l2; //length of the two parts
		if(ASYMMETRIC_SORT){ //divide asymmetrically to improve acceleration
			if (n > depth && n < 2*depth){
				l1 = depth; //accelerate one part
				l2 = n - l1; //merge-sort the other
			}
			else{ //divide symmetrically
				l1 = n/2;
				l2 = n - l1;
			}
		}
		else{ //divide in two equal parts
			l1 = n/2;
			l2 = n - l1;
		}
		int p1[l1+1], p2[l2+1];
		merge_sort(num, l1, depth); //order first part
		merge_sort(num+l1, l2, depth); //order second part

		clone_i(num, p1, l1); //copy the first array (ordered)
		clone_i(num+l1, p2, l2); //copty the second array (ordered)

		//add flags at the end
		p1[l1] = INT_MAX;
		p2[l2] = INT_MAX;

		//impera part
		int c1 = 0; //counter of the first array
		int c2 = 0; //counter of the second array
		int count;
		for (count=0; count<n; count++){
			if (p1[c1] <= p2[c2]){
				*(num+count) = p1[c1];
				c1++;
			}
			else{
				*(num+count) = p2[c2];
				c2++;
			}
		}
	}
	if (PLOT_MODE){
		fix_order_i(num, n);
		//now this partial array is sorted, save the state of the all array
		if (n >= depth && n != 1) //don't save the little states
			save_state(n);
	}
}

int get_length(){
	usleep(100000); //wait 100ms
	return IORD_32DIRECT(ADDR_LENGTH,0);
}

void read_inputs(){
	if(first_run){
		first_run = 0;
		LENGTH = FIRST_LENGTH;
	}
	else
		LENGTH = get_length();
	printf("LENGTH: %d\n", LENGTH);
	PLOT_MODE = get_led(LED_PLOTMODE);
	if(PLOT_MODE)
		printf("PLOT_MODE\n");
	ASYMMETRIC_SORT = get_led(LED_ASYM);
	if(ASYMMETRIC_SORT)
		printf("ASYMMETRICAL_SORT\n");
	CLOCK_50 = get_led(LED_CLOCK50); //read the plot mode
	if(CLOCK_50)
		printf("CLOCK_50\n");
	ACCELERATE_PLOT = get_led(LED_PLOT_ACCEL); //read the plot mode
	if(ACCELERATE_PLOT)
	printf("ACCELERATE_PLOT\n");
}

void waiting(){
	static int counter_led = 0;
	static char led_on = 1;
	static char asc = 1;
	counter_led++;
	counter_led = counter_led%5000; //5.000 clocks per led
	if (counter_led == 0){
		set_led(led_on,ON,RESET);
		if (asc)
			led_on++; //led_on goes from 1 to 9;
		else
			led_on--;
		if (led_on == 9 && asc)
			asc = 0;
		if (led_on == 1 && !asc)
			asc = 1;
	}
}

int main(){
	set_led(LED_START,OFF,RESET); //clear leds
	if (first_run)
		set_led(LED_START,ON,RESET); //set start
	//set_led(LED_PLOTMODE,ON,KEEP); //set plot_mode
	//set_led(LED_PLOT_ACCEL,ON,KEEP); //set accelerated plot

	while(1){
		//waiting for the led 0 to start
		printf("Waiting to start....\n");
		while(!get_led(LED_START))
			waiting();
		printf("Start!\n");

		//running mode
		set_led(LED_START, OFF, KEEP);
		set_led(LED_RUNNING,ON,KEEP);

		//clean possible remaining from before
		free(array); array = NULL;
		free(states); states = NULL;
		filter = 0;
		states_count = 0;
		counter_recursion = 0;
		expected_states = 0;
		int total_memory = 0;

		//read initial variables
		read_inputs();

		// malloc in bytes
		array = malloc(LENGTH*sizeof(int)); //LENGTH int vars (4 bytes x var)
		total_memory += LENGTH*sizeof(int);
		// check if malloc worked
		if (array == NULL) {
			printf("Allocation of buffer memory space failed\n");
			return -1;
		}

		if (PLOT_MODE){
			if(ACCELERATE_PLOT){
				while(states == NULL){ //increase filter until the states fit in memory
					filter++;
					expected_states = (LENGTH/16-pow(2,SAVE_LAST_STATES))/filter + pow(2,SAVE_LAST_STATES) +1; //always save the last N steps
					states = malloc(expected_states*LENGTH*sizeof(short)); // expected_states arrays of length short vars (2 bytes x var)
					if (LENGTH > 2000){
						//filter = INT_MAX-1;
						SAVE_LAST_STATES = 2;
					}
					if (LENGTH > 4000)
						filter = INT_MAX-1;
						SAVE_LAST_STATES = 1;
				}
			}
			else{
				while(states == NULL){ //increase filter until the states fit in memory
					filter++;
					expected_states = LENGTH/filter + pow(2,SAVE_LAST_STATES) +1; //always save the last N steps
					states = malloc(expected_states*LENGTH*sizeof(short)); // expected_states arrays of length short vars (2 bytes x var)
					if (LENGTH > 5000){
						filter = INT_MAX-1;
						SAVE_LAST_STATES = 1;
					}
				}
			}

			total_memory += expected_states*LENGTH*sizeof(short);
			printf("Filter choosen: 1/%d\n", filter);
			printf("Expected %d states\n", expected_states);
			printf("Total memory allocated for STATES: %fKB\n", expected_states*LENGTH/1000.0f*sizeof(short));
		}

		printf("Total memory allocated for ARRAY: %fKB\n", LENGTH/1000.0f*sizeof(int));
		printf("Total memory allocated %fKB\n", total_memory/1000.0f);

		printf("----------------------------------\n");

		//create array - random distribution from 0 to MAX_RAND
		int i;
		for (i=0; i<LENGTH; i++){
			*(array+i) = rand()%MAX_RAND;
		}

		if (PLOT_MODE){
			clone_s(array, states, LENGTH); //the initial array is the first state

			if(ACCELERATE_PLOT){
				merge_sort(array, LENGTH, ACCELERATOR_FOR_PLOTTING);
				printf("Checking result of the sort:\n");
				test_ordered_i(array,LENGTH);
			}
			else{
				merge_sort(array, LENGTH, 1);
				printf("Checking result of the sort:\n");
				test_ordered_i(array,LENGTH);
			}

//			printf("Table of states:\n");
//			int i,j;
//			for (i=0;i<= states_count;i++){
//				printf("%d ",i+1);
//				print_array_s(states+i*LENGTH,LENGTH);
//			}
			//check the final state
			//printf("Checking final state saved:\n");
			//test_ordered_s(states+states_count*LENGTH,LENGTH);

			//send the states to HPS
			IOWR_32DIRECT(ADDR_BASE_STATES,0,states); //states address
			IOWR_32DIRECT(ADDR_STATES_LENGTH,0,states_count+1); //number of states
																//+1 because one is the first state
																//ie the array created
			printf("Total states actually saved %d\n", states_count+1);
		}
		else{
			unsigned long long count_no_acc = 0, count_16_acc = 0, count_32_acc = 0;
			float time_00, time_16, time_32;
			unsigned long t0, t1, t2;
			float speed_0v16, speed_16v32, speed_0v32;
			int clone[LENGTH];
			clone_i(array,clone,LENGTH);

			// WITHOUT ACCELERATION
			PERF_RESET(PERFORMANCE_COUNTER_0_BASE);
			PERF_START_MEASURING(PERFORMANCE_COUNTER_0_BASE);
			merge_sort(clone, LENGTH, 1);
			PERF_STOP_MEASURING(PERFORMANCE_COUNTER_0_BASE);
			test_ordered_i(clone,LENGTH);
			count_no_acc = perf_get_section_time(PERFORMANCE_COUNTER_0_BASE, 0);
			//print_array_i(array, LENGTH); //print final array sorted
			time_00 = count_no_acc /(NIOS2_CPU_FREQ/1000.0f); //time in ms
			t0 = (unsigned long)(count_no_acc & 0xFFFFFFFF);
			printf("NO ACCELERATION: %llu cycles - %fms \n", count_no_acc, time_00);

			clone_i(array,clone,LENGTH);
			// 16 ACCELERATION
			if (LENGTH >= 16){
				PERF_RESET (PERFORMANCE_COUNTER_0_BASE);
				PERF_START_MEASURING (PERFORMANCE_COUNTER_0_BASE);
				merge_sort(clone, LENGTH, 16);
				PERF_STOP_MEASURING (PERFORMANCE_COUNTER_0_BASE);
				test_ordered_i(clone,LENGTH);
				count_16_acc = perf_get_section_time(PERFORMANCE_COUNTER_0_BASE, 0);
				time_16 = count_16_acc /(NIOS2_CPU_FREQ/1000.0f); //time in ms
				printf("16 ACCELERATION: %llu cycles - %fms \n", count_16_acc, time_16);
				t1 = (unsigned long)(count_16_acc & 0xFFFFFFFF);
				speed_0v16 = ((float)t0) / ((float)t1);
			}
			else{
				printf("16 ACCELERATION: disabled \n");
				time_16 = 0;
				speed_0v16 = 0;
			}

			clone_i(array,clone,LENGTH);
			// 32 ACCELERATION
			if (LENGTH >= 32){
				PERF_RESET (PERFORMANCE_COUNTER_0_BASE);
				PERF_START_MEASURING (PERFORMANCE_COUNTER_0_BASE);
				merge_sort(clone, LENGTH, 32);
				PERF_STOP_MEASURING (PERFORMANCE_COUNTER_0_BASE);
				//test_ordered_i(clone,LENGTH);
				count_32_acc = perf_get_section_time(PERFORMANCE_COUNTER_0_BASE, 0);
				time_32 = count_32_acc /(NIOS2_CPU_FREQ/1000.0f); //time in ms
				t2 = (unsigned long)(count_32_acc & 0xFFFFFFFF);
				printf("32 ACCELERATION: %llu cycles - %fms \n", count_32_acc, time_32);
				speed_0v32 = ((float)t0) / ((float)t2);
				speed_16v32 = ((float)t1) / ((float)t2);
			}
			else{
				printf("32 ACCELERATION: disabled \n");
				time_32 = 0;
				speed_0v32 = 0;
				speed_16v32 = 0;
			}

			//send to HPS the times (x10.000)
			IOWR_32DIRECT(ADDR_TIME_00,0,time_00*10000);
			IOWR_32DIRECT(ADDR_TIME_16,0,time_16*10000);
			IOWR_32DIRECT(ADDR_TIME_32,0,time_32*10000);

			//keep only 1 decimal
			speed_0v16 = (int) (speed_0v16*10);
			speed_16v32 = (int) (speed_16v32*10);
			speed_0v32 = (int) (speed_0v32*10);
			//send to HPS the speed ups (x1.000 we always pass 3 decimals)
			IOWR_32DIRECT(0x0,ADDR_SPEED_0v16,speed_0v16*100);
			IOWR_32DIRECT(ADDR_SPEED_16v32,0,speed_16v32*100);
			IOWR_32DIRECT(ADDR_SPEED_0v32,0,speed_0v32*100);

			printf("\n");
			printf("Speedup 1 vs 16:  %f \n",speed_0v16/10);
			printf("Speedup 16 vs 32: %f \n",speed_16v32/10);
			printf("Speedup 1 vs 32: %f \n",speed_0v32/10);
		}
		printf("----------------------------------\n\n\n\n");

		set_led(2,ON,RESET); //third led ON to say "finished"
	}
	return 0;
}
