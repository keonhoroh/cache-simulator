#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<math.h>
#include<stdbool.h>

unsigned long*** buildCache(unsigned long setQuant, unsigned long assoc) {
    unsigned long*** cache = malloc(setQuant * sizeof(unsigned long**));
    for(int i = 0; i < setQuant; i++) {
        cache[i] = malloc(assoc * sizeof(unsigned long*));
        for(int j = 0; j < assoc; j++) {
            cache[i][j] = malloc(4 * sizeof(unsigned long));
            cache[i][j][0] = 0; // VALIDITY bit (0/1)
            cache[i][j][1] = 0; // TAG bit
            cache[i][j][2] = 0; // AGE of the block
            cache[i][j][3] = 0; // ADDRESS used
        }
    }
    return cache;
}

void addAge(unsigned long*** cache, unsigned long index, unsigned long assoc) {
    for(int i = 0; i < assoc; i++) {
        if(cache[index][i][0] == 1) {
            cache[index][i][2] += 1;
        }
    }
}

void freeCache(unsigned long*** cache, unsigned long setQuant, unsigned long assoc) {
    for(int i = 0; i < setQuant; i++) {
        for(int j = 0; j < assoc; j++) {
            free(cache[i][j]);
        }
        free(cache[i]);
    }
    free(cache);
}

long calculateIndex(long address, unsigned long setQuant, unsigned long offsetBits) {
    long index = ((address >> offsetBits) & (setQuant - 1));
    return index;
}

bool cacheLands(unsigned long*** cache, unsigned long tag, unsigned long index, unsigned long assoc, unsigned long offsetBits, unsigned long setBits) {
    bool hitCondition = false;
    for(int i = 0; i < assoc; i++) {
        if(cache[index][i][0] == 1 && (((cache[index][i][3]) >> (offsetBits + setBits)) == tag)) {
            hitCondition = true;
            break;
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

int main(int argc, char* argv[]) {
    
    FILE* trace = fopen(argv[8], "r");

    unsigned long L1cacheSize = atoi(argv[1]); // input format is ./first <cache size> <assoc:n> <cache policy> <block size> <trace file>
    unsigned long L1blockSize = atoi(argv[4]);
    unsigned long L1assoc = atoi(&(argv[2][6])); // finding the n in assoc:n (# of lines per set)
    unsigned long L1setQuant = L1cacheSize / (L1blockSize * L1assoc); // finding the number of sets in the cache
    unsigned long L1offsetBits = log2(L1blockSize); // calcuating offset bits block bits
    unsigned long L1setBits = log2(L1setQuant); // calculating set bits (index)

    unsigned long L2cacheSize = atoi(argv[5]);
    unsigned long L2blockSize = atoi(argv[4]);
    unsigned long L2assoc = atoi(&(argv[6][6]));
    unsigned long L2setQuant = L2cacheSize / (L2blockSize * L2assoc);
    unsigned long L2offsetBits = log2(L2blockSize);
    unsigned long L2setBits = log2(L2setQuant);

    unsigned long*** L1Cache = buildCache(L1setQuant, L1assoc);// initializing the cache using a 3D array
    unsigned long*** L2Cache = buildCache(L2setQuant, L2assoc);
    char request[50]; // reading from the trace file
    long address;

    unsigned long memoryRead = 0;
    unsigned long memoryWrite = 0;
    unsigned long L1cacheHit = 0;
    unsigned long L1cacheMiss = 0;
    unsigned long L2cacheHit = 0;
    unsigned long L2cacheMiss = 0;
    
    while(fscanf(trace, "%s %lx\n", request, &address) != EOF) {
        unsigned long L1tag = address >> (L1offsetBits + L1setBits); // calculating tag and index for block
        unsigned long L1index = (address >> L1offsetBits) & (L1setQuant - 1);
        unsigned long L2tag = address >> (L2offsetBits + L2setBits); // calculating tag and index for block
        unsigned long L2index = (address >> L2offsetBits) & (L2setQuant - 1);
        if(!strcmp(request, "W")) {
            memoryWrite += 1;
        }
        if(cacheLands(L1Cache, L1tag, L1index, L1assoc, L1offsetBits, L1setBits)) { // L1 HIT
            L1cacheHit += 1;
            if(!strcmp(argv[3], "lru")) {
                addAge(L1Cache, L1index, L1assoc);
                for(int j = 0; j < L1assoc; j++) {
                    if(L1Cache[L1index][j][0] == 1 && (L1Cache[L1index][j][3] >> (L1offsetBits + L1setBits)) == L1tag) {
                        L1Cache[L1index][j][2] = 0;
                        break;
                    }
                }
            } else { // HIT and FIFO
                addAge(L1Cache, L1index, L1assoc);
            }
        } else { // L1 MISS
            L1cacheMiss += 1;
            addAge(L1Cache, L1index, L1assoc);
            if(cacheLands(L2Cache, L2tag, L2index, L2assoc, L2offsetBits, L2setBits)) { // L1 MISS AND L2 HIT
                L2cacheHit += 1;
                addAge(L2Cache, L2index, L2assoc);
                
                unsigned long tempAddress;
                for(int i = 0; i < L2assoc; i++) {
                    if(L2Cache[L2index][i][0] == 1 && (L2Cache[L2index][i][3] >> (L2offsetBits + L2setBits)) == L2tag) {
                        tempAddress = L2Cache[L2index][i][3]; // now clear this block
                        L2Cache[L2index][i][0] = 0;
                        L2Cache[L2index][i][1] = 0;
                        L2Cache[L2index][i][2] = 0;
                        L2Cache[L2index][i][3] = 0;
                        break;
                    }
                }
                long tempIndex = calculateIndex(tempAddress, L1setQuant, L1offsetBits);
                long evictLine = getLine(L1Cache, tempIndex, L1assoc);
                if(evictLine != -3) { // L1 is not full
                    addAge(L1Cache, tempIndex, L1assoc); //just added
                    L1Cache[tempIndex][evictLine][0] = 1;
                    L1Cache[tempIndex][evictLine][1] = tempAddress >> (L1offsetBits + L1setBits);
                    L1Cache[tempIndex][evictLine][2] = 0;
                    L1Cache[tempIndex][evictLine][3] = tempAddress;
                } else { // L1 is full
                    addAge(L1Cache, tempIndex, L1assoc); //just added
                    unsigned long evictedAddress;
                    unsigned long max = 0;
                    for(int i = 0; i < L1assoc; i++) {
                        if(L1Cache[tempIndex][i][0] == 1 && L1Cache[tempIndex][i][2] > max) {
                            max = L1Cache[tempIndex][i][2];
                            evictLine = i;
                        }
                    }
                    if(evictLine == -3) {
                            evictLine = 0;
                    }
                    evictedAddress = L1Cache[tempIndex][evictLine][3];
                    L1Cache[tempIndex][evictLine][0] = 1;
                    L1Cache[tempIndex][evictLine][1] = tempAddress >> (L1offsetBits + L1setBits);
                    L1Cache[tempIndex][evictLine][2] = 0;
                    L1Cache[tempIndex][evictLine][3] = tempAddress;

                    tempIndex = calculateIndex(evictedAddress, L2setQuant, L2offsetBits);
                    evictLine = getLine(L2Cache, tempIndex, L2assoc);
                    if(evictLine != -3) { // L2 is not full
                        addAge(L2Cache, tempIndex, L2assoc);
                        L2Cache[tempIndex][evictLine][0] = 1;
                        L2Cache[tempIndex][evictLine][1] = evictedAddress >> (L2offsetBits + L2setBits);
                        L2Cache[tempIndex][evictLine][2] = 0;
                        L2Cache[tempIndex][evictLine][3] = evictedAddress;
                    } else {
                        addAge(L2Cache, tempIndex, L2assoc);
                        unsigned long newMax = 0;
                        for(int i = 0; i < L2assoc; i++) {
                            if(L2Cache[tempIndex][i][0] == 1 && L2Cache[tempIndex][i][2] > newMax) {
                                newMax = L2Cache[tempIndex][i][2];
                                evictLine = i;
                            }
                        }
                        if(evictLine == -3) {
                            evictLine = 0;
                        }
                        L2Cache[tempIndex][evictLine][0] = 1;
                        L2Cache[tempIndex][evictLine][1] = evictedAddress >> (L2offsetBits + L2setBits);
                        L2Cache[tempIndex][evictLine][2] = 0;
                        L2Cache[tempIndex][evictLine][3] = evictedAddress;
                    }
                }
            } else { // L1 & L2 MISS
                L2cacheMiss += 1;
                memoryRead += 1;
                addAge(L2Cache, L2index, L2assoc);

                long evictLine = getLine(L1Cache, L1index, L1assoc);
                if(evictLine != -3) { // L1 is not full
                    addAge(L1Cache, L1index, L1assoc);
                    L1Cache[L1index][evictLine][0] = 1;
                    L1Cache[L1index][evictLine][1] = L1tag;
                    L1Cache[L1index][evictLine][2] = 0;
                    L1Cache[L1index][evictLine][3] = address;
                } else { // L1 is full
                    addAge(L1Cache, L1index, L1assoc);
                    unsigned long evictedAddress;
                    unsigned long max = 0;
                    for(int i = 0; i < L1assoc; i++) {
                        if(L1Cache[L1index][i][0] == 1 && L1Cache[L1index][i][2] > max) {
                            max = L1Cache[L1index][i][2];
                            evictLine = i;
                        }
                    }
                    if(evictLine == -3) {
                            evictLine = 0;
                    }
                    evictedAddress = L1Cache[L1index][evictLine][3];
                    L1Cache[L1index][evictLine][0] = 1;
                    L1Cache[L1index][evictLine][1] = L1tag;
                    L1Cache[L1index][evictLine][2] = 0;
                    L1Cache[L1index][evictLine][3] = address;

                    long tempIndex = calculateIndex(evictedAddress, L2setQuant, L2offsetBits);
                    long newEvictLine = getLine(L2Cache, tempIndex, L2assoc);
                    if(newEvictLine != -3) { // L2 is not full
                        addAge(L2Cache, tempIndex, L2assoc);
                        L2Cache[tempIndex][newEvictLine][0] = 1;
                        L2Cache[tempIndex][newEvictLine][1] = evictedAddress >> (L2offsetBits + L2setBits);
                        L2Cache[tempIndex][newEvictLine][2] = 0;
                        L2Cache[tempIndex][newEvictLine][3] = evictedAddress;
                    } else { // L2 is full
                        addAge(L2Cache, tempIndex, L2assoc);
                        unsigned long newMax = 0;
                        for(int i = 0; i < L2assoc; i++) {
                            if(L2Cache[tempIndex][i][0] == 1 && L2Cache[tempIndex][i][2] > newMax) {
                                newMax = L2Cache[tempIndex][i][2];
                                newEvictLine = i;
                            }
                        }
                        if(newEvictLine == -3) {
                            newEvictLine = 0;
                        }
                        L2Cache[tempIndex][newEvictLine][0] = 1;
                        L2Cache[tempIndex][newEvictLine][1] = evictedAddress >> (L2offsetBits + L2setBits);
                        L2Cache[tempIndex][newEvictLine][2] = 0;
                        L2Cache[tempIndex][newEvictLine][3] = evictedAddress;
                    }
                }
            }
        }
    }

    fclose(trace);
    freeCache(L1Cache, L1setQuant, L1assoc);
    freeCache(L2Cache, L2setQuant, L2assoc);
    printf("memread:%lu\n", memoryRead);
    printf("memwrite:%lu\n", memoryWrite);
    printf("l1cachehit:%lu\n", L1cacheHit);
    printf("l1cachemiss:%lu\n", L1cacheMiss);
    printf("l2cachehit:%lu\n", L2cacheHit);
    printf("l2cachemiss:%lu\n", L2cacheMiss);
}