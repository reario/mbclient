




#define RUNNING_DIR     "/home/reario/mbclient/"
#define LOCK_FILE       "/home/reario/mbclient/mbclient.lock"
#define LOG_FILE        "/home/reario/mbclient/mbclient.log"

#define CHECK_BIT(var,pos) ((var) & (1<<(pos)))
/*---------------------*/

/* NUMERI DELLE BOBINE SUL PLC DEI PULSANTI COMANDABILI DA ESTERNO: MAGELIS, INTERFACCIA A CARATTERI */
#define LUCI_ESTERNE_SOTTO 2 /* %M2 */
#define LUCI_CUN_LUN 3 /* %M3 */
#define LUCI_CUN_COR 4 /* %M4 */
#define LUCI_TAVERNA 5 /* %M5 */
#define LUCI_GARAGE 6 /* %M6 */
#define LUCI_STUDIO_SOTTO 7 /* %M7 */
#define LUCI_ANDRONE_SCALE 8 /* %M8 */
#define LUCI_CANTINETTA 10 /* %M10 interruttore */
#define SERRATURA_PORTONE 12 /* %M12 */
#define APERTURA_PARZIALE 96 /* %M96 */
#define APERTURA_TOTALE 97 /* %M97 */
#define CICALINO_AUTOCLAVE 60 /* %M60 */
#define CICALINO_POMPA_POZZO 61 /* %M61 */

/*Significato dei singoli bit dentro registro delle USCITE del PLC 
#define  LUCI_ESTERNE_SOTTO 2
#define  LUCI_CUN_LUN 3 
#define  LUCI_CUN_COR 4 
#define  LUCI_GARAGE 5 
#define  LUCI_TAVERNA 6
#define  LUCI_STUDIO_SOTTO 7
#define  LUCI_ANDRONE_SCALE 8
#define  LUCI_CANTINETTA 10 
*/



/***************/
/* MATRICE PLC */
/***************/
/* numeri dei registri I/O dove il PLC da remoto scrive i dati del PLC*/
#define I0_15 65 /* word che contiene lo stato degli input 0-16*/
#define I16_31 66 /* word che contiene lo stato degli input 16-32 */
#define ITM2 80 /* word che contiene lo stato degli ingressi del modulo di espansione del PLC */
#define Q0_32 67 /* word che contiene lo stato degli output 0-32*/

/* Significato dei singoli bit dentro il registro degli INPUT del PLC 
#define AUTOCLAVE 0 // registro 65
#define POMPA_SOMMERSA 1 // registro 65
#define RIEMPIMENTO 2 // registro 65
#define LUCI_ESTERNE_SOTTO 3 // registro 65
#define CENTR_R8 4// registro 65
#define LUCI_GARAGE_DA_4 5 // registro 65
#define LUCI_GARAGE_DA_2 6// registro 65
#define LUCI_TAVERNA_1_di_2 7 // registro 65
#define LUCI_TAVERNA_2_di_2 8 // registro 65
#define INTERNET 9 // registro 65
#define C9912 10 // registro 65
#define LUCI_CUN_LUN 11 // registro 65
#define LUCI_CUN_COR 12 // registro 65
#define LUCI_STUDIO_SOTTO 13 // registro 65
#define LUCI_ANDRONE_SCALE 14 // registro 65 
#define GENERALE_AUTOCLAVE 15 // registro 65
#define LUCI_CANTINETTA 16 // registro 66
*/

/***************/
/* MATRICE OTB */
/***************/
const char *otbdigitalinputs[] =  {"In0","In1","In2","In3","In4","In5","In6","In7","Apertura Parziale","Apertura Totale","Fari Esteni Sotto","Fari Esterni Sopra"};
const char *otbdigitaloutputs[] = {"Fari Esterni Sopra","Fari Esterni Sotto","Out2","Out3","Out4","Out5","Out6","out7"};
#define otbdigitalinputregister 74

const uint16_t reverseBitMask[] = {32768,16384,8192,4096,2048,1024,512,256,128,64,32,16,8,4,2,1};



/*
https://modbus.control.com/thread/1308248409#1308248409

reverseBitMask:array[1..16] of integer = (32768,16384,8192,4096,2048,1024,512,256,128,64,32,16,8,4,2,1);

For the 'and mask' you would send the 'not' of the mask for the bit to change.
x:=not reverseBitMask[bitNumber];// andMask 

For the 'or mask' you would either send the normal mask for the bit if setting the bit and the 'not' of the mask if clearing the bit.
if 0->1 then
  x:=reverseBitMask[bitNumber]                 //set bit, orMask
 else 
  x:=not reverseBitMask[bitNumber];    //clear bit, orMask

In the device receiving the command:     
 holdingRegister[registerAddress]:=(holdingRegister[registerAddress] and andMask) or   (orMask and not andMask);

 */


/* numeri dei registri I/O dove il PLC da remoto scrive i dati dell'OTB */
#define OTBDIN 74  /* word che contiene lo stato degli input 0-7 dell'OTB*/
#define OTBAIN1 75 /* word che contiene lo stato dell'input analogico 1 dell'OTB (BAR AUTOCLAVE) */
#define OTBAIN2 76 /* word che contiene lo stato dell'input analogico 3 dell'OTB (BAR POZZO) */
#define OTBDOUT 77 /* word che contiene lo stato degli output 0-7 dell'OTB */
#define OTBAOUT1 78 /* word che contiene lo stato dell'output analogico 1 dell'OTB */
#define OTBAOUT2 79 /* word che contiene lo stato dell'output analogico 2 dell'OTB */
/* REGISTRO INGRESSI OTB (OTN_IN): significato dei singoli BIT del registro n 74 (registro di 16 bit) */
#define FARI_ESTERNI_IN_SOPRA 11 /* 11-esimo bit dell'ingresso dell'OTB IN11*/
#define FARI_ESTERNI_IN_SOTTO 10 /* 10-esimo bit dell'ingresso dell'OTB IN10*/
#define OTB_IN9 9 /* 9-esimo bit dell'ingresso dell'OTB IN09 */
#define OTB_IN8 8 /* 8-esimo bit dell'ingresso dell'OTB IN08 */
#define OTB_IN7 7 /* 7-esimo bit dell'ingresso dell'OTB IN07 */
#define OTB_IN6 6 /* 6-esimo bit dell'ingresso dell'OTB IN06 */
#define OTB_IN5 5 /* 5-esimo bit dell'ingresso dell'OTB IN05 */
#define OTB_IN4 4 /* 4-esimo bit dell'ingresso dell'OTB IN04 */
#define OTB_IN3 3 /* 3-esimo bit dell'ingresso dell'OTB IN03 */
#define OTB_IN2 2 /* 2-esimo bit dell'ingresso dell'OTB IN02 */
#define OTB_IN1 1 /* 1-esimo bit dell'ingresso dell'OTB IN01 */
#define OTB_IN0 0 /* 0-esimo bit dell'ingresso dell'OTB IN00 */
/* REGISTRO USCITE OTB: significato dei singoli bit del registro (registro di 16 bit) */ 
#define FARI_ESTERNI_SOPRA 0 /* 0-esimo bit dell'uscita dell'OTB Q0 */
#define FARI_ESTERNI_SOTTO 1 /* 1-esimo bit dell'uscita dell'OTB Q1 */
#define OTB_Q2 2 /* 2-esimo bit dell'uscita dell'OTB Q2 */
#define OTB_Q3 3 /* 3-esimo bit dell'uscita dell'OTB Q3 */
#define OTB_Q4 4 /* 4-esimo bit dell'uscita dell'OTB Q4 */
#define OTB_Q5 5 /* 5-esimo bit dell'uscita dell'OTB Q5 */
#define OTB_Q6 6 /* 6-esimo bit dell'uscita dell'OTB Q6 */
#define OTB_Q7 7 /* 7-esimo bit dell'uscita dell'OTB Q7 */

/*========================================*/
