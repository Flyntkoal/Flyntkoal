#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H

#define MAX_INPUT_FILES 10
#define MINARGS 3
#define MAXARGS 12
#define MAX_RESOLVER_THREADS 10
#define MIN_RESOLVER_THREADS 2
#define MAX_NAME_LENGTH 1025
#define MAX_IP_LENGTH INET6_ADDRSTRLEN
#define INPUTFS "%1024s"
#define USAGE "<inputFilePath> <outputFilePath>"

void* requester(void* file);

void* resolver(void* nothing);
#endif
