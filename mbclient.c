/*
 * Modbus Client che riceve connessioni da altri Server
 * Gestisce solo 1 connessione
 * Per connessioni multiple vedere il codice event.
 * Mette a disposizione 200 Bobine e 200 registri
 * Dovrà sustituire event.c per fargli gestire anche i comandi del cancello
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h> 
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <modbus.h>
#include <fcntl.h>
#include <sysexits.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "gh.h"


modbus_t *ctx = NULL; // listen socket
modbus_t *mb_plc = NULL;
modbus_t *mb_otb = NULL;

int s = -1; // main socket
modbus_mapping_t *mb_mapping = NULL; // registri del server


char *printbitssimple16(char *str, int16_t n) {
  /*dato l'intero n stampa la rappresentazione binaria*/
  uint16_t i;
  //  int j;
  i = (uint16_t)1<<(sizeof(n) * 8 - 1); /* 2^n */
  //char str[100];
  str[0]='\0';
  
  /* for (j=15;j>=0;j--) {    */
  /*   printf(" %2i",j); */
  /* } */
  /* printf("\n"); */
  
  while (i > 0) {
    if (n & i)
      strcat(str,"1");
    else
      strcat(str,"0");
    i >>= 1;
  }
  //printf("\n");
  return str;
}

///////////////////////////////////////////////////////////////////

int ts(char * tst, char * fmt)
{
  time_t current_time;
  char MYTIME[50];
  struct tm *tmp ;
  /* Obtain current time. */
  current_time = time(NULL);
  if (current_time == ((time_t)-1))
    {
      //logvalue(LOG_FILE, "Failure to obtain the current time\n");
      return -1;
    }
  tmp = localtime(&current_time);
  // [04/15/19 11:40AM] "[%F %T]"
  if (strftime(MYTIME, sizeof(MYTIME), fmt, tmp) == 0)
    {
      
      return -1;
    }
  strcpy(tst,MYTIME);
  return 0;
}

void logvalue(char *filename, char *message)
{
  /*scrive su filename il messaggio message*/
  FILE *logfile;
  char t[30];
  ts(t,"[%F %T]");
  //ts(t,"[%Y%m%d-%H%M%S]");
  logfile=fopen(filename,"a");
  if(!logfile) return;
  fprintf(logfile,"%s %s",t,message);
  fclose(logfile);
}

int pulsante(modbus_t *m,int bobina)
{  
  char errmsg[100];

  if ( modbus_write_bit(m,bobina,TRUE) != 1 ) {
    // The modbus_write_bit() function shall return 1 if successful. 
    // Otherwise it shall return -1 and set errno.
    sprintf(errmsg,"ERRORE DI SCRITTURA:PULSANTE ON %s\n",modbus_strerror(errno));
    logvalue(LOG_FILE,errmsg);
    return -1;
  }

  sleep(1);

  if ( modbus_write_bit(m,bobina,FALSE) != 1 ) {
    sprintf(errmsg,"ERRORE DI SCRITTURA:PULSANTE OFF %s\n",modbus_strerror(errno));
    logvalue(LOG_FILE,errmsg);
    return -1;
  }

  return 0;
}

int interruttore(modbus_t *m, int bit, uint16_t dest, uint16_t reg) {
  /* cambia lo stato del bit "bit" del registro reg */
  /* se "bit" era 0 va a 1 e viceversa */
  /* l'XOR (^) fa proprio questo */
  /* dest è il registro destinazione */
  /* nel caso OTB dest è il registro delle uscie è cioè 100 */
  /* reg è il registro con il nuovo valore */
  /* reg contiene il valore del registro prima di cambiarlo */
  reg=reg^(1<<bit);
  
  if ( modbus_write_register(m,dest,reg) != 1 ) {
    return -1;
  }
  return 0;
}

void rotate() {

  char t[20];
  char newname[40];
  char logmsg[100];
  int fd;

  ts(t,"%Y%m%d-%H%M%S");
  sprintf(newname,"cancello-%s.log",t);
  sprintf(logmsg,"Log rotation....archived to %s\n",newname);
  logvalue(LOG_FILE,logmsg);
  rename(LOG_FILE,newname);
  fd=open(LOG_FILE, O_RDONLY | O_WRONLY | O_CREAT,0644);
  close(fd);
}

void myCleanExit(char * from) {
  logvalue(LOG_FILE,from);
  unlink(LOCK_FILE);
  
  logvalue(LOG_FILE,"\tChiudo la connessione con PLC e OTB\n");
  modbus_close(mb_plc);
  modbus_close(mb_otb);
  
  logvalue(LOG_FILE,"\tLibero la memoria dalle strutture create\n");
  modbus_free(mb_plc);
  modbus_close(mb_otb);
  
  logvalue(LOG_FILE,"\tChiudo il socket e le strutture del server\n");
  modbus_mapping_free(mb_mapping);

  if (s != -1) {
    close(s);
  }
  
  modbus_close(ctx);
  modbus_free(ctx);

  logvalue(LOG_FILE,"Fine.\n");
  logvalue(LOG_FILE,"****************** END **********************\n");
  
}

void signal_handler(int sig)
{
  switch(sig) {

  case SIGHUP:
    // ----------------------------------
    rotate();
    logvalue(LOG_FILE,"new log file after rotation\n");
    break;
    // ---------------------------------
    
  case SIGTERM:
    myCleanExit("Catched terminate signal\n");
    exit(EX_OK);
    break;
  }
}

void daemonize()
{
  int i,lfp;
  char str[10];
  if(getppid()==1) return; /* already a daemon */
  i=fork();
  if (i<0) exit(EX_OSERR); /* fork error */
  if (i>0) exit(EX_OK); /* parent exits */
  /* child (daemon) continues */
  setsid(); /* obtain a new process group */
  for (i=getdtablesize();i>=0;--i) close(i); /* close all descriptors */
  i=open("/dev/null",O_RDWR); dup(i); dup(i); /* handle standart I/O */
  umask(027); /* set newly created file permissions */
  // chdir(RUNNING_DIR); /* change running directory */
  lfp=open(LOCK_FILE,O_RDWR|O_CREAT,0640);
  if (lfp<0) exit(EX_OSERR); /* can not open */

  if (lockf(lfp,F_TLOCK,0)<0) {
    logvalue(LOG_FILE,"Cannot Start. There is another process running. exit\n");
    exit(EX_OK); /* can not lock */
  }

  /* first instance continues */
  snprintf(str,(size_t)10,"%d\n",getpid());
  write(lfp,str,strlen(str)); /* record pid to lockfile */
  
  signal(SIGCHLD,SIG_IGN); /* ignore child */
  signal(SIGTSTP,SIG_IGN); /* ignore tty signals */
  signal(SIGTTOU,SIG_IGN);
  signal(SIGTTIN,SIG_IGN);
  signal(SIGPIPE,SIG_IGN);
  signal(SIGHUP,signal_handler); /* catch hangup signal */
  signal(SIGTERM,signal_handler); /* catch kill signal */
  logvalue(LOG_FILE,"****************** START **********************\n");
}

///////////////////////////////////////////////////////////////////

//-----------------------------
void conn() {
  char errmsg[100];
  mb_plc = modbus_new_tcp("192.168.1.157",502);
  mb_otb = modbus_new_tcp("192.168.1.11",502);

  if ( (modbus_connect(mb_plc) == -1 ))
    {
      sprintf(errmsg,"ERRORE non riesco a connettermi con il PLC %s\n",modbus_strerror(errno));
      logvalue(LOG_FILE,errmsg);
      myCleanExit(errmsg);
      exit(EXIT_FAILURE);
    } else {
    logvalue(LOG_FILE,"\t Connesso con PLC\n");
  }

  if ( (modbus_connect(mb_otb) == -1))
    {
      sprintf(errmsg,"ERRORE non riesco a connettermi con l'OTB. Premature exit [%s]\n",modbus_strerror(errno));
      logvalue(LOG_FILE,errmsg);
      myCleanExit(errmsg);
      exit(EXIT_FAILURE);
    } else {
    logvalue(LOG_FILE,"\t Connesso con OTB\n");
  }
}
//-----------------------------

int main()
{
    
  int rc;
  int retval;
  uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
  
  fd_set rfds;
  fd_set current_rfds;
  int current_socket;
  int fdmax;
  
  uint16_t oldvalbit=0;
  uint16_t newvalbit=0;

  
#ifdef PIPPO
  printbitssimple16(msg,1);
  printf("1 --> %s\n",msg);
  printbitssimple16(msg,2);
  printf("2 --> %s\n",msg);
  printbitssimple16(msg,4);
  printf("4 --> %s\n",msg);
  printbitssimple16(msg,8);
  printf("8 --> %s\n",msg);
  printbitssimple16(msg,16);
  printf("16 --> %s\n",msg);
  printbitssimple16(msg,32);
  printf("32 --> %s\n",msg);
  printbitssimple16(msg,64);
  printf("64 --> %s\n",msg);
  printbitssimple16(msg,128);
  printf("128 --> %s\n",msg);
  printbitssimple16(msg,256);
  printf("256 --> %s\n",msg);
  printbitssimple16(msg,512);
  printf("512 --> %s\n",msg);
  printbitssimple16(msg,1024);
  printf("1024 --> %s\n",msg);
  printbitssimple16(msg,2048);
  printf("2048 --> %s\n",msg);
  printbitssimple16(msg,1536);
  printf("1536 --> %s\n",msg);  
  return 0;
#endif

  
  daemonize();

  conn();
  ctx = modbus_new_tcp("192.168.1.110", 1502);
  mb_mapping = modbus_mapping_new(200, 0, 200, 0);
  
  if (mb_mapping == NULL) {
    fprintf(stderr, "Failed to allocate the mapping: %s\n",
	    modbus_strerror(errno));
    modbus_free(ctx);
    return -1;
  }

  s = modbus_tcp_listen(ctx, 10);
  //modbus_tcp_accept(ctx, &s);

  FD_ZERO(&rfds);
  FD_SET(s, &rfds);  
  fdmax = s;

  oldvalbit=newvalbit;
  
  for(;;) {

    current_rfds = rfds;
    
    retval = select(fdmax+1,&current_rfds,NULL,NULL,NULL);
    if (retval == -1) {
      logvalue(LOG_FILE,"Err: select() failure\n");
      exit(-1);
    }

    for (current_socket = 0; current_socket <= fdmax; current_socket++) {	
      if (!FD_ISSET(current_socket, &current_rfds)) {
	continue;
      }
      //-----------------------        
      /*************** NUOVA CONNESSIONE ************************/
      if (current_socket == s) {
	
	socklen_t addrlen;
	struct sockaddr_in clientaddr;
	int newfd;
	
	addrlen = sizeof(clientaddr);
	memset(&clientaddr, 0, sizeof(clientaddr));
	newfd=accept(s, (struct sockaddr *)&clientaddr, &addrlen);
	
	if (newfd==-1)
	  logvalue(LOG_FILE,"Err: Server accept() error\n");
	else
	  FD_SET(newfd, &rfds);
	if (newfd > fdmax) {
	  /* Keep track of the maximum */
	  fdmax = newfd;
	}  

	/*
	printf("New connection from %s:%d on socket %d\n",
	       inet_ntoa(clientaddr.sin_addr),
	       clientaddr.sin_port, 
	       current_socket);
	*/	
      }
      /*********************************************************/      
      else
	{
	/* gestisco dati provenienti da una connessione già stabilita */
	modbus_set_socket(ctx, current_socket);
	rc = modbus_receive(ctx, query);
	
	if (rc > 0) {
	  
	  modbus_reply(ctx, query, rc, mb_mapping);
	  
	  /////////////////////////////////////////////////
	  int offset = modbus_get_header_length(ctx);
	  /***** Estraggo il codice richiesta *****/
	  // uint16_t reg_address = (query[offset + 1]<< 8) + query[offset + 2];		  
	  
	  switch (query[offset]) {
	  case 0x05: {/* il PLC sta chiedendo di scrivere BITS (non caèpita con il TWIDO)*/

	    /*
	      printf("\til PLC sta chiedendo di scrivere BITS\n");
	      printf("\tCoil Registro %d-->%s [0x%02X]\n",
	      reg_address,
	      (mb_mapping->tab_bits[reg_address])?"ON":"OFF",
	      query[offset]);
	    */     
	  }
	    break;
	  case 0x10:
	  case 0x06: {/* il PLC sta chiedendo di scrivere N registri  */

	    /*
	      printf("\til PLC sta chiedendo di scrivere REGISTRI\n");	    
	      printf("\t%d-->%i [0x%02X]\n",reg_address,
	      mb_mapping->tab_registers[reg_address],
	      query[offset]);
	    */
	    
	    
	    /***********/
	    // do something with data sent


	    /* OTB */
	    /* OTB gestione transizioni BIT registro ingressi */
	    newvalbit=mb_mapping->tab_registers[OTBDIN];

	    
	    if (oldvalbit!=newvalbit) {
	      uint8_t cur;
#ifdef DIFF	      
	      uint16_t diff;
	      char str[100];
	      diff = oldvalbit^newvalbit;	    
	      sprintf(msg,"\toldval:\t[%i],\t%s\n",oldvalbit,printbitssimple16(str,oldvalbit));
	      logvalue(LOG_FILE,msg);
	      sprintf(msg,"\tnewval:\t[%i],\t%s\n",newvalbit,printbitssimple16(str,newvalbit));
	      logvalue(LOG_FILE,msg);
	      sprintf(msg,"\tdiff:\t[%i],\t%s\n",diff,printbitssimple16(str,diff));
	      logvalue(LOG_FILE,msg);
#endif	    
	      for (cur=0;cur<12;cur++) { // 12 num ingressi digitali OTB
		//------------------------------------------------------
		if (!CHECK_BIT(oldvalbit,cur) && CHECK_BIT(newvalbit,cur)) {
		  switch ( cur ) {
		  case FARI_ESTERNI_IN_SOPRA: {
		    logvalue(LOG_FILE,"Fari Esterni Sopra\n");
		    break;
		  }
		  case FARI_ESTERNI_IN_SOTTO: {
		    logvalue(LOG_FILE,"Fari Esterni Sotto\n");
		    break;
		  }
		    
		  case OTB_IN9: {
		    logvalue(LOG_FILE,"Apertura Totale Cancello\n");
		    if (FALSE) {
		      if (pulsante(mb_plc,APERTURA_TOTALE)<0) {
			logvalue(LOG_FILE,"\tproblemi con pulsante durante apertura totale\n");
		      }
		    }
		    break;
		  }		    
		  case OTB_IN8: {
		    logvalue(LOG_FILE,"Apertura Parziale Cancello\n");
		    if (FALSE) {
		      if (pulsante(mb_plc,APERTURA_PARZIALE)<0) {
			logvalue(LOG_FILE,"\tproblemi con pulsante durante apertura parziale\n");
		      }
		    }
		    break;
		  }
		    
		  case OTB_IN6: {
		    break;
		  }
		  case OTB_IN5: {
		    break;
		  }
		  case OTB_IN4: {
		    break;
		  }
		  case OTB_IN3: {
		    break;
		  }
		  case OTB_IN2: {
		    break;
		  }
		  case OTB_IN1: {
		    break;
		  }
		  case OTB_IN0: {
		    break;
		  }
		  }		      
		  // sprintf(msg,"\tBit %s (num. %i): 0->1\n",otbdigitalinputs[cur],cur);
		  // logvalue(LOG_FILE,msg);
		}
		if (CHECK_BIT(oldvalbit,cur) && !CHECK_BIT(newvalbit,cur)) {
		  //sprintf(msg,"\tBit %s (num. %i): 1->0\n",otbdigitalinputs[cur],cur);
		  //logvalue(LOG_FILE,msg);
		}
		//-------------------------------		
	      }
	    }
	    oldvalbit=newvalbit;
	    /***********/
	  }
	    break;
	  }
	  /////////////////////////////////////////////////      
	} else if (rc  == -1) {
	  /* il client ha chiuso la connessione */
	  close(current_socket);
	  FD_CLR(current_socket, &rfds);	
	  
	  if (current_socket == fdmax) {
	    fdmax--;
	  }
	  /*printf("Pulito il socket....\n"); */
	}
	}
    } /* fine current_socket */
  } /* fine for (;;) */
  return 0;
}
