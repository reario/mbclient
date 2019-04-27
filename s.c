/*
 * Copyright © 2008-2014 Stéphane Raimbault <stephane.raimbault@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <modbus.h>


int main()
{
  int s = -1;
  modbus_t *ctx = NULL;
  modbus_mapping_t *mb_mapping = NULL;
  int rc;
  uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
  
  ctx = modbus_new_tcp("192.168.1.110", 1502);
  mb_mapping = modbus_mapping_new(200, 0, 200, 0);
  
  if (mb_mapping == NULL) {
    fprintf(stderr, "Failed to allocate the mapping: %s\n",
	    modbus_strerror(errno));
    modbus_free(ctx);
    return -1;
  }

  s = modbus_tcp_listen(ctx, 1);
  modbus_tcp_accept(ctx, &s);
  
  for(;;) {
   
    rc = modbus_receive(ctx, query);
    if (rc > 0) {
      printf("ricevuto qualcosa....\n");      
      modbus_reply(ctx, query, rc, mb_mapping);
      
      /////////////////////////////////////////////////
      int offset = modbus_get_header_length(ctx);
      
      /***** Estraggo il codice richiesta *****/
      uint16_t address = (query[offset + 1]<< 8) + query[offset + 2];		  
      
      switch (query[offset]) {
      case 0x05: {/* il PLC sta chiedendo di scrivere BITS */		      
	printf("\til PLC sta chiedendo di scrivere BITS\n");
	printf("\tCoil Registro %d-->%s [0x%02X]\n",address,(mb_mapping->tab_bits[address])?"ON":"OFF",query[offset]);     
      }
	break;
      case 0x10:
      case 0x06: {/* il PLC sta chiedendo di scrivere N registri  */
	printf("\til PLC sta chiedendo di scrivere REGISTRI\n");
	printf("\t%d-->%i [0x%02X]\n",address,mb_mapping->tab_registers[address],query[offset]);
      }
	break;
      }
      /////////////////////////////////////////////////      
    } else if (rc  == -1) {
      close(s);
      s = modbus_tcp_listen(ctx, 1);
      modbus_tcp_accept(ctx, &s);
      /* Connection closed by the client or error */
      //break;
    }    
  }

  printf("Quit the loop: %s\n", modbus_strerror(errno));

  modbus_mapping_free(mb_mapping);
  if (s != -1) {
    close(s);
  }
  /* For RTU, skipped by TCP (no TCP connect) */
  modbus_close(ctx);
  modbus_free(ctx);
  
  return 0;
}
