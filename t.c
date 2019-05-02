#include <stdio.h>
#include <stdint.h>


int main() {

  const uint16_t BitMask[] = {1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768};

  uint8_t i;
  
  printf("BitMask:\n");
  for (i=0; i<16; i++) {
    printf("\t%u",BitMask[i]);
    
  }
  printf("\n");
  printf("BitShift:\n");
  for (i=0; i<16; i++) {
    printf("\t%u",(1<<i));
    
  }
  printf("\n");
  return 0;
}
