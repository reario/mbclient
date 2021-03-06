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
#include <semaphore.h>
#include "gh.h"

modbus_t *ctx = NULL; // listen socket
modbus_mapping_t *mb_mapping = NULL; // registri del server
int s = -1; // main socket

typedef struct datasignal {
  modbus_t *m; // la connessione al device
  uint8_t b; // la bobina o il registro
  sem_t *sem; // il semaforo per il lock
} datasignal_t;

sem_t sem_otb;
sem_t sem_plc;

char *printbitssimple16(char *str, int16_t n) {
  /* dato l'intero n ritorna la rappresentazione binaria nella stringa str */
  uint16_t i;
  //  int j;
  i = (uint16_t)1<<(sizeof(n) * 8 - 1); /* 2^n */
  //char str[100];
  str[0]='\0';  
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

void mylog(char *filename, char *f, const char *message) {

  FILE *logfile;
  char t[30];
  ts(t,"[%F %T]");
  //ts(t,"[%Y%m%d-%H%M%S]");
  logfile=fopen(filename,"a");
  if(!logfile) return;
  fprintf(logfile,"%s ",t);
  fprintf(logfile,f,message);
  fclose(logfile);
  
}
/****************************************************/
void buttonRelease(int sig, siginfo_t *si, void *uc) {
  /*
   | Viene chiamata da un SIGNAL (SIGUSR1) scatenato da un timer che è impostato 
   | al momento della pressione di un tasto (la funzione pulsante())
   | quello che fa è mettere a OFF la bobina che la funzione pulsante() 
   | aveva messo a ON
   | E' asincrona e viene chiamata con un timer impostato a 1.5 sec.
   | *si è di tipo siginfo_t e tra le altre cose, contiene una UNION 
   | valorizzata con il valore
   | di un puntatore ad una struttura che contiene il context modbus (la connessione) 
   | e la bobna da resettare.
   */
  
  char msg[100];
  modbus_t *mbus;
  uint8_t B;
  sem_t *s;
  datasignal_t *dsig = (datasignal_t *)si->si_value.sival_ptr;
  
  mbus = dsig->m;
  B = dsig->b;
  s = dsig->sem;
  if ( modbus_write_bit(mbus,B,FALSE) != 1 ) {
    sprintf(msg,"ERRORE DI SCRITTURA:PULSANTE OFF %s\n",modbus_strerror(errno));
    logvalue(LOG_FILE,msg);
    return;
  } else { logvalue(LOG_FILE,"\tPulsante rilasciato...1->0\n");}
  
  modbus_close(mbus);
  modbus_free(mbus);
  // sem_post(s);
  UNLOCK(s);
}

static int makeTimer( datasignal_t *dsignal)
// dsig contiene il modbuscontex e il numero della bobina/bit
{
  struct sigevent te;
  struct itimerspec its;
  struct sigaction sa;
  timer_t timerID;
  int sigNo = SIGUSR1;
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = buttonRelease;
  sigemptyset(&sa.sa_mask);
  if (sigaction(sigNo, &sa, NULL) == -1) {
    logvalue(LOG_FILE,"sigaction\n");
  }  
  te.sigev_notify = SIGEV_SIGNAL;
  te.sigev_signo = sigNo;
  te.sigev_value.sival_ptr = dsignal;
  if (timer_create(CLOCK_REALTIME, &te, &timerID)<0)
    logvalue(LOG_FILE,"\tErr creazione Timer\n");
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_nsec = 0 * 1000000; // attivo solo una volta
  its.it_value.tv_sec = 1;
  its.it_value.tv_nsec = 500 * 1000000;
  if (timer_settime(timerID, 0, &its, NULL)<0)
    logvalue(LOG_FILE,"\tErr nel setTimer\n");

  return 0;
}
/****************************************************/

int pulsante(modbus_t *m, int bobina, sem_t *sem)
{  
  char errmsg[100];
  static datasignal_t ds;
  if (  modbus_write_bit(m,bobina,TRUE) != 1 ) {

    sprintf(errmsg,"ERRORE DI SCRITTURA:PULSANTE ON %s\n",modbus_strerror(errno));
    logvalue(LOG_FILE,errmsg);
    return -1;

    logvalue(LOG_FILE,"\tPulsante premuto...luci studio sotto 0->1\n");
  } 

  ds.m = m;
  ds.b = bobina;
  ds.sem = sem;
  makeTimer(&ds); // setta il timer per il rilasio del pulsante
  
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
  
  logvalue(LOG_FILE,"\tChiudo il socket e le strutture del server\n");
  modbus_mapping_free(mb_mapping);
  if (s != -1) {
    close(s);
  }
  modbus_close(ctx);
  modbus_free(ctx);
  
  logvalue(LOG_FILE,"Fine.\n");
  logvalue(LOG_FILE,"****************** END **********************\n");
  exit(EXIT_SUCCESS);
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
  signal(SIGUSR1,SIG_IGN);
  signal(SIGHUP,signal_handler); /* catch hangup signal */
  signal(SIGTERM,signal_handler); /* catch kill signal */
  logvalue(LOG_FILE,"****************** START **********************\n");
}

uint16_t interruttore (modbus_t *m, uint16_t R, const uint8_t COIL, const uint8_t V) {
  /* 
     ad ogni sua chiamata questa funzione inverte lo stato del bit COIL 
     nel registro R a seconda del valore di V: V=TRUE 0->1, V=FALSE 1>0 
  */
  /*------------------------------------------*/  
  /*---> Usa la MaskWrite FC22 del modbus <---*/
  /*------------------------------------------*/
  /* V=TRUE se transizione da 0->1, V=FALSE se transizione da 1->0 */
  /* R num registro remoto (per OTB R = registro delle uscite ad indirizzo=100 */
  /* COIL il numero del BIT del registro R da mettere a 1 o a 0 in base al valore di V */
  /* IL TWIDO NON SUPPORTA LA FC022 !!! */
  /* ---------------------------------- */
  char errmsg[100];
  uint16_t mask_coil;

  mask_coil = (1<<COIL);
  uint16_t and_mask = ~mask_coil; 
  uint16_t or_mask = V ? mask_coil : ~mask_coil;    


  // int modbus_write_registers(modbus_t *ctx, int addr, int nb, const uint16_t *src);
  if (modbus_mask_write_register(m,R,and_mask,or_mask) == -1) {
    sprintf(errmsg,"ERRORE nella funzione interruttore %s\n",modbus_strerror(errno));
    logvalue(LOG_FILE,errmsg);
    return -1;
  }
  return 0;
}

uint16_t gestioneOTB() {

  uint8_t cur;
  char msg[100];
  
  if (oldvalbitOTB!=newvalbitOTB) {
    
    for (cur=0;cur<12;cur++) { // 12 num ingressi digitali OTB
      if (!CHECK_BIT(oldvalbitOTB,cur) && CHECK_BIT(newvalbitOTB,cur)) {
	switch ( cur ) {
	  /*----------------------------------------------------------------*/  
	case FARI_ESTERNI_IN_SOPRA: {
	  logvalue(LOG_FILE,"Fari Esterni Sopra\n");
	  break;
	}
	  /*----------------------------------------------------------------*/  
	case FARI_ESTERNI_IN_SOTTO: {
	  logvalue(LOG_FILE,"Fari Esterni Sotto\n");
	  break;
	}
	  /*----------------------------------------------------------------*/  
	case OTB_IN9: {
	  // l'ingresso 9 di otb è stato chiuso, attivo l'uscita del plc per aprire il cancello totale
	  logvalue(LOG_FILE,"Apertura Totale Cancello\n");
	  if (FALSE) {
	    modbus_t *mb_plc = mb_plc = modbus_new_tcp("192.168.1.157", 502);
	    // while ( sem_wait(&sem_plc) == -1 && errno == EINTR) continue;	    
	    LOCK(&sem_plc);
	    if ( (modbus_connect(mb_plc) == -1 )) {
	      sprintf(msg,"ERRORE non riesco a connettermi con il PLC nella fase 0->1 [%s]\n",modbus_strerror(errno));
	      logvalue(LOG_FILE,msg);
	    } else {
	      if (pulsante(mb_plc,APERTURA_TOTALE,&sem_plc)<0) {
		logvalue(LOG_FILE,"\tproblemi con pulsante durante apertura totale\n");
	      }
	    }
	  } // if (FALSE) ....
	  break;
	}
	  /*----------------------------------------------------------------*/  
	case OTB_IN8: {
	  // l'ingresso 9 di otb è stato chiuso, attivo l'uscita del plc per aprire il cancello parziale
	  logvalue(LOG_FILE,"Apertura Parziale Cancello\n");
	  if (FALSE) {
	    modbus_t *mb_plc = modbus_new_tcp("192.168.1.157", 502);
	    //while ( sem_wait(&sem_plc) == -1 && errno == EINTR) continue;
	    LOCK(&sem_plc);
	    if ( (modbus_connect(mb_plc) == -1 )) {
	      sprintf(msg,"ERRORE non riesco a connettermi con il PLC nella fase 0->1 [%s]\n",modbus_strerror(errno));
	      logvalue(LOG_FILE,msg);
	    } else {
	      if (pulsante(mb_plc,APERTURA_PARZIALE,&sem_plc)<0) {
		logvalue(LOG_FILE,"\tproblemi con pulsante durante apertura parziale\n");
	      }
	      // modbus_close(mb_plc);  
	    }
	    // modbus_free(mb_plc);
	  }
	  break;
	}
	  /*----------------------------------------------------------------*/  
	case OTB_IN7: {
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
      }
      if (CHECK_BIT(oldvalbitOTB,cur) && !CHECK_BIT(newvalbitOTB,cur)) {
	// qui vanno messe le funzioni per la gestione 1->0 come per gli
	// interruttori che devono invertire lo stato precedente
      }
    } // for (cur=0;....)
  } // oldvalBIT != newvalBIT
  return 0;
}

uint16_t gestioneIN0() {
  // IN0 registro 0 dei 200 impostati su questo server
  uint8_t cur;
  char msg[100];
  
  if (oldvalbitIN0!=newvalbitIN0) {
    /*
      printbitssimple16(msg,oldvalbitIN0);
      mylog(LOG_FILE,"\t%s\n",msg);
      printbitssimple16(msg,newvalbitIN0);
      mylog(LOG_FILE,"\t%s\n",msg);
    */
    for (cur=0;cur<16;cur++) { // registro 16 bit
      if ( !CHECK_BIT(oldvalbitIN0,cur) && CHECK_BIT(newvalbitIN0,cur) )
	{		
	  switch ( cur ) {
	  case IN00: {
	    logvalue(LOG_FILE,"IN00 0->1\n");

	    modbus_t *mb_otb = modbus_new_tcp("192.168.1.11", 502);
	    LOCK(&sem_otb);
	    if ( (modbus_connect(mb_otb) == -1 )) {
	      sprintf(msg,"ERRORE non riesco a connettermi con l'OTB nella fase 0->1 [%s]\n",modbus_strerror(errno));
	      logvalue(LOG_FILE,msg);
	    } else {
	      if (interruttore(mb_otb,100,FARI_ESTERNI_SOPRA,TRUE) == 0) { // 100 = regisro uscite OTB
		logvalue(LOG_FILE,"\tFunzione interruttore avvenuta...0->1\n");
	      }
	      modbus_close(mb_otb);
	    }
	    UNLOCK(&sem_otb);
	    modbus_free(mb_otb);
	    
	    break;
	  } // Fari Esterni IN00
	    
	  case IN01: {
	    logvalue(LOG_FILE,"IN01 0->1\n");
	    modbus_t *mb_otb = modbus_new_tcp("192.168.1.11" ,502);
	    //while ( sem_wait(&sem_otb ) == -1 && errno == EINTR) continue;
	    LOCK(&sem_otb);
	    if ( (modbus_connect(mb_otb) == -1 )) {
	      sprintf(msg,"ERRORE non riesco a connettermi con l'OTB nella fase 0->1 [%s]\n",modbus_strerror(errno));
	      logvalue(LOG_FILE,msg);
	    } else {
	      if (interruttore(mb_otb,100,FARI_ESTERNI_SOTTO,TRUE) == 0) { // 100 = regisro uscite OTB
		logvalue(LOG_FILE,"\tFunzione interruttore avvenuta...0->1\n");
	      }
	      modbus_close(mb_otb);
	    }
	    UNLOCK(&sem_otb);
	    modbus_free(mb_otb);
	    break;
	  }
	    
	  case IN02: { // inserire num 4 nel registro
	    /*..................................................*/
	    logvalue(LOG_FILE,"IN02 0->1\n");
	    modbus_t *mb_plc = modbus_new_tcp("192.168.1.157" ,502);
	    //while ( sem_wait(&sem_plc) == -1 && errno == EINTR) continue;
	    LOCK(&sem_plc);
	    if ( (modbus_connect(mb_plc) == -1 )) {
	      sprintf(msg,"ERRORE non riesco a connettermi con il PLC nella fase 0->1 [%s]\n",modbus_strerror(errno));
	      logvalue(LOG_FILE,msg);
	    } else {
	      /**/  if (pulsante(mb_plc,LUCI_STUDIO_SOTTO,&sem_plc) == -1) { /**/
		logvalue(LOG_FILE,"\tErrore Pulsante premuto...luci studio sotto 0->1\n");
	      } 
	      // modbus_close(mb_otb); // il rilascio del pulsante avviene dentro buttonRelease dove
	      // sono chiuse le connessioni di mbus e liberata la memoria: mbus_free 
	    }
	    // modbus_free(mb_otb);
	    /*..................................................*/
	    break;
	  }

	  case IN03: {
	    logvalue(LOG_FILE,"IN03 0->1\n");
	    /*.......registro 888 del PLC per prova funzione FC022 .......*/
	    /* tramite questo bit 3 del registro 0, provo ad accendere e spegnere il bit 4 */
	    /* del registro 888 del PLC */
	    modbus_t *mb_plc = modbus_new_tcp("192.168.1.157" ,502);
	    LOCK(&sem_plc);
	    if ( (modbus_connect(mb_plc) == -1 )) {
	      sprintf(msg,"ERRORE non riesco a connettermi con il PLC registro 8888 nella fase 0->1 [%s]\n",modbus_strerror(errno));
	      logvalue(LOG_FILE,msg);
	    } else {
	      if (interruttore(mb_plc,888,4,TRUE) == 0) {
		logvalue(LOG_FILE,"\tFunzione interruttore avvenuta...0->1\n");
	      }
	      modbus_close(mb_plc);
	    }
	    UNLOCK(&sem_plc);
	    modbus_free(mb_plc);
	    /*........................*/
	    break;
	  }	    
	  case IN04: {
	    logvalue(LOG_FILE,"IN04 0->1\n");
	    break;
	  }
	  case IN05: {
	    logvalue(LOG_FILE,"IN05 0->1\n");
	    break;
	  }
	  case IN06: {
	    logvalue(LOG_FILE,"IN06 0->1\n");
	    break;
	  }
	  case IN07: {
	    logvalue(LOG_FILE,"IN07 0->1\n");
	    break;
	  }
	  case IN08: {
	    logvalue(LOG_FILE,"IN08 0->1\n");
	    break;
	  }
	  case IN09: {
	    logvalue(LOG_FILE,"IN09 0->1\n");
	    break;
	  }
	  case IN10: {
	    logvalue(LOG_FILE,"IN10 0->1\n");
	    break;
	  }
	  case IN11: {
	    logvalue(LOG_FILE,"IN11 0->1\n");
	    break;
	  }
	  case IN12: {
	    logvalue(LOG_FILE,"IN12 0->1\n");
	    break;
	  }
	  case IN13: {
	    logvalue(LOG_FILE,"IN13 0->1\n");
	    break;
	  }
	  case IN14: {
	    logvalue(LOG_FILE,"IN14 0->1\n");
	    break;
	  }
	  case IN15: {
	    logvalue(LOG_FILE,"IN15 0->1\n");
	    break;
	  }
	  }
	}

      if (CHECK_BIT(oldvalbitIN0,cur) && !CHECK_BIT(newvalbitIN0,cur)) {
	switch ( cur ) {
	case IN00: {	  
	    logvalue(LOG_FILE,"IN00 1->0\n");
	    modbus_t *mb_otb = modbus_new_tcp("192.168.1.11" ,502);
	    LOCK(&sem_otb);
	    if ( (modbus_connect(mb_otb) == -1 )) {
	      sprintf(msg,"ERRORE non riesco a connettermi con l'OTB nella fase 0->1 [%s]\n",modbus_strerror(errno));
	      logvalue(LOG_FILE,msg);
	    } else {
	      if (interruttore(mb_otb,100,FARI_ESTERNI_SOPRA,FALSE) == 0) { // 100 = regisro uscite OTB
		logvalue(LOG_FILE,"\tFunzione interruttore avvenuta...1->0\n");
	      }
	      modbus_close(mb_otb);
	    }
	    UNLOCK(&sem_otb);
	    modbus_free(mb_otb);
	    break;
	}

	case IN01: {
	  logvalue(LOG_FILE,"IN00 1->0\n");
	  modbus_t *mb_otb = modbus_new_tcp("192.168.1.11" ,502);
	  LOCK(&sem_otb);
	  if ( (modbus_connect(mb_otb) == -1 )) {
	    sprintf(msg,"ERRORE non riesco a connettermi con l'OTB nella fase 0->1 [%s]\n",modbus_strerror(errno));
	    logvalue(LOG_FILE,msg);
	  } else {
	    if (interruttore(mb_otb,100,FARI_ESTERNI_SOTTO,FALSE) == 0) { // 100 = regisro uscite OTB
	      logvalue(LOG_FILE,"\tFunzione interruttore avvenuta...1->0\n");
	    }
	    modbus_close(mb_otb);
	  }
	  UNLOCK(&sem_otb);
	  modbus_free(mb_otb);	  
	  break;
	}
	case IN02: {
	}
	case IN03: {
	  logvalue(LOG_FILE,"IN03 1->0\n");
	  /*.......registro 888 del PLC per prova funzione FC022 .......*/
	  /* tramite questo bit 3 del registro 0, provo ad accendere e spegnere il bit 4 */
	  /* del registro 888 del PLC */
	  modbus_t *mb_plc = modbus_new_tcp("192.168.1.157" ,502);
	  LOCK(&sem_plc);
	  if ( (modbus_connect(mb_plc) == -1 )) {
	    sprintf(msg,"ERRORE non riesco a connettermi con il PLC registro 8888 nella fase 1->0 [%s]\n",modbus_strerror(errno));
	    logvalue(LOG_FILE,msg);
	  } else {
	    if (interruttore(mb_plc,888,4,FALSE) == 0) {
	      logvalue(LOG_FILE,"\tFunzione interruttore avvenuta...1->0\n");
	    }
	    modbus_close(mb_plc);
	  }
	  UNLOCK(&sem_plc);
	  modbus_free(mb_plc);
	  /*........................*/
	}
	}
      }
    }
  }

  return 0;
}

int main()
{ 
  int rc;
  int retval;
  uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
  
  fd_set rfds;
  fd_set current_rfds;
  int current_socket;
  int fdmax;
  
  
  daemonize();

  ctx = modbus_new_tcp(NULL, 1502);
  mb_mapping = modbus_mapping_new(200, 0, 200, 0);
  modbus_set_debug(ctx, FALSE);

  
  if (mb_mapping == NULL) {
    mylog(LOG_FILE,"Failed to allocate the mapping: %s\n", modbus_strerror(errno));
    modbus_free(ctx);
    return -1;
  }

  s = modbus_tcp_listen(ctx, 10);

  FD_ZERO(&rfds);
  FD_SET(s, &rfds);  
  fdmax = s;

  oldvalbitOTB = newvalbitOTB;
  oldvalbitIN0 = newvalbitIN0;

  sem_init(&sem_otb,0,2); // fino a 2 connessioni posso disporre verso otb
  sem_init(&sem_plc,0,2); // fino a 2 connessioni posso disporre verso plc
    
  for(;;) {
    
    current_rfds = rfds;
    
    retval = select(fdmax+1,&current_rfds,NULL,NULL,NULL);
    if (retval == -1) {
      if (errno == EINTR) {
	continue;
      } else {
	char msg[50];
	sprintf(msg,"Err: select() failure %i\n",errno);
	logvalue(LOG_FILE,msg);
	exit(-1);
      }
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

	  if ( modbus_reply(ctx, query, rc, mb_mapping) == -1 ) {
	    fprintf(stderr,"\tErr: %s\n",modbus_strerror(errno));
	  }
	  
	  /////////////////////////////////////////////////
	  int offset = modbus_get_header_length(ctx);
	  /***** Estraggo il codice richiesta *****/
	  uint16_t reg_address = (query[offset + 1]<< 8) + query[offset + 2];		  
	  char msg[100];	  
	  // sprintf(msg,"\tOffset: %i\n",query[offset]);
	  // logvalue(LOG_FILE,msg);
	  switch (query[offset]) {
	  case 0x05: { /* il PLC sta chiedendo di scrivere BITS (non concepita per il TWIDO) */
	    logvalue(LOG_FILE,"\til PLC sta chiedendo di scrivere BITS\n");
	    sprintf(msg,"\tCoil Registro %d-->%s [0x%02X]\n",
		    reg_address,
		    (mb_mapping->tab_bits[reg_address])?"ON":"OFF",
		    query[offset]);
	    logvalue(LOG_FILE,msg);
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
	    newvalbitOTB=mb_mapping->tab_registers[OTBDIN];
	    newvalbitIN0=mb_mapping->tab_registers[IN0];

	    gestioneOTB(newvalbitOTB,oldvalbitOTB); // gestione input OTB
	    gestioneIN0(newvalbitIN0,oldvalbitIN0);	    

	    oldvalbitOTB=newvalbitOTB;
	    oldvalbitIN0=newvalbitIN0;
	    /***********/
	  }
	    break;
	  } // switch
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
