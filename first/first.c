#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include<stdbool.h>

void freeCache(unsigned long*** cache, unsigned long setQuant, unsigned long assoc) {
    for(int i = 0; i < setQuant; i++) {
        for(int j = 0; j < assoc; j++) {
            free(cache[i][j]);
        }
        free(cache[i]);
    }
    free(cache);
}

unsigned long*** buildCache(unsigned long setQuant, unsigned long assoc) {
    unsigned long*** cache = malloc(setQuant * sizeof(unsigned long**));
    for(int i = 0; i < setQuant; i++) {
        cache[i] = malloc(assoc * sizeof(unsigned long*));
        for(int j = 0; j < assoc; j++) {
            cache[i][j] = malloc(3 * sizeof(unsigned long));
            cache[i][j][0] = 0; // VALIDITY bit (0/1)
            cache[i][j][1] = 0; // TAG bit
            cache[i][j][2] = 0; // AGE of the block
        }
    }
    return cache;
}

bool cacheLands(unsigned long*** cache, unsigned long tag, unsigned long index, unsigned long assoc) {
    bool hitCondition = false;
    for(int i = 0; i < assoc; i++) {
        if(cache[index][i][0] == 1 && cache[index][i][1] == tag) {
            hitCondition = true;
        }
    }
    return hitCondition;
}

long getLine(unsigned long*** cache, unsigned long index, unsigned long assoc) {
    for(int i = 0; i < assoc; i++) {
        if(cache[index][i][0] == 0) {
            return i;
        }
    }
    return -3;
}

void addAge(unsigned long*** cache, unsigned long index, unsigned long assoc) {
    for(int i = 0; i < assoc; i++) {
        if(cache[index][i][0] == 1) {
            cache[index][i][2] += 1;
        }
    }
}

int main(int argc, char* argv[]) {

    FILE* trace = fopen(argv[5], "r");

    unsigned long cacheSize = atoi(argv[1]); // input format is ./first <cache size> <assoc:n> <cache policy> <block size> <trace file>
    unsigned long blockSize = atoi(argv[4]);
    unsigned long assoc = atoi(&(argv[2][6])); // finding the n in assoc:n (# of lines per set)
    unsigned long setQuant = cacheSize / (blockSize * assoc); // finding the number of sets in the cache
    unsigned long offsetBits = log2(blockSize); // calculating offset bits block bits
    unsigned long setBits = log2(setQuant); // calculating set bits (index)
    unsigned long*** cache = buildCache(setQuant, assoc);// initializing the cache using a 3D array
    char request[50]; // reading from the trace file
    long address;

    unsigned long memoryRead = 0;
    unsigned long memoryWrite = 0;
    unsigned long cacheHit = 0;
    unsigned long cacheMiss = 0;

    while(fscanf(trace, "%s %lx\n", request, &address) != EOF) {
        unsigned long tag = address >> (offsetBits + setBits); // calculating tag and index for block
        unsigned long index = (address >> offsetBits) & (setQuant - 1);
        if(!strcmp(request, "W")) {
            memoryWrite += 1;
        }
        if(cacheLands(cache, tag, index, assoc)) {
            cacheHit += 1;
            if(!strcmp(argv[3], "lru")){ // CACHE HIT AND LRU
                addAge(cache, index, assoc);
                for(int j = 0; j < assoc; j++) {
                    if(cache[index][j][0] == 1 && cache[index][j][1] == tag) {
                        cache[index][j][2] = 0; // must set the recently visited block to age of 0
                        break;
                    }
                }
            } else { // CACHE HIT AND FIFO
                addAge(cache, index, assoc);
            }
        } else { //CACHE MISS: does the set have space or not???
            memoryRead += 1;
            cacheMiss += 1;
            addAge(cache, index, assoc);
            long emptyLine = getLine(cache, index, assoc);
            if(emptyLine == -3) { // no space
                unsigned long max = 0;
                for(int i = 0; i < assoc; i++) {
                    if(cache[index][i][0] == 1 && cache[index][i][2] > max) {
                        max = cache[index][i][2];
                        emptyLine = i;
                    }
                }
                cache[index][emptyLine][0] = 1;
                cache[index][emptyLine][1] = tag;
                cache[index][emptyLine][2] = 0; // set age to 0 or 1?
            } else {
                cache[index][emptyLine][0] = 1;
                cache[index][emptyLine][1] = tag;
                cache[index][emptyLine][2] = 0;
            }
        }
    }
    fclose(trace);
    freeCache(cache, setQuant, assoc);
    printf("memread:%lu\n", memoryRead);
    printf("memwrite:%lu\n", memoryWrite);
    printf("cachehit:%lu\n", cacheHit);
    printf("cachemiss:%lu\n", cacheMiss);
}