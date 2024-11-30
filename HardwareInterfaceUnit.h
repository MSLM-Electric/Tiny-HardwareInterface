#ifndef HARDWARE_INTERFACE_UNIT_H
#define HARDWARE_INTERFACE_UNIT_H
#ifdef __cplusplus
extern "C" {
#endif // !_cplusplus
#include <stdint.h>
#include "type_def.h"
//#include "../../Lib/fileHandle.h"
//#include "../SimpleTimer/SimpleTimerWP.h"
#include "SimpleTimerWP.h"
//#include "MasterImmitationCfg.h"
//#include "SlaveImmitationCfg.h"

#ifdef DEBUG_ON_VS
HANDLE HardwareImmitMutex; //on real hardware mutex not needed
#endif

#define ONLY //just nothing. Only for clarifying ports state currently
#define no_required_now 0
#define RECV_BUFFER_SIZE 24
#define TRANSMIT_BUFFER_SIZE 24 

//#pragma region HARDWARE_PORT
//#define IN_CASE_OF_FIFO_TYPE
typedef struct {
	u8 TXInterruptEnable;
	u8 RXInterruptEnable;
	u32 someSettings; //Common Inits
	u8 BUFFER;
	u8 StartTX;
	u8 StartRX;
	u8 clearOrResetSomeFlags;
#ifdef IN_CASE_OF_FIFO_TYPE
	u8 FIFO_BUFFER[255];
	u8 clearFIFO;
#endif //!IN_CASE_OF_FIFO_TYPE
}HardwarePort_t;
#define MASTER_SLAVE_IMMIT_IN_1CORE
#if defined(DEBUG_ON_VS) && defined(MASTER_SLAVE_IMMIT_IN_1CORE)
enum {
	SLAVENO,
	MASTERNO
};
HardwarePort_t HWPortN[2];

HardwarePort_t HWPort;
#else
extern HardwarePort_t HWPort;
#endif //

//#pragma endregion

#ifdef IOFILE_PATH
char iofilePath[200];
#endif // IOFILE_PATH
#ifdef GLOB_MUTEX_FILE
char globMutexFile[200];
#endif //GLOB_MUTEX_FILE

typedef enum {
	PORT_CLEAR = 0,//PORT_OFF = 0, //,
	PORT_READY = 1, //mb PORT_OK
	PORT_BUSY = 1 << 1,      //mb not needed. instead it mb use only receiving flag
	PORT_SENDING = 1 << 2,
	PORT_SENDED = 1 << 3,
	PORT_SENDING_LAST_BYTE = 1 << 4,
	PORT_RECEIVING = 1 << 5, //mb not needed
	PORT_RECEIVED = 1 << 6,
	PORT_ASYNC = 1 << 7,     //if zero, then it is a PORT_SYNC.  PORT_ASYNC = RS-485/CAN like interfaces. //!Not implemented yet
	PORT_MASTER = 1 << 8,    //if zero, it is SLAVE (good approach//?)
	PORT_BUFFER_FIFO = 1 << 9, //if zero, it is simple 8bit buffer //PORT_BUFF_FIFO_ENABLED
	PORT_RECEIVED_ALL = 1 << 10,
	PORT_SENDED_ALL = 1 << 11,
	PORT_ERROR = 1 << 12,
	//PORT_USING_ON_BACKGND = 1 << 12, //?mb not needed!
}InterfacePortState_e;

#ifdef ENABLE_DELAYED_RECV
typedef int* (DelayedRecv_fn)(void* ifsPort, u16 maxPossibleLen);
typedef struct {
	DelayedRecv_fn* DelayedRecv;
	void* ifsArg;
	u16 maxLen;
}DelayedRecv_t;
#endif // !ENABLE_DELAYED_RECV

//#ifdef SPECLIBS
typedef Timert_t Timerwp_t;
//#endif // !SPECLIBS


typedef struct {
	uint8_t BufferRecved[RECV_BUFFER_SIZE];
	uint8_t BufferToSend[TRANSMIT_BUFFER_SIZE];
	uint16_t LenDataToSend;
	uint16_t LenDataToRecv; //mb maxPossibleDataRecv
	Timerwp_t ReceivingTimer;     //for port timeout
	Timerwp_t SendingTimer;       //this is for describing timeout
	uint16_t communicationPeriod; //!not demanded for slave //Timerwp_t
	//uin32_t TotalCommunicationPeriod; //!not demanded for slave
	InterfacePortState_e Status; //(int)
	u16 outCursor; //outPtr; mb outCursorPos; sendbufPos;
	u16 inCursor;  //inPtr;
	u16 errCnt;
#ifdef ENABLE_DELAYED_RECV
	DelayedRecv_t DelayedRecv;
#endif // !ENABLE_DELAYED_RECV
}InterfacePortHandle_t;

#if defined(DEBUG_ON_VS) && defined(CMSIS_OS_ENABLE)
#include "../../../../ExternalLibs/cmsis_os/cmsis_os.h"
extern osPoolId mpool;
extern osMessageQId MsgBox;
typedef struct {
	uint8_t BUFFER; //Hardware 8bit BUFFER Case
	InterfacePortHandle_t* Port;
}portsBuffer_t;
#include "../debug_print.h"
#define HARDWARE_INTERFACE_LOG(e, s) DEBUG_PRINTM(e,s)
#else
#define HARDWARE_INTERFACE_LOG(e, s)
#endif // !DEBUG_ON_VS && CMSIS_OS_ENABLE

//extern InterfacePortHandle_t InterfacePort; //InterfacePort[ALL_CHANNELS] //InterfacePort[PORT0];
int InitMasterPort(InterfacePortHandle_t* PortHandle);
int InitSlavePort(InterfacePortHandle_t* PortHandle);
int Write(InterfacePortHandle_t *PortHandle, const uint8_t *inDatas, const int size);
int Recv(InterfacePortHandle_t *PortHandle, uint8_t *outBuff, const int maxPossibleSize);
int SendingHandle(InterfacePortHandle_t* Port);
int SendingTimerHandle(InterfacePortHandle_t *Port);
int ReceivingHandle(InterfacePortHandle_t* Port);
int ReceivingTimerHandle(InterfacePortHandle_t* PortHandle);
void TransmitInterrupt(void* arg); //Call_TXInterrupt()
void ReceiveInterrupt(void* arg); //void ReceiveInterrupt(void* arg);
//int SentInterrupt(void); //End of transmit callback
int immitationReceivingOfPortsBus(InterfacePortHandle_t* outPortHandle);
#ifdef GLOB_MUTEX_FILE
FRESULT TakeGLOBMutex(/*char* tempBuffer, const size_t maxPossibleLen,*/ FIL* f, uint32_t timeOut);
FRESULT RealeaseGLOBMutex(FIL* f);
#endif // !GLOB_MUTEX_FILE
#define ACCUMUL_CAPACITY 200
typedef struct {
	Timerwp_t TraceTime; //TraceTimeToAccum
	Timerwp_t FrequencyTrace;
	u16 accumulatedStats[ACCUMUL_CAPACITY];
	u16 accumArrayPos;
}tracePortCfg_t;
//tracePortCfg_t PortTracer;
void tracePortInit(tracePortCfg_t* traceP);
void tracePort(InterfacePortHandle_t* Port, tracePortCfg_t* traceP);
void ShowTracedAccumulations(tracePortCfg_t* traceP);
u16 BitPos(u16 Bit);

#ifdef __cplusplus
}
#endif // !_cplusplus

#endif // !HARDWARE_INTERFACE_UNIT_H