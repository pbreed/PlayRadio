#include "predef.h" 
#include <stdio.h>
#include <ctype.h>
#include <startnet.h>
#include <autoupdate.h>
#include <dhcpclient.h>
#include <smarttrap.h>  //Comment
#include <taskmon.h>
#include <sim.h>
#include <cfinter.h>
#include <tcp.h>
#include <string.h>
#include <stdlib.h>
#include <dns.h>
#include <effs_fat/cfc_mcf.h>
#include <ethernet.h>
#include <pins.h>
#include "FileSystemUtils.h"

extern const unsigned long WavLen; 
extern const unsigned char WavData[];

bool bLog;							 
volatile WORD LedState;	
volatile DWORD LastSqueltchTime;

bool bSqueltchActive();

#define LED1_RED (1)
#define LED1_GREEN (2)
#define LED2_RED (4)
#define LED2_GREEN (8)


#define LED_BLINK(x) ((x)<<4)

#define LED_NO_ETHERNET        LED_BLINK((LED1_RED|LED2_RED)) //Blinking Red Red  ->No Ethernet Link.
#define LED_NO_ADDRESS         LED_BLINK(LED1_RED) //Blinking Red <off> ->No DHCP Server 
#define LED_NO_GATEWAY	       LED_BLINK(LED2_RED) //Blinking <off> Red -> Don't have or can't reach Gateway 
#define LED_GATEWAY_WRONG   LED_BLINK(LED2_RED) //Blinking <off> Red -> Don't have or can't reach Gateway 
#define LED_NO_DNS_ADDR        (LED1_RED|LED2_RED) //Steady Red Red    ->Don't have a DNS address  
#define LED_NO_DNS_RESOLVE     (LED1_RED)       //Steady  Red <off> -> Can't resolve name.      
#define LED_NO_SERVERRESPONSE  (LED2_RED)    //Steady <off> Red ->No response From Server.   
#define LED_NO_SERVER_CONNECT  (LED2_RED)    //Steady <off> Red ->No response From Server.   

#define LED_NULL_RESULT (LED1_GREEN)                        //Green  <off>  Got null result from Server No Squeltch Detected                   
#define LED_TXING       (LED1_GREEN |LED2_RED)              //Green Red Transmitting                                                           
#define LED_WAITING (LED_BLINK(LED1_GREEN)|LED2_GREEN)      //Blink Green Steady Green  ->Got from server result waiting for Squelch to clear. 
#define LED_NULL_SQUELTCH (LED1_GREEN|LED_BLINK(LED2_GREEN))//   Steady Green, Blinking Green Got null result from Server Squeltch detected.      

                                     
void LedTask(void * p)
{
int n=0;
while(1)
 {
  OSTimeDly(TICKS_PER_SECOND /2);
  
  if(bSqueltchActive()) 
  {
	if (LedState==LED_NULL_RESULT)  LedState=LED_NULL_SQUELTCH;
  }
  else
  {
   if (LedState==LED_NULL_SQUELTCH)  LedState=LED_NULL_RESULT;
  }

 
  if(n&1)
   {
	  putleds(((LedState>>4)| LedState)&0x0F);
   }
  else
  {
	  putleds(LedState&0x0F);
  }
  n++;
 }
}



#define AD_CH (0)





















extern "C" { 
void UserMain(void * pd);
void ShowStatus( int sock, PCSTR url );
void SetIntc( long func, int vector, int level, int prio );

}


const char * AppName="PlayRadio";

volatile DWORD pitr_count;

bool bKey;


OS_SEM EightKSem;

INTERRUPT( my_pitr_func, 0x2600 )
{
WORD tmp = sim.pit[1].pcsr;
tmp &= 0xFF0F; // Bits 47 cleared
tmp |= 0x0F; // Bits 03   set
sim.pit[1].pcsr = tmp;
OSSemPost(&EightKSem);
}


//
void SetUpPITR( int pitr_ch, WORD clock_interval, BYTE pcsr_pre /* See
table 213
in the reference manual for bits 811
*/ )
{
WORD tmp;
if ( ( pitr_ch < 1 ) || ( pitr_ch > 3 ) )
{
iprintf( "*** ERROR PIT channel out of range ***\r\n" );
return;
}
//
// Populate the interrupt vector in the interrupt controller. The
// SetIntc() function is supplied by the NetBurner API to make the
// interrupt control register configuration easier
//
SetIntc( ( long ) &my_pitr_func, 36 + pitr_ch, 2 /* IRQ2 */, 3 );
//
// Configure the PIT for the specified time values
//
sim.pit[pitr_ch].pmr = clock_interval; // Set PIT modulus value
tmp = pcsr_pre;
tmp = ( tmp << 8 ) | 0x0F;
sim.pit[pitr_ch].pcsr = tmp; // Set system clock
}





WORD OutBuffer[65536];
WORD OutPut;
WORD OutGet;


volatile DWORD icnt;
volatile int nState;
volatile BYTE nWm;
volatile BYTE nWl;

volatile const unsigned char * pData;
volatile int datalen;
int send_len;
volatile DWORD dir_cnt;
volatile DWORD out_cnt;


#define BASEADDRESS 0xA0000000
#define BASEADDRESS2 (BASEADDRESS+0x20000)

volatile unsigned char* pDacClk; 
volatile unsigned char* pLoadClk; 



#define DACCSPIN Pins[28]


void TxKey(bool bk)
{
if(bk) 
{
Pins[36]=1;
iprintf("Transmit on");
}
else 
{
 Pins[36]=0;
 iprintf("Transmit off");
}
}


void DacWrite(BYTE v)
{
WORD w=0x3000 | (v<<4);
DACCSPIN=0;
asm(" nop");
asm(" nop");

for(int i=0; i<16; i++)
{
	if(w & 0x8000) 
		sim.gpio.ppdsdr_timer = 0x08; 
	else
		sim.gpio.pclrr_timer = ~0x08; 
	asm(" nop");
	*pDacClk=0;
    w=w<<1;
}

asm(" nop");
DACCSPIN=1;
asm(" nop");
*pLoadClk=1;

}

void MyInit()
{

	sim.cs[2].csar = ( BASEADDRESS >> 16 );
	sim.cs[2].cscr = 0x0140; /* 0000 0001 0110  0000 */
	sim.cs[2].csmr = 0x00000001;
	pDacClk = ( PBYTE )BASEADDRESS;

	sim.cs[3].csar = ( BASEADDRESS2 >> 16 );
	sim.cs[3].cscr = 0x0140; /* 0000 0001 0110  0000 */
	sim.cs[3].csmr = 0x00000001;
	
	pLoadClk = ( PBYTE )BASEADDRESS2;


	Pins[36].function(PIN36_GPIO );
    Pins[36]=0;
    Pins[27].function(PIN27_GPIO );
    Pins[27]=0;

	Pins[29].function(PIN29_GPIO);
	Pins[29].read();



	DACCSPIN.function(0);
	DACCSPIN=1;



}

volatile DWORD SqCount;


void EightKTask(void * p)
{
static bool bLastKey;

OSSemInit(&EightKSem,0);
SetUpPITR( 1 /* Use PIT1 */, 4607 /* Wait 4607 clocks */, 1 );
send_len=0;

MyInit();

bLastKey=0;
datalen=0;
send_len=0;
nWl=0;
nWm=0x80;
DacWrite(0x80);
bKey=false;
 

while(1)
{
 OSSemPend(&EightKSem,0);
  
 if(sim.eport.eppdr & 0x02 ) 
  {
   SqCount++;
   if(SqCount>5)		   //Bi polar D00 to FFF is -0 to -0x200  and 0->200
   {
	LastSqueltchTime=TimeTick;
	SqCount=0;
   }
 }

 if(datalen)
   {
	nWl=pData[send_len++];
	nWm=(pData[send_len++]+0x80)&0xFF;				
	if(send_len>=datalen) 
		{datalen=0;
		 send_len=0;
		 nWl=0;
		 nWm=0x80;
		 bKey=false;
		 TxKey(false);
		}
    DacWrite(nWm);
  }
  
  if(bKey!=bLastKey)
  {
   bLastKey=bKey;
   TxKey(bKey);
  }
}

}


const unsigned char *FindWav(const unsigned char * data, int & len)
{
for(int i=0; i<256; i++)
{
 if((data[i]=='d') && (data[i+1]=='a') && (data[i+2]=='t') && (data[i+3]=='a'))
  {
  BYTE ba[4];
  ba[3]=data[i+4]; 
  ba[2]=data[i+5]; 
  ba[1]=data[i+6]; 
  ba[0]=data[i+7]; 
  len=*((int *)ba);
  return data+i+8;
  }
}
len=0;
return NULL;
}


void SendWav(const unsigned char * pWavData, int len)
{
bKey=true;
OSTimeDly(TICKS_PER_SECOND);
OSLock();
pData=(volatile const unsigned char *)pWavData;
datalen=len;
send_len=0;
OSUnlock();
// bKey=false;
}


#define SCRATCHSIZE (1500000)
unsigned char ScratchBuffer[SCRATCHSIZE];
int nScratchLen;




class ConfigObject
{
static ConfigObject * pHead;
ConfigObject * m_pNext;
const char * m_tag;
char m_buffer[256];
int m_ival;
void OParseData(const char * pData);

public:

ConfigObject(const char * tag, const char * defaultv);
int ival() {return m_ival;}
WORD wval() {return (WORD) m_ival; };
WORD tval() {return (WORD) m_ival*TICKS_PER_SECOND; };

const char * string() {return m_buffer; };
static void ParseData(const char * pData);
};

ConfigObject * ConfigObject::pHead;

ConfigObject::ConfigObject(const char * tag, const char * defaultv)
{
m_pNext=pHead;
pHead=this;
strncpy(m_buffer,defaultv,255);
m_tag=tag;
m_ival=atoi(m_buffer);
}


void ConfigObject::OParseData(const char * pData)
{
const char * p=strstr(pData,m_tag);
if(p)
{
 while((*p) && (*p!=',')) p++;
 while((*p) && (*p==',')) p++;
 while((*p) && (isspace(*p))) p++;
 char * p2=m_buffer;
 while((*p) && (*p!='\r') && (!isspace(*p))) *p2++=*p++;
 *p2=0;
 m_ival=atoi(m_buffer);
 iprintf("Found Config %s=[%s]\r\n",m_tag,m_buffer);

}
}


void ConfigObject::ParseData(const char * pData)
{
ConfigObject * pCur=ConfigObject::pHead;
while(pCur)
 {
  pCur->OParseData(pData);
  pCur=pCur->m_pNext;
 }
}



/*
URL,http://www.servicegem.com/radiodispatch.ashx
DEVICENAME,StoreNameGoesHere
OUTBOUNDPORT,80
SLEEPINTERVAL,5 
REQUESTTIMEOUT,20 

*/

ConfigObject SLEEP_INTERVAL("SLEEPINTERVAL","5");
ConfigObject OUTBOUND_PORT("OUTBOUNDPORT","80");
ConfigObject REQUEST_TIMEOUT("REQUESTTIMEOUT","10");
ConfigObject URL ("URL","http://www.servicegem.com/radiodispatch.ashx");
ConfigObject DEVICENAME ("DEVICENAME","STORENAMEGOESHERE");
ConfigObject SQUELTCH_TIMEOUT("SQUELTCHTIMEOUT","5");





bool bSqueltchActive()
{
if ((LastSqueltchTime+TICKS_PER_SECOND)<TimeTick) return false;
return true;
}




#define RESPONSE_SIZE (5000)

static int nRequests,nResponses,nFails;





void TcpWebTask(void * p)
{

f_enterFS(); 
InitExtFlash();  // Initialize the CFC or SD/MMC external flash drive
	
DWORD result=ReadFile( ScratchBuffer,(const char *)"config.txt",SCRATCHSIZE );

if(result>0)
{
 ScratchBuffer[result]=0;
 iprintf("REad %d bytes of config file\r\n",result);
 ConfigObject::ParseData((const char *)ScratchBuffer);
}

	
static char tbuffer[256];
static char tRequest[256];
static char tResponse[RESPONSE_SIZE];

strncpy(tbuffer,URL.string(),255);
tbuffer[255]=0;

char * pAddress;
char * pUrl;

pAddress=tbuffer;

while((*pAddress) && isspace(*pAddress)) pAddress++;

if ((*pAddress=='H') || (*pAddress=='h'))
{
 while((*pAddress) && (*pAddress!='/')) pAddress++;
 while((*pAddress) && (*pAddress=='/')) pAddress++;
}

pUrl=pAddress;

while((*pUrl) && (*pUrl!='/')) pUrl++;

if(*pUrl=='/') {*pUrl=0; pUrl++; };



iprintf("Web address = [%s] pUrl=[%s]\r\n",pAddress,pUrl);
IPADDR ip=0;

while(1)
{
 while (ip==0) 
 {
  while(!EtherLink()) { iprintf("No Ethernet Link\r\n"); LedState=LED_NO_ETHERNET; OSTimeDly(TICKS_PER_SECOND);};
  while(EthernetIP==0) { iprintf("No Ethernet Address\r\n"); LedState=LED_NO_ADDRESS; OSTimeDly(TICKS_PER_SECOND);};
  while(EthernetIpGate==0) { iprintf("No Ethernet Gateway Address\r\n"); LedState=LED_NO_GATEWAY; OSTimeDly(TICKS_PER_SECOND);};
  while((EthernetIpGate&EthernetIpMask) !=(EthernetIpMask&EthernetIP )) { iprintf("Gateway Address Wrong Subnet\r\n"); LedState=LED_GATEWAY_WRONG; OSTimeDly(TICKS_PER_SECOND);};
  while(EthernetDNS==0) { iprintf("No Ethernet DNS Address\r\n"); LedState=LED_NO_DNS_ADDR; OSTimeDly(TICKS_PER_SECOND);};
  

  if(GetHostByName(pAddress,&ip,0,SLEEP_INTERVAL.tval())==DNS_OK)
  {
   iprintf("IP Address ="); ShowIP(ip);
   siprintf(tRequest,"GET /%s?%s HTTP/1.1\r\nHost:%s\r\n\r\n",pUrl,DEVICENAME.string(),pAddress);
   iprintf("Request =[%s]\r\n",tRequest);
  }
  else
  {
	LedState=LED_NO_DNS_RESOLVE;
	iprintf("Failed to get DNS address\r\n");
  }
 }
 DWORD LastStartTime=TimeTick;
 DWORD LastConTime=0;
 DWORD LastReadDoneTime=0;
 
 int fd=connect(ip,0,OUTBOUND_PORT.wval(),SLEEP_INTERVAL.tval());
 if(fd<0)
 {
	 LedState=LED_NO_SERVER_CONNECT;
 	 ip=0;
  }
 else
 {
  
  LastConTime=TimeTick;
  writestring(fd,tRequest);
  int nread=0;
  while(1)
  {
   int rv=ReadWithTimeout(fd,tResponse+nread,RESPONSE_SIZE-nread,REQUEST_TIMEOUT.tval());
if(bLog)   iprintf("Read at %ld\r\n",TimeTick-LastStartTime);
   if(rv>0) 
	   {nread+=rv;
	   if(nread>=(RESPONSE_SIZE-1)) break;
	   tResponse[nread]=0;
       }
    else
	break;
  };

  LastReadDoneTime=TimeTick;

 if(nread)
 {
	 nRequests++;
  char * FindResult= strstr(tResponse,"\r\n\r\n");
  if(FindResult) 
  {
	 
	 FindResult+=4;
	 if(FindResult[0])
	  {
		char * p=FindResult;
		while (*p) p++;
		*p++='.';
		*p++='w';
		*p++='a';
		*p++='v';
		*p++=0;
       //iprintf("Looking for file %s\r\n",FindResult);

		nResponses++;
      DWORD result=ReadFile( ScratchBuffer,FindResult,SCRATCHSIZE );
	  
	  if(result>50)
	  {
		  int len;
		  const unsigned char * pWav=FindWav(ScratchBuffer,len); 
		  if(pWav)
		  {
		  while(bSqueltchActive())
		  {
			LedState=LED_WAITING;
			iprintf("Waiting for Squeltch %04X %ld %ld\r\n",SqCount,LastSqueltchTime,TimeTick);
			OSTimeDly(2);
		  }

			  LedState=LED_TXING;
			  SendWav(pWav,len);
		      iprintf("Sending File %s\r\n",FindResult);
			  while(datalen>0) OSTimeDly(2);
			  putleds(2);
		  }
	  }
	  else
	  {
		iprintf("Failed to find file %s\r\n",FindResult);
		nFails++;
	  }


	  }
	 else
	 {
//	 iprintf("Null for Squeltch %04X %ld %ld\r\n",LastADR,LastSqueltchTime,TimeTick);
		 if(bLog)
		 {
		 iprintf("\r\n Response:");
		 ShowData((PBYTE)tResponse,nread);
		 }
		 else
		 {
			iprintf("The server had nothing for me %d requests %d actions %d find failures)\r\n",nRequests,nResponses,nFails);
		 }
/*      	 if(nread>20)
		 {
			int n=0;
			while((tResponse[n]!='\r') && (n<20)) n++;
			tResponse[n]=0;
			iprintf("Null response[%s] [%s][%s]\r\n",tResponse,FindResult);
		 }
    	 else iprintf("Null response\r\n");
	*/
	  
    	  LedState=LED_NULL_RESULT;
	 }
  }
  else
  {
   LedState=LED_NO_SERVERRESPONSE;
   iprintf("Failed to find result\r\n");
  }

 }
 close(fd);
 if(bLog)
 {
  iprintf("\r\nS= %ld C = %ld   R= %ld ticks\r\n",LastStartTime,LastConTime,LastReadDoneTime);
  iprintf("\r\nTotal = %ld Connect = %ld   Read = %ld ticks\r\n",LastReadDoneTime-LastStartTime,LastConTime-LastStartTime,LastReadDoneTime-LastConTime);
  }
 }

 DWORD tl=LastStartTime+SLEEP_INTERVAL.tval();
 DWORD now=TimeTick;
 if ((now<tl) && ((tl-now)<0xFFFF)) OSTimeDly((WORD)(tl-now));
}

}


void ShowStatus( int sock, PCSTR url )
{
static char buffer[2048];
 siprintf(buffer,"LED State = %02X<BR> Last Squeltch %04X <BR>Off at :%ld <BR>Now:%ld\r\n",LedState,SqCount,LastSqueltchTime,TimeTick);
 writestring(sock,buffer); 
}




void UserMain(void * pd)
{
   InitializeStack();/* Setup the TCP/IP stack buffers etc.. */     

   GetDHCPAddressIfNecessary();/*Get a DHCP address if needed*/
   /*You may want to add a check for the return value from this function*/
   /*See the function definition in  \nburn\include\dhcpclient.h*/

   OSChangePrio(MAIN_PRIO);/* Change our priority from highest to something in the middle */

   EnableAutoUpdate();/* Enable the ability to update code over the network */ 

   EnableSmartTraps();/* Enable the smart reporting of traps and faults */

   EnableTaskMonitor();/*Enable the Task scan utility */


   iprintf("Application started\n");




//   OSTimeDly(20);
//   iprintf("About to start task 1\r\n");
/*   getchar();
   putleds(0); iprintf("0");
   getchar();
   putleds(1); iprintf("1");
   getchar();
   putleds(2); iprintf("2");
   getchar();
   putleds(4); iprintf("4");
   getchar();
   putleds(8); iprintf("8");
   getchar();
*/


   OSSimpleTaskCreatewName(EightKTask,2,"Eight K Task");
   
  // OSTimeDly(20); iprintf("About to start task 3\r\n");
   
   OSSimpleTaskCreatewName(TcpWebTask,MAIN_PRIO-2,"Tcp Server Task");
   
   //OSTimeDly(20); iprintf("About to start task 4\r\n");
   
   OSSimpleTaskCreatewName(LedTask,MAIN_PRIO-1,"Led Blink");
			  
   
   iprintf("After start task\r\n");

 /*
   while(1)
   {
    xLedState=LED1_RED; iprintf("Led 1 Red\r\n"); OSTimeDly(80);
    xLedState=LED2_RED; iprintf("Led 2 Red\r\n"); OSTimeDly(80);
    xLedState=LED1_GREEN; iprintf("Led 1 Green\r\n"); OSTimeDly(80);
    xLedState=LED2_GREEN; iprintf("Led 2 Green\r\n"); OSTimeDly(80);
    
	xLedState=LED_BLINK(LED1_RED); iprintf("Led 1 Blink Red\r\n"); OSTimeDly(80);
	xLedState=LED_BLINK(LED2_RED); iprintf("Led 2 Blink Red\r\n"); OSTimeDly(80);
	xLedState=LED_BLINK(LED1_GREEN); iprintf("Led 1 Blink Green\r\n"); OSTimeDly(80);
	xLedState=LED_BLINK(LED2_GREEN); iprintf("Led 2 Blink Green\r\n"); OSTimeDly(80);
	xLedState=LED_BLINK((LED1_RED|LED2_RED)); iprintf("Led both Blink Red\r\n"); OSTimeDly(80);
	xLedState=LED_BLINK((LED1_GREEN|LED2_GREEN)); iprintf("Led both Blink Green\r\n"); OSTimeDly(80);


   }

   */



   DWORD lc;
   DWORD pc=0;
   DWORD li;
   while (1)
  {
      OSTimeDly(TICKS_PER_SECOND * 1);
	  lc=pitr_count;
	  li=icnt;
	  if (bSqueltchActive())
		  {if(bLog)
		  iprintf("SqCount %04X  %ld %ld\r\n",SqCount,LastSqueltchTime,TimeTick);
		  else
		   iprintf("I hear people talking\r\n");
	      }
	  else
		  {
		  if(bLog)
		  {
          iprintf("No SQ    %04X  %ld %ld\r\n",SqCount,LastSqueltchTime,TimeTick);
		  }
		  else
		  iprintf("The Radio is quiet\r\n");
		}

	  //iprintf("Pitr count = %ld %ld delta = %ld  d=%ld o=%ld\r\n",lc,li,lc-pc,dir_cnt,out_cnt);
	  pc=lc;
	  if(charavail())
	  {
		  char c=getchar();
		  if(c=='L') bLog=true;
		  else

		  if(c=='l')bLog=false;
		  else
		  if(c=='P')
			  {
			  iprintf("Play..");
			  SendWav(WavData,WavLen);
			  iprintf("Done\r\n");
			  }
		  else
		if(c=='C')
		{

	  int len;
	  const unsigned char * pWav=FindWav((const unsigned char *)"aisle two.wav",len); 
	  if(pWav)
	  {
		  LedState=LED_TXING;
		  SendWav(pWav,len);
		  iprintf("Sending File\r\n");
		  while(datalen>0) OSTimeDly(2);
		  putleds(2);
	  }
	  else
	  iprintf("Did not find aisle two.wav");

		}


    	  else
			  iprintf("Commands are P (lay) L(og on) l(og off)\r\n");



	  }

  }
}
