#include "cachelab.h"
#include "stdio.h"
#include "string.h"
int main(int argc, char *argv[])
{
  Info_t info;
  info.num_hits = 0;
  info.num_misses = 0;
  info.num_evictions = 0;
  Cache_T cache;
  unsigned int s = 0,b = 0,E = 0;
  char* filePath;
  // Read Command Argunments
  for (size_t i = 0; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (argv[i][1] == 's') {
        sscanf(argv[++i], "%d", &s);
      } else if (argv[i][1] == 'b') {
        sscanf(argv[++i], "%d", &b);
      } else if (argv[i][1] == 'E') {
        sscanf(argv[++i], "%d", &E);
      } else if (argv[i][1] == 't') {
        filePath = argv[++i];
      }
    }
  }

  if (s == 0 && b ==0 && E == 0) {
    printf("arguments Error!");
    return -1;
  }
  cache = initCache(s, E, b);

  FILE *fp = NULL;
  char buff[255];
  fp = fopen(filePath, "r");
  while (fgets(buff, 255, fp)) {
      if (buff[0] == ' ') {
        char *ptr_comma = strrchr(buff, ',');
        char *ptr_space = strrchr(buff, ' ');
        unsigned address_len = ptr_comma - ptr_space - 1;
        char address[255] = "";
        
        strncpy(address, ptr_space + 1, address_len);
        char operation = buff[1];
        unsigned size;
        sscanf(ptr_comma + 1, "%d", &size);
        Denote_T denote;
        denote.operation = operation;
        sscanf(address, "%lx", &(denote.address));
        denote.size = size;
        denote.description = buff;
        dealCommand(cache, denote, &info);
      }
  };
  printSummary(info.num_hits, info.num_misses, info.num_evictions);
  return 0;
}


