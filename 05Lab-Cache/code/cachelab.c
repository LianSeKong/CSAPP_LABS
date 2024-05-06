/*
 * cachelab.c - Cache Lab helper functions
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "cachelab.h"
#include <time.h>

trans_func_t func_list[MAX_TRANS_FUNCS];
int func_counter = 0; 

/* 
 * printSummary - Summarize the cache simulation statistics. Student cache simulators
 *                must call this function in order to be properly autograded. 
 */
void printSummary(int hits, int misses, int evictions)
{
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
    FILE* output_fp = fopen(".csim_results", "w");
    assert(output_fp);
    fprintf(output_fp, "%d %d %d\n", hits, misses, evictions);
    fclose(output_fp);
}

/* 
 * initMatrix - Initialize the given matrix 
 */
void initMatrix(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;
    srand(time(NULL));
    for (i = 0; i < N; i++){
        for (j = 0; j < M; j++){
            // A[i][j] = i+j;  /* The matrix created this way is symmetric */
            A[i][j]=rand();
            B[j][i]=rand();
        }
    }
}

void randMatrix(int M, int N, int A[N][M]) {
    int i, j;
    srand(time(NULL));
    for (i = 0; i < N; i++){
        for (j = 0; j < M; j++){
            // A[i][j] = i+j;  /* The matrix created this way is symmetric */
            A[i][j]=rand();
        }
    }
}

/* 
 * correctTrans - baseline transpose function used to evaluate correctness 
 */
void correctTrans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;
    for (i = 0; i < N; i++){
        for (j = 0; j < M; j++){
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    
}



/* 
 * registerTransFunction - Add the given trans function into your list
 *     of functions to be tested
 */
void registerTransFunction(void (*trans)(int M, int N, int[N][M], int[M][N]), 
                           char* desc)
{
    func_list[func_counter].func_ptr = trans;
    func_list[func_counter].description = desc;
    func_list[func_counter].correct = 0;
    func_list[func_counter].num_hits = 0;
    func_list[func_counter].num_misses = 0;
    func_list[func_counter].num_evictions =0;
    func_counter++;
}


/**
 * init cache
 * 
*/

Cache_T initCache(unsigned int s, unsigned int E, unsigned int b) {
    Cache_T cache;
    cache.b = b;
    cache.s = s;
    cache.E = E;
    cache.SetArray = (Set_T *) malloc(sizeof(Set_T) * getS(s)); // 多少个SET

    // build LineArray
    for (size_t i = 0; i < getS(s); i++)
    {
        Line_T* lineArray = (Line_T*)malloc(sizeof(Line_T) * E);
        for (size_t j = 0; j < E; j++)
        {
            lineArray[j].valid = 0;
            lineArray[j].CacheBlock = malloc(getB(b)); // 每行中有多少个bytes
            lineArray[j].Tag = 0;
        }
        cache.SetArray[i].LineArray = lineArray;
    }
    return cache;
}


// Number of set index bits (S = 2^s)

unsigned int getS(unsigned int s)
{
  return (1 << s);
}

// Number of block bit (B = 2^b)

unsigned int getB(unsigned int b)
{
  return (1 << b);
}

unsigned long getSetIndex(unsigned int s, unsigned int b, unsigned long address)
{
    return ((address >> b) & (getS(s) - 1));
}

Set_T getSet(Cache_T cache, unsigned long address) {
    unsigned int index = getSetIndex(cache.s, cache.b, address);
    return cache.SetArray[index];
}

Line_T* getUnuseLine(Set_T set, unsigned int E) {
    for (size_t i = 0; i < E; i++) {
        Line_T* linePtr = set.LineArray + i;
        if (linePtr->valid == 0) {
            return linePtr;
        }
    }
    return NULL;
}


Line_T* getLRULine(Set_T set, unsigned int E) {
    Line_T* LRULinePtr = set.LineArray;
    for (size_t i = 1; i < E; i++)
    {
        Line_T* currentLinePtr = set.LineArray + i;
        if (currentLinePtr->time > LRULinePtr->time)
        {
            LRULinePtr = currentLinePtr;
        }
    }
    return LRULinePtr;
}

unsigned memoryAccesses(Cache_T cache, unsigned long address) {
    Set_T set = getSet(cache, address);
    // printf("Set: Tag", set.LineArray[0].Tag, set.LineArray[1].Tag);
    unsigned int validCount = 0;
    unsigned int Tag = address >> (cache.s + cache.b);
    for (size_t i = 0; i < cache.E; i++) {
        Line_T* linePtr = set.LineArray + i;
        if (linePtr->valid == 1) {
            validCount += 1;
            if (linePtr->Tag == Tag) {
                linePtr->time = time(NULL);
                return 0;
            }
        }
    }
    unsigned missFlag;
    Line_T* linePtr;
    if (validCount == cache.E) {
        linePtr = getLRULine(set, cache.E);
        missFlag = 1;
    } else {
        linePtr = getUnuseLine(set, cache.E);
        missFlag = 2;
    }
    linePtr->Tag = Tag;
    linePtr->valid = 1;
    linePtr->time = time(NULL);
    return missFlag;
} 

void dealOperM(Cache_T cache, Denote_T denote, Info_t* infoPtr) {
    unsigned int flag = memoryAccesses(cache, denote.address);
    memoryAccesses(cache, denote.address);
    char *desc;
    if (flag == 0) {
        desc = "hit hit";
        infoPtr->num_hits+=2;
    } else if (flag == 1) {
        infoPtr->num_hits+=1;
        infoPtr->num_evictions+=1;
        infoPtr->num_misses+=1;
        desc = "miss eviction hit";
    } else {
        infoPtr->num_hits+=1;
        infoPtr->num_misses+=1;
        desc = "miss hit";
    }
    printf("%c %lx,%d %s\n", denote.operation, denote.address, denote.size, desc); 
}


void dealOperLOrS(Cache_T cache, Denote_T denote, Info_t* infoPtr) {
    unsigned int flag = memoryAccesses(cache, denote.address);
    char *desc;
    if (flag == 0) {
        desc = "hit";
        infoPtr->num_hits++;
    } else if (flag == 1) {
        infoPtr->num_evictions+=1;
        infoPtr->num_misses+=1;
        desc = "miss eviction";
    } else {
        infoPtr->num_misses+=1;
        desc = "miss";
    }
     printf("%c %lx,%d %s\n", denote.operation, denote.address, denote.size, desc); 
}




void dealCommand(Cache_T cache, Denote_T denote, Info_t* infoPtr)
{

    if (denote.size > getB(cache.b)) {
        infoPtr->num_misses+=1;
        printf("%s miss\n", denote.description);
        return; 
    }

    switch (denote.operation) {
        case 'M':
            dealOperM(cache, denote, infoPtr);
            break;
        case 'L':
            dealOperLOrS(cache, denote, infoPtr);
            break;
        case 'S':
            dealOperLOrS(cache, denote, infoPtr);
            break;            
        default:
            break;
    }
}