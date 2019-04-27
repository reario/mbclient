

#include <stdio.h>

#include <stdint.h>

int main() {
  //  uint8_t i=16;
  uint16_t v;
  
  for (v=16; v-->0;) {
    printf("%i  %i\n",v,1<<v);
	//printf("%i\t",1<<v);

  }
  printf("\n");
  return 0;
}
