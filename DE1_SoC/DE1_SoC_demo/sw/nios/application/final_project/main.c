//  main.c
//  rtes_project

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include "io.h"
#include <stdio.h>
#include "system.h"
#include "altera_avalon_performance_counter.h"
//#include <alt_cache.h>

//hardcoded addresses to communicate with HPS
//REMEMBER THAT THE STATES ARE SHORT VARIABLES
#define ADDR_LENGTH 0x577F0				//int					IN
#define ADDR_BASE_STATES 0x577F1		//pointer SHORT			OUT
#define ADDR_STATES_LENGTH 0x577F2		//int					OUT
#define ADDR_SPEED_0v16 0x577F3 		//float					OUT
#define ADDR_SPEED_16v32 0x577F4 		//float					OUT
#define ADDR_SPEED_0v32 0x577F5 		//float					OUT
#define ADDR_TIME_00 0x577F6 			//float in ms			OUT
#define ADDR_TIME_16 0x577F7 			//float in ms			OUT
#define ADDR_TIME_32 0x577F8 			//float in ms			OUT


#define LENGHT 1024
//#define CLK_50 //if you want to use the accelerator @50Hz
#define ON 1
#define OFF 0
#define RESET 1
#define KEEP 0
#define LED_START 0
#define LED_RUNNING 1
#define LED_FINISH 2
#define LED_PLOTMODE 3

int filter; //we save the state every FILTER calls
int* array = NULL;
short* states = NULL;
int states_count;
int plot_mode;

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
			if (value == 1) //if I want to switch it on
				leds_dec += pow(2,pos); //add the value
			else
				leds_dec -= pow(2,pos); //else substract it
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
	if (n == 1)
		return;
	else if (n == 16) {
		alt_dcache_flush(num,n*sizeof(int));
		IOWR_32DIRECT(ACCELERATOR_16_0_BASE,0, num);  // RegAddStart
		IOWR_32DIRECT(ACCELERATOR_16_0_BASE, 2*4, 1); // 	Start

		// Active wait for finishing accelerator
		while((IORD_32DIRECT(ACCELERATOR_16_0_BASE, 3*4) & (1<<0)) == 0);// wait for the finish of the calculus
	}
#ifdef CLK_50
	else if (n == 32) {
		alt_dcache_flush(num,n*sizeof(int));
		IOWR_32DIRECT(ACCELERATOR_32_1_BASE,0, num);  // RegAddStart
		IOWR_32DIRECT(ACCELERATOR_32_1_BASE, 2*4, 1); // 	Start

		// Active wait for finishing accelerator
		while((IORD_32DIRECT(ACCELERATOR_32_1_BASE, 3*4) & (1<<0)) == 0);// wait for the finish of the calculus
	}
#else
	else if (n == 32) {
		alt_dcache_flush(num,n*sizeof(int));
		IOWR_32DIRECT(ACCELERATOR_32_0_BASE,0, num);  // RegAddStart
		IOWR_32DIRECT(ACCELERATOR_32_0_BASE, 2*4, 1); // 	Start

		// Active wait for finishing accelerator
		while((IORD_32DIRECT(ACCELERATOR_32_0_BASE, 3*4) & (1<<0)) == 0);// wait for the finish of the calculus
	}
#endif
	else
		printf("ERROR ACCELERATION: array of %d variables", n);
}

/**
 * Save the actual state of the array, one call every FILTER calls
 *
 **/
void save_state(){
	static int counter = 0;
	counter++; //number of calls
	if(counter%filter != 0) //save only one state every FILTER calls
		return;
	states_count++; //number of states recorded
	short* next_state = states+states_count*LENGHT;
	clone_s(array, next_state, LENGHT);
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
	if (n == depth) //if accelerable, accelerate it
		accelerate(num, n);
	else{
		//divide part
		int l1 = n/2; //lentgh of first array
		int l2 = n-l1; //lentgh of second array
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
	//printf("Sorted array ");
	//print_array_i(array,LENGHT);
	if (plot_mode){
		//now this partial array is sorted, save the state of the all array
		if (n > depth)
			save_state();
	}
}

int main(){
	set_led(LED_START,OFF,RESET); //clean leds
	//set_led(LED_START,ON,RESET); //set start
	//set_led(LED_PLOTMODE,ON,KEEP); //set plot_mode

	while(1){
		//waiting for the led 0 to start
		printf("Waiting to start....\n");
		while(!get_led(LED_START));
		printf("Start!\n");

		//clean possible remaining from before
		free(array); array = NULL;
		free(states); states = NULL;
		filter = 0;
		states_count = 0;

		plot_mode = get_led(LED_PLOTMODE); //read the plot mode
		set_led(LED_RUNNING,ON,RESET); //second led ON to say "running"

		int total_memory = 0;

		// malloc in bytes
		array = malloc(LENGHT*sizeof(int)); //LENGTH int vars (4 bytes x var)
		total_memory += LENGHT*sizeof(int);
		// check if malloc worked
		if (array == NULL) {
			printf("Allocation of buffer memory space failed\n");
			return -1;
		}

		if (plot_mode){
			printf("PLOT MODE\n");
			int n_states;
			while(states == NULL){ //increase filter until the states fit in memory
				filter++;
				n_states = LENGHT/filter;
				states = malloc(n_states*LENGHT*sizeof(short)); // n_states arrays of LENGHT short vars (2 bytes x var)
			}
			total_memory += n_states*LENGHT*sizeof(short);
			printf("Filter choosen: %d\n", filter);
			printf("Expected %d states\n", n_states);
			printf("Total memory allocated for STATES: %fKB\n", n_states*LENGHT/1000.0f*sizeof(short));
		}

		printf("Total memory allocated for ARRAY: %fKB\n", LENGHT/1000.0f*sizeof(int));
		printf("Total memory allocated %fKB\n", total_memory/1000.0f);

		printf("----------------------------------\n");

		//create array - random distribution from 0 to 30k
		int i;
		for (i=0; i<LENGHT; i++){
			*(array+i) = rand()%30000;
		}

		if (plot_mode){
			clone_s(array, states, LENGHT); //the initial array is the first state

			// NO ACCELERATION
			merge_sort(array, LENGHT, 1);

			printf("Table of states:\n");
			int i,j;
			for (i=0;i<= states_count;i++){
				printf("%d ",i+1);
				print_array_s(states+i*LENGHT,LENGHT);
			}

			//send the states to HPS
			IOWR_32DIRECT(ADDR_BASE_STATES,0,states); //states address
			IOWR_32DIRECT(ADDR_STATES_LENGTH,0,states_count+1); //number of states
																//+1 because one is the first state
																//ie the array created
			printf("states addr: %p",states);
		}
		else{
			unsigned long long count_no_acc = 0, count_16_acc = 0, count_32_acc = 0;
			float time_00, time_16, time_32;

			// WITHOUT ACCELERATION
			PERF_RESET(PERFORMANCE_COUNTER_0_BASE);
			PERF_START_MEASURING(PERFORMANCE_COUNTER_0_BASE);
			merge_sort(array, LENGHT, 1);
			PERF_STOP_MEASURING(PERFORMANCE_COUNTER_0_BASE);
			count_no_acc = perf_get_section_time(PERFORMANCE_COUNTER_0_BASE, 0);
			//print_array_i(array, LENGHT); //print final array sorted
			time_00 = count_no_acc /(NIOS2_CPU_FREQ/1000.0f); //time in ms
			printf("NO ACCELERATION: %llu cycles - %fms \n", count_no_acc, time_00);

			// 16 ACCELERATION
			if (LENGHT >= 16){
				PERF_RESET (PERFORMANCE_COUNTER_0_BASE);
				PERF_START_MEASURING (PERFORMANCE_COUNTER_0_BASE);
				merge_sort(array, LENGHT, 16);
				PERF_STOP_MEASURING (PERFORMANCE_COUNTER_0_BASE);
				count_16_acc = perf_get_section_time(PERFORMANCE_COUNTER_0_BASE, 0);
				time_16 = count_16_acc /(NIOS2_CPU_FREQ/1000.0f); //time in ms
				printf("16 ACCELERATION: %llu cycles - %fms \n", count_16_acc, time_16);
			}

			// 32 ACCELERATION
			if (LENGHT >= 32){
				PERF_RESET (PERFORMANCE_COUNTER_0_BASE);
				PERF_START_MEASURING (PERFORMANCE_COUNTER_0_BASE);
				merge_sort(array, LENGHT, 32);
				PERF_STOP_MEASURING (PERFORMANCE_COUNTER_0_BASE);
				count_32_acc = perf_get_section_time(PERFORMANCE_COUNTER_0_BASE, 0);
				time_32 = count_32_acc /(NIOS2_CPU_FREQ/1000.0f); //time in ms
				printf("32 ACCELERATION: %llu cycles - %fms \n", count_32_acc, time_32);
			}

			//send to HPS the times
			IOWR_32DIRECT(ADDR_TIME_00,0,time_00);
			IOWR_32DIRECT(ADDR_TIME_16,0,time_16);
			IOWR_32DIRECT(ADDR_TIME_32,0,time_32);

			unsigned long t0 = (unsigned long)(count_no_acc & 0xFFFFFFFF);
			unsigned long t1 = (unsigned long)(count_16_acc & 0xFFFFFFFF);
			unsigned long t2 = (unsigned long)(count_32_acc & 0xFFFFFFFF);
			float speed_0v16 = ((float)t0) / ((float)t1);
			float speed_16v32 = ((float)t1) / ((float)t2);
			float speed_0v32 = ((float)t0) / ((float)t2);

			//send to HPS the speed ups
			IOWR_32DIRECT(ADDR_SPEED_0v16,0,speed_0v16);
			IOWR_32DIRECT(ADDR_SPEED_16v32,0,speed_16v32);
			IOWR_32DIRECT(ADDR_SPEED_0v32,0,speed_0v32);

			printf("\n");
			printf("Speedup 1 vs 16:  %f \n",speed_0v16);
			printf("Speedup 16 vs 32: %f \n",speed_16v32);
			printf("Speedup 1 vs 32: %f \n",speed_0v32);
		}
		printf("----------------------------------\n\n\n\n");

		set_led(2,ON,RESET); //third led ON to say "finished"
	}
	return 0;
}
