#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <signal.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
char *LOG_FILE = "/home/reario/mbclient/t.log";

sem_t sem_otb;
sem_t sem_plc;

#define LOCK(se) while ( ( s = sem_wait((se))) == -1 && errno == EINTR ) continue
#define UNLOCK(se) sem_post((se))

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

void buttonRelease(int sig, siginfo_t *si, void *uc) {

  char msg[50];
  uint8_t B = si->si_value.sival_int;

  logvalue(LOG_FILE,"SIGALRM\n");
  sprintf(msg,"\tBobina %u\n",B);
  logvalue(LOG_FILE,msg);  
}

void signal_handler(int sig, siginfo_t *si, void *uc)
{
  char msg[50];
  uint8_t B = si->si_value.sival_int;
  switch(sig) {

  case SIGHUP:
    // ----------------------------------
    logvalue(LOG_FILE,"SIGHUP\n");
    break;
    // ---------------------------------
    
  case SIGTERM:
    logvalue(LOG_FILE,"SIGTERM\n");
    break;

  case SIGALRM:
    logvalue(LOG_FILE,"SIGALRM\n");
    B==1?sprintf(msg,"\tBobina %u\n",B):sprintf(msg,"\tBobina %u\n---------\n",B);
    logvalue(LOG_FILE,msg);
    break;

  case SIGUSR1:
    logvalue(LOG_FILE,"\tcatched SIGUSR1 trying to unlock semaphore sem_otb\n");
    //sem_post(&sem_otb); // unlock
    UNLOCK(&sem_otb);
    break;
  }
}

static int makeTimer( uint8_t BOBINA, int expireMS,
		      int intervalMS )
{
  struct sigevent te;
  struct itimerspec its;
  struct sigaction sa;
  timer_t timerID;
  int sigNo = SIGUSR1;
  
  /* Set up signal handler. */
  sa.sa_flags = SA_SIGINFO;
  sa.sa_sigaction = signal_handler;
  sigemptyset(&sa.sa_mask);
  if (sigaction(sigNo, &sa, NULL) == -1) {
    perror("sigaction");
  }
  
  te.sigev_notify = SIGEV_SIGNAL;
  te.sigev_signo = sigNo;
  te.sigev_value.sival_ptr = &timerID;
  te.sigev_value.sival_int = BOBINA;

  timer_create(CLOCK_REALTIME, &te, &timerID);
  
  its.it_interval.tv_sec = 0;
  its.it_interval.tv_nsec = intervalMS * 1000000;

  its.it_value.tv_sec = 5;
  its.it_value.tv_nsec = expireMS * 1000000;
  timer_settime(timerID, 0, &its, NULL);
  
  return 1;
}

int main() {

  sem_init(&sem_otb, 0, 2);
  sem_init(&sem_plc, 0, 2);
  
  sem_wait(&sem_otb); // == -1 && errno == EINTR) continue;
  sem_wait(&sem_otb); //== -1 && errno == EINTR) continue;
  
  logvalue(LOG_FILE,"Start program....\n");
  makeTimer(2,0,0);
  int s;
  //while ( (s = sem_wait(&sem_otb) ) == -1 && errno == EINTR) continue;
  LOCK(&sem_otb);
  if(s == 0) {
    logvalue(LOG_FILE,"\tnot interrupted\n");
    logvalue(LOG_FILE,"unlocked otb\n");  
      } else {
    logvalue(LOG_FILE,"\tinterrupted on wait server sem\n");
  }
  
    
  return 0;
  
#ifdef PLUTO
  makeTimer(1,900,0); 
  makeTimer(2,500,0);
  
  while(1) sleep(10);  
  //-----------------------------
  struct sigaction act;  
  memset (&act, '\0', sizeof(act));
  /* inizializzo la struttura sigaction */
  act.sa_handler = &signal_handler;  
  if (sigaction(SIGALRM, &act, NULL) < 0) {
    perror ("sigaction");
    return 1;
  }
  //-----------------------------
  logvalue(LOG_FILE,"Start program....\n");
  //-----------------------------
  struct itimerval tv;
  tv.it_interval.tv_sec = 3;
  tv.it_interval.tv_usec = 0;  // when timer expires, reset to 100ms
  tv.it_value.tv_sec = 3;
  tv.it_value.tv_usec = 0; //100000; //500000;   // 100 ms == 100000 us+
  setitimer(ITIMER_REAL, &tv, NULL);
  //-----------------------------

  logvalue(LOG_FILE,"1..\n");
  logvalue(LOG_FILE,"2..\n");
#endif
  
#ifdef PIPPO
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
#endif

}
