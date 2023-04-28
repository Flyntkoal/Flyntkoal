#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "multi-lookup.h"
#include "util.h"
#include "queue.h"

pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER; //prevents race conditions in the queue
pthread_mutex_t doc_lock = PTHREAD_MUTEX_INITIALIZER; //prevents race conditions in results.txt
pthread_mutex_t iter_lock = PTHREAD_MUTEX_INITIALIZER; //prevents race conditions with req_threads_complete

queue q; //FIFO queue for domain names
FILE *results; //file that will contain the domain name and resulting ip addresses
int total_req_threads; //variable to keep track of how many requester threads in total are going to be used by the program
int req_threads_complete = 0; //variable to keep track of how many requester threads have completed

//requester is designed to take a file as input and place each domain onto the queue one at a time
void* requester(void* file){
	FILE *fpt;
	char* cur_file = file;
	char file_line[MAX_NAME_LENGTH];

	fpt = fopen(cur_file, "r");
	if(!fpt){
		perror("Error Opening Output File");
		exit(EXIT_FAILURE);
	}
	while(fscanf(fpt, INPUTFS, file_line) > 0){
		int result_fail = -1;
		char* domain = strdup(file_line);
		while(result_fail < 0){
			pthread_mutex_lock(&queue_lock);
			result_fail = queue_push(&q, domain);
			pthread_mutex_unlock(&queue_lock);
			
			if(result_fail < 0)
				usleep(rand()%100);
		}
	}

	fclose(fpt);

	pthread_mutex_lock(&iter_lock);
	req_threads_complete++;//one more requester thread is complete
	pthread_mutex_unlock(&iter_lock);
	return NULL;
}

//resolver is designed to continually pop a web address from the queue, look up the IP, and place them both in results.txt
//if domain doesn't exist, an error is printed, and the domain is placed in results.txt without an IP
//thread terminates once the queue is empty and all requester threads have completed
void* resolver(void* nothing){
	(void)nothing;
	while(1){
		pthread_mutex_lock(&queue_lock);
                int empty_queue = queue_is_empty(&q);
                pthread_mutex_unlock(&queue_lock);
		
		if(!empty_queue){
                        //if the queue still has addresses in it, continue as normal
                        pthread_mutex_lock(&queue_lock);
                        char* s = queue_pop(&q);
                        pthread_mutex_unlock(&queue_lock);

        		char hostname[MAX_NAME_LENGTH];
        		char domain[INET6_ADDRSTRLEN];

        		strncpy(hostname, s, sizeof(hostname));

        		if(dnslookup(hostname, domain, sizeof(domain))
               		== UTIL_FAILURE){
                		fprintf(stderr, "dnslookup error: %s\n", hostname);
                		strncpy(domain, "", sizeof(domain));
            		}

        		pthread_mutex_lock(&doc_lock);
        		fprintf(results, "%s,%s\n", hostname, domain);
        		pthread_mutex_unlock(&doc_lock);
                }
		else{
                        pthread_mutex_lock(&iter_lock);
                        int program_done = (req_threads_complete >= total_req_threads);
                        pthread_mutex_unlock(&iter_lock);

                        //program is done if the queue is empty and req_threads_complete is equal to the amount of files passed into the arguments
                        if(program_done){
                                return EXIT_SUCCESS;
                        }
                }


	}
}

int main(int argc, char *argv[]){
	pthread_t req_threads[MAX_INPUT_FILES]; //requester thread pool
	pthread_t res_threads[MAX_RESOLVER_THREADS]; //resolver thread pool
	int rc;

	queue_init(&q, QUEUEMAXSIZE);

	//check to see if at least 3 arguments are passed (the execution command, a file of domain names, and a file to print the output to)
	if(argc < MINARGS){
		fprintf(stderr, "Not enough arguments: %d\n", (argc - 2));
		fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
		return EXIT_FAILURE;
    	}

	//check to see if 10 files or fewer were passed
	if(argc > MAXARGS){
		fprintf(stderr, "Too many arguments: %d\n", (argc - 2));
                fprintf(stderr, "Usage:\n %s %s\n", argv[0], USAGE);
                return EXIT_FAILURE;
	}

	total_req_threads = argc-2;

	//open results file
        results = fopen(argv[argc-1], "w");
        if(!results){
                perror("Error Opening Output File");
                return EXIT_FAILURE;
        }

	//create requester threads for each file passed
	for(int i = 1; i < argc - 1; i++){
		rc = pthread_create(&(req_threads[i-1]), NULL, requester, argv[i]);
		if (rc){
	    		printf("ERROR; return code from pthread_create() is %d\n", rc);
	    		return EXIT_FAILURE;
		}
	}
	
	//create 10 resolver threads
	for(int i = 0; i < MAX_RESOLVER_THREADS; i++){
		rc = pthread_create(&(res_threads[i]), NULL, resolver, NULL);
		if (rc){
                        printf("ERROR; return code from pthread_create() is %d\n", rc);
                        return EXIT_FAILURE;
                }
	}

	//wait for all 10 resolver threads to complete
	for(int i = 0; i < MAX_RESOLVER_THREADS; i++){
		pthread_join(res_threads[i], NULL);
	}

	//cleanup queue and close the results file
	queue_cleanup(&q);
        fclose(results);
}
