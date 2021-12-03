/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <cassert>
#include <iostream>
#include <string.h>
#include <unordered_map>
#include <deque>
#include <bits/stdc++.h>
#include <limits.h>
#include <stdio.h>
#include <time.h>

using namespace std;

using std::cout;
using std::endl;

// Prototype for test program
typedef void (*program_f)(char *data, int length);

// Number of physical frames
int nframes;

// Pointer to disk for access from handlers
struct disk *disk = nullptr;

// Chosen algorithm
const char *algorithm;

// Number needed to be print out
static int page_faults = 0;
static int disk_reads = 0;
static int disk_writes = 0;

// Store the relationship, frame number is the key and page as its value
typedef unordered_map<int, int> frame_page;
frame_page _frame_page;

// Array for counting the fault times for each frame
static int *DAFA_array = new int[101];
static int *new_DAFA_array = new int[101];
static int DAFA_page_faults;
static bool first_DAFA_range = true;

// construct deque for each algorithm
static deque<int> rand_deque;
static deque<int> fifo_deque;
static deque<int> DAFA_deque;

void removeFromQueue(deque<int> &queue, int del_frame){
        for (int i = 0; i < (int)queue.size(); ++i){
                if (*(queue.begin() + i) == del_frame){
                    queue.erase(queue.begin() + i);
                }
        }
}

void evict( struct page_table *pt, int evict_frame ){

	int evict_page = _frame_page[evict_frame];

	char* physmem = page_table_get_physmem( pt );
	int cur_bit = pt->page_bits[evict_page];

	if ( (cur_bit & PROT_WRITE) == PROT_WRITE ) {
		++disk_writes;
		disk_write(disk, evict_page, &physmem[evict_frame*PAGE_SIZE]);
	}
	
	page_table_set_entry( pt, evict_page, 0, 0 );

}

void rand( struct page_table *pt, int page, int cur_bits ){
	int pick_frame;

	// the fifo_deque is not full -> physical memory is not full
	if ((int)rand_deque.size() < page_table_get_nframes(pt)){

		// pick a frame, since the frame is empty right now
		pick_frame = rand_deque.size();

	} 
	else{

		srand(time(0));
		int del_index = (rand() % (rand_deque.size() - 0)) + 0;
		// we choose the random frame of the rand_deque as the evict frame
		int pick_frame = rand_deque[del_index];

		removeFromQueue(rand_deque, pick_frame);

		int evict_page = _frame_page[pick_frame]; 
		evict( pt, pick_frame );
		
	}

	// add the re-used frame or new frame to the end of the queue
	rand_deque.push_back(pick_frame);
	// update the bits and frame for current page
	cur_bits = PROT_READ;

	// update the relationship in the queue
	_frame_page[pick_frame] = page;
	
	// update the page table
	page_table_set_entry( pt, page, pick_frame, cur_bits );

	// read the data from disk to physical memeory and disk_reads increment 1
	char* physmem = page_table_get_physmem( pt );
	disk_read( disk, page, &physmem[pick_frame*PAGE_SIZE] );
	++disk_reads;

}

void fifo( struct page_table *pt, int page, int cur_bits ){
	int pick_frame;
	
	// the fifo_deque is not full -> physical memory is not full
	if ((int)fifo_deque.size() < page_table_get_nframes(pt)){

		// pick a frame, since the frame is empty right now
		pick_frame = fifo_deque.size();

	} 
	else{
		
		// we choose the first frame of the fifo_deque as the evict frame
		int pick_frame = fifo_deque.front();
		fifo_deque.pop_front();

		int evict_page = _frame_page[pick_frame]; //debug
		evict( pt, pick_frame );
		
	}

	// add the re-used frame or new frame to the end of the queue
	fifo_deque.push_back(pick_frame);

	// update the bits and frame for current page
	cur_bits = PROT_READ;

	// update the relationship in the queue
	_frame_page[pick_frame] = page;
	
	// update the page table
	page_table_set_entry( pt, page, pick_frame, cur_bits );

	// read the data from disk to physical memeory and disk_reads increment 1
	char* physmem = page_table_get_physmem( pt );
	disk_read( disk, page, &physmem[pick_frame*PAGE_SIZE] );
	++disk_reads;
}

void custom( struct page_table *pt, int page, int cur_bits ){
	int pick_frame;
	
	// Ratio for Dynamic Adjustable Frame Allocation (DAFA). If the ratio is large, we adjust the 
	// strategy more frequently.
	int ratio = page_table_get_npages( pt ) / page_table_get_nframes( pt );

	// Base number of times we adjust the replacing strategy
	int base = 100;
	
	int adjust_range;
	if (base/ratio < 5){
		adjust_range = 5;
	}
	else{
		adjust_range = base / ratio;
	}


	// the fifo_deque is not full -> physical memory is not full
	if ((int)DAFA_deque.size() < page_table_get_nframes(pt)){

		// pick a frame, since the frame is empty right now
		pick_frame = DAFA_deque.size();

	} 
	else if ( first_DAFA_range ){

		if (DAFA_page_faults >= adjust_range){
			DAFA_page_faults = 0;
			first_DAFA_range = false;
		}

		// In the first adjust_range, we choose the first frame of the fifo_deque as the evict frame
		int pick_frame = DAFA_deque.front();
		DAFA_deque.pop_front();

		// Count the evict time for first range
		DAFA_array[pick_frame]++;

		int evict_page = _frame_page[pick_frame]; //debug
		evict( pt, pick_frame );
		
	}
	else{
		// In the new range, update the parameters
		if (DAFA_page_faults >= adjust_range){
			DAFA_page_faults = 0;
			DAFA_array = new_DAFA_array;
			for(int k = 0;k < page_table_get_nframes( pt );k++){
				new_DAFA_array[k] = 0;
			}
			// memset(new_DAFA_array, 0, sizeof(int)*page_table_get_npages( pt ));
		}

		int min_fault = INT_MAX;
		int del_index;

		// Find the frame with minimum fault times
		for (int i = 0; i < page_table_get_nframes( pt ); i++){
			if (DAFA_array[i] < min_fault){
				min_fault = DAFA_array[i];
				del_index = i;
			}
		}

		// +1 after the choosing. +1 also in the new_DAFA_array -> counting the faults
		// use in next adjust range
		DAFA_array[del_index]++;
		new_DAFA_array[del_index]++;

		int pick_frame = DAFA_deque[del_index];
		removeFromQueue(DAFA_deque, pick_frame);

		int evict_page = _frame_page[pick_frame]; // debug
		evict( pt, pick_frame );

	}

	// add the re-used frame or new frame to the end of the queue
	DAFA_deque.push_back(pick_frame);

	// update the bits and frame for current page
	cur_bits = PROT_READ;

	// update the relationship in the map
	_frame_page[pick_frame] = page;
	
	// update the page table
	page_table_set_entry( pt, page, pick_frame, cur_bits );

	// read the data from disk to physical memeory and disk_reads increment 1
	char* physmem = page_table_get_physmem( pt );
	disk_read( disk, page, &physmem[pick_frame*PAGE_SIZE] );
	++disk_reads;

}

// Simple handler for pages == frames
void page_fault_handler(struct page_table *pt, int page) {
	cout << "page fault on page #" << page << endl;

	// Print the page table contents
	cout << "Before ---------------------------" << endl;
	page_table_print(pt);
	cout << "----------------------------------" << endl;

	// add page faults
		++page_faults; 

	// get information
	int cur_frame;
	int cur_bits;
	page_table_get_entry( pt, page, &cur_frame, &cur_bits );

	// only read
	if ( (cur_bits & PROT_READ) == PROT_READ && (cur_bits & PROT_WRITE) == 0){
		
			cur_bits = PROT_READ | PROT_WRITE;
			page_table_set_entry( pt, page, cur_frame, cur_bits );

	}
	else{
		

		// counting page faults for custom algorithm
		++DAFA_page_faults;

		// call the corresponding function
		void (*chosen_algorithm[])( struct page_table *pt, int page, int cur_bits ) = {rand, fifo, custom};

		if (strcmp(algorithm, "rand") == 0){
			(*chosen_algorithm[0])( pt, page, cur_bits );
		}
		else if (strcmp(algorithm, "fifo") == 0){
			(*chosen_algorithm[1])( pt, page, cur_bits );
		}
		else if (strcmp(algorithm, "custom") == 0){
			(*chosen_algorithm[2])( pt, page, cur_bits );
		}
	}

	// Print the page table contents
	cout << "After ----------------------------" << endl;
	page_table_print(pt);
	cout << "----------------------------------" << endl;
}

int main(int argc, char *argv[]) {
	// Check argument count
	if (argc != 5) {
		cerr << "Usage: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>" << endl;
		exit(1);
	}

	// Parse command line arguments
	int npages = atoi(argv[1]);
	nframes = atoi(argv[2]);
	algorithm = argv[3];
	const char *program_name = argv[4];

	// Validate the algorithm specified
	if ((strcmp(algorithm, "rand") != 0) &&
	    (strcmp(algorithm, "fifo") != 0) &&
	    (strcmp(algorithm, "custom") != 0)) {
		cerr << "ERROR: Unknown algorithm: " << algorithm << endl;
		exit(1);
	}

	// Validate the program specified
	program_f program = NULL;
	if (!strcmp(program_name, "sort")) {
		if (nframes < 2) {
			cerr << "ERROR: nFrames >= 2 for sort program" << endl;
			exit(1);
		}
		program = sort_program;
	}
	else if (!strcmp(program_name, "scan")) {
		program = scan_program;
	}
	else if (!strcmp(program_name, "focus")) {
		program = focus_program;
	}
	else {
		cerr << "ERROR: Unknown program: " << program_name << endl;
		exit(1);
	}

	// Create a virtual disk
	disk = disk_open("myvirtualdisk", npages);
	if (!disk) {
		cerr << "ERROR: Couldn't create virtual disk: " << strerror(errno) << endl;
		return 1;
	}

	// Create a page table
	struct page_table *pt = page_table_create(npages, nframes, page_fault_handler /* TODO - Replace with your handler(s)*/);
	if (!pt) {
		cerr << "ERROR: Couldn't create page table: " << strerror(errno) << endl;
		return 1;
	}

	// Run the specified program
	char *virtmem = page_table_get_virtmem(pt);
	program(virtmem, npages * PAGE_SIZE);

	// Clean up the page table and disk
	page_table_delete(pt);
	disk_close(disk);

	cout << "page faults number: " << page_faults << endl;
	cout << "disk reads number: " << disk_reads << endl;
	cout << "disk_writes number: " << disk_writes << endl;

	// free(DAFA_array);
	// free(new_DAFA_array);
	return 0;
}
