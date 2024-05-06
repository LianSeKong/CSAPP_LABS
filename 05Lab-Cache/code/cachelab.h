/* 
 * cachelab.h - Prototypes for Cache Lab helper functions
 */

#ifndef CACHELAB_TOOLS_H
#define CACHELAB_TOOLS_H

#define MAX_TRANS_FUNCS 100

typedef struct trans_func{
  void (*func_ptr)(int M,int N,int[N][M],int[M][N]);
  char* description;
  char correct;
  unsigned int num_hits;
  unsigned int num_misses;
  unsigned int num_evictions;
} trans_func_t;

// Cache consist of E Array
// E consist of L Array
// L consist of [valid bit, flag, block] Array


// Line 

typedef struct Line {
  void* CacheBlock;     // block 
  unsigned int valid;  // 1: valid, 0: invalid 
  unsigned int Tag;    // Check for equality
  unsigned int time; // LRU 
} Line_T;

typedef struct Set
{
  Line_T* LineArray;
} Set_T;

typedef struct Cache {
  unsigned int s;
  unsigned int E;
  unsigned int b;
  Set_T* SetArray;
} Cache_T;

// denote format:  [space]operation address,size

typedef struct Denote {
  char operation;
  unsigned long address;
  unsigned int size;
  char* description;
} Denote_T;

typedef struct Info {
  unsigned int num_hits;
  unsigned int num_misses;
  unsigned int num_evictions;
} Info_t;

Cache_T initCache(unsigned int s, unsigned int E, unsigned int b);

// Number of set index bits (S = 2^s)

unsigned int getS(unsigned int s);

// Number of block bit (B = 2^b)

unsigned int getB(unsigned int b);


unsigned long getSetIndex(unsigned int s, unsigned int b, unsigned long address);

Set_T getSet(Cache_T cache, unsigned long address);

Line_T* getUnuseLine(Set_T set, unsigned int E);

Line_T* getLRULine(Set_T set, unsigned int E);
unsigned memoryAccesses(Cache_T cache, unsigned long address);
void dealOperM(Cache_T cache, Denote_T denote, Info_t* infoPtr);
void dealOperLOrS(Cache_T cache, Denote_T denote, Info_t* infoPtr);

void dealCommand(Cache_T cache, Denote_T denote, Info_t* infoPtr);


/* 
 * printSummary - This function provides a standard way for your cache
 * simulator * to display its final hit and miss statistics
 */ 
void printSummary(int hits,  /* number of  hits */
				  int misses, /* number of misses */
				  int evictions); /* number of evictions */

/* Fill the matrix with data */
void initMatrix(int M, int N, int A[N][M], int B[M][N]);

/* The baseline trans function that produces correct results. */
void correctTrans(int M, int N, int A[N][M], int B[M][N]);

/* Add the given function to the function list */
void registerTransFunction(
    void (*trans)(int M,int N,int[N][M],int[M][N]), char* desc);

#endif /* CACHELAB_TOOLS_H */
