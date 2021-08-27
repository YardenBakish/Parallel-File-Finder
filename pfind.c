/*
 * pfind.c
 *
 *  Created on: May 21, 2021
 *      Author: DELL
 */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>



/*DATA STRUCTURES*/
/*queue will be handed with linked list with global head_node and tail_node
 * (as seen in the GLOBAL VARIABLES SECTION)
 used for adding to tail, and dequeueing from head.*/

typedef struct pathfile_node
{
	char directory_path[PATH_MAX];
	struct pathfile_node* next;
} directory;


/*GLOBAL VARIABLES*/

/*CONDITION VARIABLES*/
pthread_cond_t count_threshold_cv; //used for initial setup of threads
pthread_cond_t LAUNCH; //used to alert threads that queue is not empty again
pthread_cond_t RESUME; //used to let a thread that inserted directory to an empy queue that another thread dequeued it

/*MUTEX*/
pthread_mutex_t  count_mutex;  //general mutex used for counting
pthread_mutex_t  local; //our main mutex used during the main flow

pthread_t* threads; // an array cosisting our threads
directory* head_node; // at any time will point to head of queue
directory* tail_node; // at any time will point to end of queue
char* T_toFind; //the search_term


/*COUNTERS AND FLAG VARIABLES*/
int active_counter = 0; // at any time will be updated to alert how many threads are functional
int passive_counter = 0; //at any time will alert how many threads are suspended
int number_of_threads = 0; //will be set upon recieving input from user
int number_of_files = 0; //number of files found that matches search term. printed in the end
int thread_failed = 0; // a flag variable to suggest if a thread failed
int terminate = 0; //a flag variable which tells all threads to terminate





/*concatanate folder and file with '/' in between (creating filepath)*/
void concatanate_with_slesh(char* old, char* file_inside_folder, char* current) {
	int i = 0, j = 0;
	int len_str1 = strlen(old);
	int len_str2 = strlen(file_inside_folder);
	for (i = 0; i < len_str1; i++) {
		current[i] = old[i];
	}
	current[i] = '/';
	for (j = 0; j < len_str2; j++) {
		current[j + i + 1] = file_inside_folder[j];
	}
	current[j + i + 1] = '\0';

}
/*upon receiving the name for the file - check if it matches search term*/
void is_search_term(char* str, char* file_name) {
	if (strstr(file_name, T_toFind) != NULL) { //if matches search term
		pthread_mutex_lock(&count_mutex); //lock and update number of files matched
		number_of_files = number_of_files + 1;
		pthread_mutex_unlock(&count_mutex);
		printf("%s\n", str); //print the file path
	}
	return;
}

/*The flow - STEP 1+2 (create FIFO and insert search root directory*/
void initialize_data(char* search_root_directory) {
	struct stat buf;
	head_node = (directory*)malloc(sizeof(directory));
	if (head_node == NULL) {
		fprintf(stderr, "Failed allocating memory: \n");
		exit(1);
	}
	head_node->next = NULL;
	tail_node = head_node;
	/*allocate threads array depandant on arguments from user*/
	threads = calloc(sizeof(pthread_t), number_of_threads);
	if (threads == NULL) {
		fprintf(stderr, "Failed creating threads array: \n");
		exit(1);
	}
	/*is root directory searchable*/
	if ((lstat(search_root_directory, &buf)) == -1) {
		fprintf(stderr, "problem with root directory\n");
		exit(1);
	}
	if (((buf.st_mode & S_IRUSR) == 0) || ((buf.st_mode & S_IXUSR) == 0)) {
		fprintf(stderr, "root directory not searchable\n");
		exit(1);
	}

	strcpy(head_node->directory_path, search_root_directory);
	/*initialize locks*/
	pthread_mutex_init(&local, NULL);
	pthread_mutex_init(&count_mutex, NULL);
	pthread_cond_init(&LAUNCH, NULL);
	pthread_cond_init(&RESUME, NULL);
	pthread_cond_init(&count_threshold_cv, NULL);
}

/*enqueue to queue*/
int enqueue(char* path, long id) {
	pthread_mutex_lock(&local);
	directory* tmp; /*allocate new node to append*/

	tmp = (directory*)malloc(sizeof(directory));
	if (tmp == NULL) return 1; /*if allocation failed return 1*/
	strcpy(tmp->directory_path, path); /*copy the path of directory*/
	/*(1) QUEUE IS EMPTY AND WE ARE ENQUEUEING*/
	if (head_node == NULL) {
		head_node = tmp;
		tail_node = head_node;
		tail_node->next = NULL;
	/*if there are other suspended threads to dequeue*/
	if (passive_counter > 0){
		pthread_cond_signal(&LAUNCH); /*alert suspended threads that queue is not empty*/
		pthread_cond_wait(&RESUME, &local); /*wait until some thread will dequeue and signal*/
		}
	}
	/*(2) QUEUE IS NOT EMPTY*/
	else { /*simply insert*/
		tail_node->next = tmp;
		tail_node = tail_node->next;
		tail_node->next = NULL;
		/*if (active_counter < number_of_threads) {
			pthread_cond_broadcast(&LAUNCH);

		}*/
	}
	pthread_mutex_unlock(&local);
	return 0;

}


int iterate_over_directory(DIR *directory, struct dirent *iterable, char* old, struct stat buf, int id){
	char* file_inside_folder;
	char current[PATH_MAX];
	while ((iterable = readdir(directory)) != NULL) {
			/*extract name of file*/
			file_inside_folder = iterable->d_name;
			/*concatanate in the way: "old filepath" + "/" + "name of file"*/
			concatanate_with_slesh(old, file_inside_folder, current);
			/*check file's type (file, directory, symbolic link etc... and move result to buf*/
			if ((lstat(current, &buf)) != 0) {
				return 1;
			}
			/*STEP 3 - FLOW OF THREAD - (a)*/
			if (S_ISDIR(buf.st_mode)) {
				if ((strcmp(file_inside_folder, ".") == 0) || (strcmp(file_inside_folder, "..") == 0)) {
					continue;
				}
				/*STEP 3 - FLOW OF THREAD - (c) - are we dealing with searchable directory*/
				if (((buf.st_mode & S_IRUSR) == 0) || ((buf.st_mode & S_IXUSR) == 0)) {
					printf("Directory %s: Permission denied.\n", current);
					continue;
				}
				/*enqueue the directory. if recieved 1 - error occured with memory allocation*/
				if (enqueue(current, id) == 1) {
					return 1;
				}

			}
			/*STEP 3 - FLOW OF THREAD - (d) - are we dealing with file*/
			/*check if file matches search term*/
			else { is_search_term(current, file_inside_folder); }
		}
		closedir(directory);
		return 0;
}

int dequeue(long id) {

	directory* tmp;
	char old[PATH_MAX];
	/*flow of thread - STEP 2 - dequeue the head directory*/
	tmp = head_node;
	if (head_node->next != NULL) head_node = head_node->next;
	else {
		head_node = NULL;
		tail_node = NULL;
	}
	/*remember pathfile*/
	strcpy(old, tmp->directory_path);
	free(tmp);
	/*for all threads who enqueued to empty queue - continue*/
	pthread_cond_broadcast(&RESUME);
	pthread_mutex_unlock(&local);

	struct stat buf;
	DIR *directory;
	struct dirent *iterable = NULL;
	directory = opendir(old);
	if (directory == NULL) {
		return 1;
	}
	/*STEP 3 - FLOW OF THREAD - Iterate through each file in the directory obtained from the queue: */
	return iterate_over_directory(directory, iterable, old,  buf, id);

}


/*HANDLER - FLOW OF SEARCHING THREAD*/
void handler(void* t) {
	long id = (long)t;
	int res = 0;

/*IFF all threads were initialized - handler kicks in*/
	while (1) {
		pthread_mutex_lock(&local);
		/*(1) if queue is empty*/
		if (head_node == NULL) {
			/*If I'm the only non-suspended thread*/
			if (passive_counter == active_counter-1) {
				/*raise terminate - indicating all threads must exit*/
				terminate = 1;
				/*signal all suspended signals to work*/
				pthread_cond_broadcast(&LAUNCH);
				pthread_mutex_unlock(&local);
				/*exit*/
				pthread_exit(NULL);
			}
			else {
				/*if queue is empty - wait until signaled*/
				passive_counter = passive_counter + 1; //update number of suspended threads
				pthread_cond_wait(&LAUNCH, &local);
				/*if terminate flag was raised - exit*/
				if (terminate == 1) {
					pthread_mutex_unlock(&local);
					pthread_exit(NULL);
				}
				/*queue is not empty and we are done waiting*/
				passive_counter = passive_counter - 1;
				pthread_mutex_unlock(&local);
			}
		}
		else {
			/*(2) if queue is not empty*/
			res = dequeue(id);
			/*if res==1 error occurred - terminate thread*/
			if (res == 1) {
				fprintf(stderr, "problem with handling queue\n");
				pthread_mutex_lock(&local);
				thread_failed = 1;
				active_counter = active_counter - 1;
				/*if there are no other active threads - simply exit*/
				if (active_counter == 0) {
					terminate = 1;
					pthread_cond_signal(&LAUNCH);
					pthread_mutex_unlock(&local);
					pthread_exit(NULL);
				}
				pthread_mutex_unlock(&local);
				pthread_exit(NULL);
			}
		}
		/*STEP 4 - FLOW OF THREAD - REPEAT LOOP*/
	}
}



/*The flow - STEPS 3-4*/
void* initialization_handler(void* t) {
	/*lock for update*/
	pthread_mutex_lock(&count_mutex);
	/*update number of active threads*/
	active_counter = active_counter + 1;
	/*if all n threads are created - signal to main thread that we're ready*/
	if (active_counter == number_of_threads) pthread_cond_signal(&LAUNCH);
	/*Suspened for other threads to be created or main threard telling us to go*/
	pthread_cond_wait(&count_threshold_cv, &count_mutex);
	/*unlock for other threads to exit their suspended mode*/
	pthread_mutex_unlock(&count_mutex);
	/*go to main Handler*/
	handler(t);
	return NULL;
}



void launch_threads() {
/*STEP 3 - CREATE n THREADS*/
	pthread_mutex_lock(&count_mutex);
	for (long i = 0; i < number_of_threads; i++) {
		int rc = pthread_create(&threads[i], NULL, initialization_handler, (void *) i);
		if (rc) {
			fprintf(stderr, "Failed creating thread %s\n", strerror(rc));
			exit(1);
		}
	}
	pthread_cond_wait(&LAUNCH, &count_mutex);
	/*STEP 4 - After all searching threads are created, signal them to start searching*/
	pthread_cond_broadcast(&count_threshold_cv);
	pthread_mutex_unlock(&count_mutex);
}

/*MAIN THREAD*/
int main(int argc, char* argv[])
{
	/*check number of arguments provided*/
	if (argc != 4) {
		fprintf(stderr, "number of arguments!=4");
		exit(1);
	}
	/*initialize global variables from user*/
	char* search_root_directory = argv[1];
	T_toFind = argv[2];
	number_of_threads = atoi(argv[3]);

	/*INITIALIZING DATA STRUCTURES*/
	initialize_data(search_root_directory);
	/*LAUNCHING THREADS*/
	launch_threads();
	// Wait for all threads to complete
	for (int i = 0; i < number_of_threads; i++) {
		pthread_join(threads[i], NULL);
	}

	printf("Done searching, found %d files\n", number_of_files); //print number of files matched
	if (thread_failed == 1) exit(1); //if one or more threads failed we exit(1)
	exit(0);
}


