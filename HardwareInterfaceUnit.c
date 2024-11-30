#ifdef __cplusplus
extern "C" {
#endif // !_cplusplus

#include "HardwareInterfaceUnit.h"
//#include "PortsBusMessages.h"
//#include "../../MultiThreadSupport.h"
#ifdef HANDLING_WITH_IOFILE
#elif defined(IOSOCKET_HANDLE)
#include "../IO_immitationBetweenMasterSlave/iosocket.h"
//if MASTER
extern SOCKET ConnectSocket;
//else SLAVE
extern SOCKET ListenSocket;
extern HANDLE SocketMutex;
#endif // HANDLING_WITH_IOFILE

#if defined(DEBUG_ON_VS) && defined(CMSIS_OS_ENABLE)
osPoolId mpool;
osMessageQId MsgBox;
#else
HardwarePort_t HWPort; //!?
#endif // !DEBUG_ON_VS && CMSIS_OS_ENABLE

#include "usart.h"

static int PortSend(InterfacePortHandle_t* PortHandle, uint8_t BUFFER);
static void ErrorPortSendingHandle(InterfacePortHandle_t *Port);

//char mastersMessageId[] = MASTER_MESSAGE_ID;
//char slavesMessageId[] = SLAVE_MESSAGE_ID;
#ifndef MINIMAL_CODESIZE
static stopwatchwp_t timeMeasure[10];
#endif // !MINIMAL_CODESIZE

static int InitPort(InterfacePortHandle_t* PortHandle)
{
	int res = 0;
#ifndef MINIMAL_CODESIZE
	InitStopWatchGroup(timeMeasure, (tickptr_fn*)GetTickCount, sizeof(timeMeasure) / sizeof(stopwatchwp_t));
	InitTimerWP(&PortHandle->ReceivingTimer, (tickptr_fn*)GetTickCount);
	InitTimerWP(&PortHandle->SendingTimer, (tickptr_fn*)GetTickCount);
#else
	InitTimerWP(&PortHandle->ReceivingTimer, NULL);
	InitTimerWP(&PortHandle->SendingTimer, NULL);
#endif // !MINIMAL_CODESIZE
	PortHandle->Status = 0;
	PortHandle->ReceivingTimer.setVal = (U32_ms)50;
	PortHandle->SendingTimer.setVal = (U32_ms)400;
	return res;
}

int InitMasterPort(InterfacePortHandle_t* PortHandle)
{
	int res = 0;
	res = InitPort(PortHandle);
	PortHandle->Status |= PORT_MASTER;
	return res;
}

int InitSlavePort(InterfacePortHandle_t* PortHandle)
{
	int res = 0;
	res = InitPort(PortHandle);
	return res;
}

//#ifdef IN_CASE_OF_FIFO_TYPE
//#define IN_CASE_OF_FIFO_TYPE
int Write(InterfacePortHandle_t* PortHandle, const uint8_t *inDatas, const int size)
{
	int res = -1;
	u8 IsSendingTimerRinging = IsTimerWPRinging(&PortHandle->SendingTimer);
	if ((PortHandle->Status & (PORT_READY | PORT_SENDING | PORT_BUSY)) == ONLY PORT_READY) {
		memcpy(PortHandle->BufferToSend, inDatas, size);
		PortHandle->LenDataToSend = size;
		PortHandle->outCursor = 0;
		//PortHandle->outCursor++;
#ifdef IN_CASE_OF_FIFO_TYPE
		memcpy(HWPort.FIFO_BUFFER, PortHandle->BufferToSend, PortHandle->LenDataToSend);
#endif // !IN_CASE_OF_FIFO_TYPE
		/*The real hardware interface peripheral settings*/
		HWPort.someSettings = 0xFF;
		HWPort.TXInterruptEnable = 1;
		HWPort.StartRX = 0;
		HWPort.StartTX = 1;
		USART_ReceiveINTDisable();
		USART_TransmitINTEnable();
		/*^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^*/
		PortHandle->Status |= PORT_BUSY;
		PortHandle->Status |= PORT_SENDING;
		PortHandle->Status clearBITS(PORT_SENDED | PORT_SENDED_ALL | PORT_RECEIVED | PORT_RECEIVING | PORT_RECEIVED_ALL/*//?mb all not needed here*/);
		LaunchTimerWP(PortHandle->SendingTimer.setVal, &PortHandle->SendingTimer);
		/*Simulation part*/
		//res = immitationOfPortsBus(PortHandle);
		u8 BUFFER = PortHandle->BufferToSend[PortHandle->outCursor];
		res = PortSend(PortHandle, BUFFER);
		if (res < 0) {
			DEBUG_PRINTF(1, ("Port sending ERROR!\n"));
			ErrorPortSendingHandle(PortHandle);
		}
		else {
			;;
		}
	}
	else if ((PortHandle->Status & (PORT_BUSY | PORT_SENDING | PORT_SENDING_LAST_BYTE)) == ONLY (PORT_BUSY | PORT_SENDING | PORT_SENDING_LAST_BYTE)) {
		/*Real hardware interface peripheral settings*/
		HWPort.clearOrResetSomeFlags = 0;
		HWPort.TXInterruptEnable = 0;
		HWPort.someSettings = 0xFF;
		HWPort.BUFFER = 0;
		USART_TransmitINTDisable();
		/*********************************************/
		PortHandle->Status setBITS(PORT_SENDED_ALL);
		PortHandle->Status clearBITS(PORT_SENDING_LAST_BYTE | PORT_SENDING | PORT_BUSY);
		PortHandle->outCursor = 0;
		StopTimerWP(&PortHandle->SendingTimer);
	}
	else if ((PortHandle->Status & (PORT_BUSY | PORT_SENDED)) == ONLY (PORT_BUSY | PORT_SENDED)) { //After fully sended through fifo (or sending last byte)
		/*Real hardware interface peripheral settings*/
		USART_TransmitINTDisable();
		HWPort.clearOrResetSomeFlags = 0;
#ifdef IN_CASE_OF_FIFO_TYPE
		HWPort.TXInterruptEnable = 0;
		HWPort.clearFIFO = 1;
		PortHandle->Status clearBITS(PORT_SENDING_LAST_BYTE | PORT_SENDING | PORT_BUSY);
		StopTimerWP(&PortHandle->SendingTimer); //!!! [NOTE.3.A] controling timers here is a bad approach! //mb?
#endif // !IN_CASE_OF_FIFO_TYPE
		HWPort.someSettings = 0xFF;
		/*********************************************/
		if (PortHandle->outCursor >= PortHandle->LenDataToSend - 1)
			PortHandle->Status setBITS(PORT_SENDING_LAST_BYTE);
		PortHandle->outCursor++;
		HWPort.BUFFER = PortHandle->BufferToSend[PortHandle->outCursor];
		PortHandle->Status clearBITS(PORT_SENDED);
		PortHandle->Status setBITS(PORT_SENDING);
		HWPort.StartTX = 1;
		USART_TransmitINTEnable();
		u8 BUFFER = PortHandle->BufferToSend[PortHandle->outCursor];
		PortSend(PortHandle, BUFFER);
#ifdef ENABLE_DELAYED_RECV
		if (PortHandle->DelayedRecv.DelayedRecv) {   //??? DelayedRecvAskedToDoAfterSending
			void* arg = PortHandle->DelayedRecv.ifsArg; 
			u16 Len = PortHandle->DelayedRecv.maxLen;
			FUNCTION_EXECUTE_PRINT(/*TRACE_RECV_FUNC*/0);
			PortHandle->DelayedRecv.DelayedRecv(arg, Len);
		}
#endif // !ENABLE_DELAYED_RECV
	}
	else {
		res = -4;
	}
	return res;
}

int Recv(InterfacePortHandle_t* PortHandle, uint8_t *outBuff, const int maxPossibleSize)
{
	int res = 0;
	//u8 IsRecvTimerRinging = IsTimerWPRinging(&PortHandle->ReceivingTimer);
	if ((PortHandle->Status & (PORT_READY | PORT_RECEIVING | PORT_BUSY)) == ONLY PORT_READY) {
#ifdef ENABLE_DELAYED_RECV
		memset(&PortHandle->DelayedRecv, 0, sizeof(PortHandle->DelayedRecv));
#endif // !ENABLE_DELAYED_RECV
		PortHandle->LenDataToRecv = maxPossibleSize;
		PortHandle->inCursor = 0;
		FUNCTION_EXECUTE_PRINT(/*TRACE_RECV_TIMER*/0);
		RestartTimerWP(&PortHandle->ReceivingTimer);
		PortHandle->Status setBITS(PORT_BUSY | PORT_RECEIVING);
		PortHandle->Status clearBITS(PORT_RECEIVED | PORT_RECEIVED_ALL);
		/*Real hardware interface peripheral settings*/
		HWPort.clearOrResetSomeFlags = 0;
		HWPort.someSettings = 0xFF;
#ifdef IN_CASE_OF_FIFO_TYPE
		HWPort.clearFIFO = 1; //?mb
#endif // !IN_CASE_OF_FIFO_TYPE
		HWPort.RXInterruptEnable = 1;
		HWPort.StartRX = 1;
		USART_ReceiveINTEnable();
		/*********************************************/
	}
	else if (((PortHandle->Status & (PORT_BUSY | PORT_RECEIVED)) == ONLY (PORT_BUSY | PORT_RECEIVED)) /*&& !IsRecvTimerRinging*/) {
		//PortHandle->LenDataToRecv ..
		FUNCTION_EXECUTE_PRINT(/*TRACE_RECV_TIMER*/0);
		/*Real hardware interface peripheral settings*/
#ifdef IN_CASE_OF_FIFO_TYPE
		memcpy(&PortHandle->BufferRecved[PortHandle->inCursor], HWPort.FIFO_BUFFER, sizeof(HWPort.FIFO_BUFFER));
		PortHandle->inCursor += sizeof(HWPort.FIFO_BUFFER);
		HWPort.clearFIFO = 1;//?mb
#else
		/*********************************************/
#ifdef DEBUG_ON_VS
		if(PortHandle->Status & PORT_MASTER)
			;//!?+PortHandle->BufferRecved[PortHandle->inCursor] = HWPortN[MASTERNO].BUFFER;
		else
			;//!?+PortHandle->BufferRecved[PortHandle->inCursor] = HWPortN[SLAVENO].BUFFER;
#else
		PortHandle->BufferRecved[PortHandle->inCursor] = USART_GetDataFromReceiveISR();
		USART_ReceiveINTDisable();
#endif // !DEBUG_ON_VS
		PortHandle->inCursor++;
#endif // !IN_CASE_OF_FIFO_TYPE
		PortHandle->Status clearBITS(PORT_RECEIVED);
		//if(PortHandle->inCursor >= PortHandle->LenDataToRecv){
			//HARDWARE_INTERFACE_LOG(1, "MAX BUFFER SIZE occur!");
			//ErrorPortReceivingHandle(Port);
		//}
		/*--Real hardware interface peripheral settings--*/
		HWPort.clearOrResetSomeFlags = 0;
		HWPort.RXInterruptEnable = 0;
		HWPort.someSettings = 0xff;
		HWPort.RXInterruptEnable = 1;
		HWPort.StartRX = 1;
		USART_ReceiveINTEnable();
		/*-----------------------------------------------*/
	}
	else if ((PortHandle->Status & (PORT_BUSY | PORT_SENDING | PORT_RECEIVING)) == ONLY (PORT_BUSY | PORT_SENDING)) {//! mb reject from this feature
		//Set DoDelayedCall callback for This function after sending if user asked to do it before
		if (IsTimerWPStarted(&PortHandle->SendingTimer)) {
			FUNCTION_EXECUTE_PRINT(/*TRACE_RECV_TIMER*/1);
#ifdef ENABLE_DELAYED_RECV
			PortHandle->DelayedRecv.DelayedRecv = (DelayedRecv_fn*)Recv;
			PortHandle->DelayedRecv.ifsArg = PortHandle;
			PortHandle->DelayedRecv.maxLen = maxPossibleSize;
#endif // !ENABLE_DELAYED_RECV
		}
	}
	else {
		res = -4;
	}
	return res;
}
//#endif // IN_CASE_OF_FIFO_TYPE
int SendingHandle(InterfacePortHandle_t* Port)
{
	return Write(Port, NULL, no_required_now);
}

int ReceivingHandle(InterfacePortHandle_t* Port)
{
	return Recv(Port, NULL, no_required_now);
}

int SendingTimerHandle(InterfacePortHandle_t *Port) //!!<---  IsTimerWPStarted()?
{
	int res = 0;
	u8 IsSendingTimerRinging = IsTimerWPRinging(&Port->SendingTimer);
	if ((Port->Status & (PORT_BUSY | PORT_SENDING)) == ONLY(PORT_BUSY | STILL PORT_SENDING)) {
		if (NOT IsTimerWPStarted(&Port->SendingTimer)) {
			DEBUG_PRINTF(1, ("Send timer not started even!\n"));
			Port->Status clearBITS(PORT_BUSY | PORT_SENDING | PORT_SENDED | PORT_SENDING_LAST_BYTE | PORT_SENDED_ALL);
			res = -1;
		}
		else if (IsSendingTimerRinging) {
			StopTimerWP(&Port->SendingTimer);
			DEBUG_PRINTF(1, ("Sending timeout occured!\n"));
			//Error occur. Successfull sending should not reach Sending timeout! If SendingTimer Ringed then:
			//Port->Status setBITS(PORT_ERROR);
			//Port->sendErrCnt;
			Port->Status clearBITS(PORT_SENDING_LAST_BYTE | PORT_SENDING | PORT_BUSY | PORT_SENDED_ALL);
			res = -1;
		}
		if(res == -1){
			Port->errCnt++;
			Port->Status setBITS(PORT_ERROR);
			/*--Real hardware interface peripheral settings--*/
			HWPort.clearOrResetSomeFlags = 0;
			HWPort.TXInterruptEnable = 0;
#ifdef IN_CASE_OF_FIFO_TYPE
			HWPort.clearFIFO = 1;
#endif // !IN_CASE_OF_FIFO_TYPE
			HWPort.someSettings = 0xFF;
			USART_TransmitINTDisable();
			/*-----------------------------------------------*/
#ifdef ENABLE_DELAYED_RECV
			if (Port->DelayedRecv.DelayedRecv) {   //??? DelayedRecvAskedToDoAfterSending
				void* arg = Port->DelayedRecv.ifsArg;
				u16 Len = Port->DelayedRecv.maxLen;
				Port->DelayedRecv.DelayedRecv(arg, Len);
			}
#endif // !ENABLE_DELAYED_RECV
		}
	}
	//else if ((Port->Status & (PORT_BUSY | PORT_SENDING)) == NOTHING) {
	//	StopTimerWP(&Port->SendingTimer);
	//}
	return res;
}

#ifdef DEBUG_ON_VS
#include "../debug_print.h"
#endif // DEBUG_ON_VS

int ReceivingTimerHandle(InterfacePortHandle_t* PortHandle)
{
	int res = 0;
	u8 IsRecvTimerRinging = IsTimerWPRinging(&PortHandle->ReceivingTimer);
	FUNCTION_EXECUTE_PRINT(/*TRACE_RECV_TIMER*/0);
	if ((PortHandle->Status & (PORT_BUSY | PORT_RECEIVING)) == STILL ONLY (PORT_BUSY | PORT_RECEIVING)) {
		if (NOT IsTimerWPStarted(&PortHandle->ReceivingTimer)) {
			PortHandle->Status setBITS(PORT_ERROR);
			DEBUG_PRINTF(1, ("Recv timer not started even!\n"));
			PortHandle->Status clearBITS(PORT_RECEIVING | PORT_BUSY | PORT_RECEIVED);
			res = -1;
		}
		else if (IsRecvTimerRinging) {
			StopTimerWP(&PortHandle->ReceivingTimer);
			PortHandle->Status clearBITS(PORT_RECEIVING | PORT_BUSY | PORT_RECEIVED);
			if (PortHandle->inCursor != 0) {
				PortHandle->Status |= PORT_RECEIVED_ALL;//PORT_RECEIVED; //? mb RECEIVED_TIMEOUT or RECEIVED_ALL? ALL better
				PortHandle->Status clearBITS(PORT_ERROR);
			}
			else
			{
				//Port not received data;
				PortHandle->Status setBITS(PORT_ERROR);
				//PortHandle->RecvErrCnt++;
			}
			PortHandle->LenDataToRecv = PortHandle->inCursor;
			FUNCTION_EXECUTE_PRINT(/*TRACE_RECV_TIMER*/0);
			HARDWARE_INTERFACE_LOG(0, PortHandle->BufferRecved);
		}
		if((res == -1) || (IsRecvTimerRinging)){
			/*--Real hardware interface peripheral settings--*/
			HWPort.clearOrResetSomeFlags = 0;
			HWPort.RXInterruptEnable = 0;
			HWPort.StartRX = 0;
			HWPort.someSettings = 0xff;
#ifdef IN_CASE_OF_FIFO_TYPE
			HWPort.clearFIFO = 1;
#endif // !IN_CASE_OF_FIFO_TYPE
			//memset(HWPort.FIFO_BUFFER, 0, sizeof(HWPort.FIFO_BUFFER /*.LenDataToRecv*/)); //! Nope! Don't clear!
			USART_ReceiveINTDisable();
			/*-----------------------------------------------*/
		}
	}
	return res;
}

static void ErrorPortSendingHandle(InterfacePortHandle_t *Port)
{
	Port->errCnt++;
	/*--Real hardware interface peripheral settings--*/
	HWPort.clearOrResetSomeFlags = 0;
	HWPort.TXInterruptEnable = 0;
#ifdef IN_CASE_OF_FIFO_TYPE
	HWPort.clearFIFO = 1;
#endif // !IN_CASE_OF_FIFO_TYPE
	HWPort.someSettings = 0xFF;
	USART_TransmitINTDisable();
	/*-----------------------------------------------*/
#ifdef ENABLE_DELAYED_RECV
	memset(&Port->DelayedRecv, 0, sizeof(Port->DelayedRecv));
#endif // !ENABLE_DELAYED_RECV
	Port->Status setBITS(PORT_ERROR);
	Port->Status clearBITS(PORT_BUSY | PORT_SENDING | PORT_SENDED | PORT_SENDING_LAST_BYTE);
	StopTimerWP(&Port->SendingTimer);
}

static void ErrorPortReceivingHandle(InterfacePortHandle_t* Port)
{

}

#define DISABLE_PORTSIMMITATIONS_BY_UDPIP
#ifndef DISABLE_PORTSIMMITATIONS_BY_UDPIP
static int immitationOfPortsBus(InterfacePortHandle_t* PortHandle) //! immitationSendingOfPortsBus()
{
	int res = 0;
	char buffer[300];
	SOCKADDR_IN *remoteNodeAddr = &ConnectSocketIfs.interfaceService;
	int remoteNodeAddrSize = sizeof(SOCKADDR_IN);
	//char portsBusMessageId = portsMessageId;
	char* DirectionSendingOfBusMessageId;
	if (PortHandle->Status & PORT_MASTER) {
		DirectionSendingOfBusMessageId = mastersMessageId;
	}else {
		DirectionSendingOfBusMessageId = slavesMessageId;
	}
	u16 cursorPos = 0;
	SOCKET* SocketHandle;
#ifdef MASTER_PORT_PROJECT
	SocketHandle = &ConnectSocketIfs.Socket;
	if (ThisMastersConfigs.localTimeEn == 1) {
		SYSTEMTIME currTime;
		GetLocalTime(&currTime);
		sprintf(buffer, "%dh:%dm:%ds: ", currTime.wHour, currTime.wMinute, currTime.wSecond);
		cursorPos = strlen(buffer);
	}
#elif SLAVE_PORT_PROJECT
	SocketHandle = &ListenSocketIfs.Socket;
#endif // MASTER_PORT_PROJECT	
	//"%s%s %s\n"
	sprintf(&buffer[cursorPos], "%s %s\n", DirectionSendingOfBusMessageId, PortHandle->BufferToSend); //sizes?
	size_t siz;
	//res = shutdown(ConnectSocket, SD_RECEIVE);
	TakeMutex(SocketMutex, maxDELAY);
	DEBUG_PRINTF(1, ("sending time ms: %d\n", StopWatchWP(&timeMeasure[1])));
	StopWatchWP(&timeMeasure[2]);
	res = sendto(*SocketHandle, buffer, strlen(buffer), 0, remoteNodeAddr, remoteNodeAddrSize);
	DEBUG_PRINTF(1, ("sendto time ms: %d\n", StopWatchWP(&timeMeasure[2])));
	ReleaseMutex(SocketMutex);
	if ((res != -1) && (res > 0)) {
		commonMasterSlaveCfgs_t* currentObjCfg;
#ifdef MASTER_PORT_PROJECT
		currentObjCfg = &ThisMastersConfigs;
#else
		currentObjCfg = &ThisSlavesConfigs;
#endif // MASTER_PORT_PROJECT
		if (res > 0) {
			currentObjCfg/*[PortNo]*/->currentIOfileLine++;
			TransmitInterrupt(PortHandle); //Called_TXInterrupt()
		}
	}
	else {
		//ERROR;
	}
	return res;
}

int immitationReceivingOfPortsBus(InterfacePortHandle_t* outPortHandle)
{
	SOCKADDR_IN remoteNodeAddr;
	int remoteNodeAddrSize = sizeof(remoteNodeAddr);
	int res = 0;
	int PortNo = 0;
	FRESULT fres = FR_OK;
	char buffer[300];
	//char portsBusMessageId = portsMessageId;
	if (NOT IsTimerWPRinging(&outPortHandle->ReceivingTimer)) {
		DEBUG_PRINTFS(0, ("NOT recv ringing section measure: %u\n", StopWatchWP(&timeMeasure[3])));
		TakeMutex(SocketMutex, maxDELAY);
		res = 
#ifdef MASTER_PORT_PROJECT
			recvWithTimeoutToClient
#elif SLAVE_PORT_PROJECT
			recvWithTimeoutToServer
#endif // MASTER_PORT_PROJECT
		(buffer, sizeof(buffer), (U32_ms)100);
		ReleaseMutex(SocketMutex);
		if (res == 0) {
			DEBUG_PRINTF(0, ("Timeout occured!\n"));
			res = -1;
			return res;
		}
		else if (res > 0) {
			DEBUG_PRINTFS(1, ("readed period: %u\n", StopWatchWP(&timeMeasure[0])));
			DEBUG_PRINTFS(1, ("Readed data: %s\n", buffer, sizeof(buffer)));
		}
	}
	/*if (strncmp(buffer, portsMessageId, strlen(portsMessageId) - 2) == 0) {
		buffer[strlen(portsMessageId) - 2] = PortNo;
	}*/
	char* DirectionSendingOfBusMessageId;
	size_t MessageIdlen = 0;
	if (outPortHandle->Status & PORT_MASTER) {
		DirectionSendingOfBusMessageId = slavesMessageId; //Receiving from SLAVE
		MessageIdlen = strlen(slavesMessageId);
	}else{
		DirectionSendingOfBusMessageId = mastersMessageId;
		MessageIdlen = strlen(mastersMessageId);
	}
	if ((fres == FR_OK) && (strncmp(buffer, DirectionSendingOfBusMessageId, MessageIdlen) == 0)) {
		if (outPortHandle->Status && PORT_MASTER && (DirectionSendingOfBusMessageId == slavesMessageId)) {
#ifdef MASTER_PORT_PROJECT
			//Port detected a datas on Bus 
			//we pretend that the Hardware has the big FIFO
			//The copying should be but not in here. This is the immitation of interrupt section
			//on bus/port by detecting datas on there. The copying to InterfacePort.BuffRecved must be occure on inside interrupt section. 
			memcpy(HWPort.FIFO_BUFFER, &buffer[strlen(slavesMessageId)], sizeof(HWPort.FIFO_BUFFER));
			ThisMastersConfigs.lastReadedLine = ThisMastersConfigs.currentIOfileLine++;
#endif // MASTER_PORT_PROJECT
		}
		else if ((DirectionSendingOfBusMessageId == mastersMessageId)) {
#ifdef SLAVE_PORT_PROJECT
			memcpy(HWPort.FIFO_BUFFER, &buffer[strlen(mastersMessageId)], sizeof(HWPort.FIFO_BUFFER));
#endif // SLAVE_PORT_PROJECT
		}
		ReceiveInterrupt(&InterfacePort);
	}
	else { //?
		res = (int)fres;
		//RestartTimerWP(&outPortHandle->ReceivingTimer); //? NOPE! delete it!
	}
	return res;
}
#endif // !DISABLE_PORTSIMMITATIONS_BY_UDPIP

void TransmitInterrupt(void *arg) 
{
	InterfacePortHandle_t* Port = (InterfacePortHandle_t *)arg;
	if ((Port->Status & (PORT_READY | PORT_SENDING)) == ONLY (PORT_READY | PORT_SENDING)) {
		Port->Status |= PORT_SENDED;
		Write(Port, NULL, no_required_now);
	}
	//StopTimerWP(&Port->SendingTimer);
	return;
}

void ReceiveInterrupt(void* arg) //ReceiveInterrupt()
{
	InterfacePortHandle_t* Port = (InterfacePortHandle_t*)arg;
	if ((Port->Status & (PORT_READY | PORT_RECEIVING)) == ONLY (PORT_READY | PORT_RECEIVING)) {
		Port->Status |= PORT_RECEIVED;
		FUNCTION_EXECUTE_PRINT(/*TRACE_RECV_FUNC*/0);
		Recv(Port, NULL,no_required_now);
	}
	return;
}


static int PortSend(InterfacePortHandle_t *PortHandle, uint8_t BUFFER)
{
	int res = 0;
#ifdef CMSIS_OS_ENABLE  // & SIMULATION_TESTING  /*Do simulation for sending*/
	portsBuffer_t* ifsPtr;
	ifsPtr = osPoolAlloc(mpool);
	ifsPtr->Port = PortHandle;
	ifsPtr->BUFFER = BUFFER;
	osMessagePut(MsgBox, (uint32_t /*not 64 bit in VS//?*/)ifsPtr, 10/*osWaitForever*/);
	if (ifsPtr == NULL)
		res = -1;
#else
	UNUSED(PortHandle);
	res = USART_Transmit(BUFFER);
#endif // !CMSIS_OS_ENABLE
	return res;
}
//#ifdef TRACE_PORT_ENABLE
//#endif // TRACE_PORT_ENABLE
//tracePortCfg_t PortTracer;

/*PORT_READY = 1,
  PORT_BUSY = 1 << 1,
  PORT_SENDING = 1 << 2,
  PORT_SENDED = 1 << 3,
  PORT_SENDING_LAST_BYTE = 1 << 4,
  PORT_RECEIVING = 1 << 5,
  PORT_RECEIVED = 1 << 6,
  PORT_ASYNC = 1 << 7,
  PORT_MASTER = 1 << 8,
  PORT_BUFFER_FIFO = 1 << 9,*/
static stopwatchwp_t tracemeasure;
void tracePortInit(tracePortCfg_t* traceP)
{
	memset(traceP, 0, sizeof(tracePortCfg_t)); //!
	//memset(traceP->accumulatedStats, 0, sizeof(traceP->accumulatedStats));
	traceP->accumArrayPos = 0;
#ifndef MINIMAL_CODESIZE
	InitStopWatchWP(&tracemeasure, (tickptr_fn*)GetTickCount);
	InitTimerWP(&traceP->TraceTime, (tickptr_fn*)GetTickCount);
	InitTimerWP(&traceP->FrequencyTrace, (tickptr_fn*)GetTickCount);
#else
	InitTimerWP(&traceP->TraceTime, NULL);
	InitTimerWP(&traceP->FrequencyTrace, NULL);
#endif // !MINIMAL_CODESIZE
	LaunchTimerWP(2000, &traceP->TraceTime);
	LaunchTimerWP(50, &traceP->FrequencyTrace);
}
void tracePort(InterfacePortHandle_t* Port, tracePortCfg_t *traceP) {
	if (IsTimerWPRinging(&traceP->FrequencyTrace)) {
		RestartTimerWP(&traceP->FrequencyTrace);
		if (traceP->accumArrayPos < ACCUMUL_CAPACITY) {
			traceP->accumulatedStats[traceP->accumArrayPos++] = Port->Status;
		}else {
			traceP->accumArrayPos--;
			printf("Tracing over capacity space!\n");
			RestartTimerWP(&traceP->TraceTime);
			ShowTracedAccumulations(traceP);
			traceP->accumArrayPos = 0;
		}
	}
	if (IsTimerWPRinging(&traceP->TraceTime)) {
#ifndef MINIMAL_CODESIZE
		StopWatchWP(&tracemeasure);
#endif // !MINIMAL_CODESIZE
		RestartTimerWP(&traceP->TraceTime);
		ShowTracedAccumulations(traceP);
		traceP->accumArrayPos = 0;
	}

}
#define SET_BIT(x) (1 << x)
void ShowTracedAccumulations(tracePortCfg_t* traceP)
{
	u16 bitPos = 0;
	u16 AccumCnt = 0;
	u8 stats[9];
	printf("MAST  ASYN  RCVED  RCVING  SNDGLAST  SNDED  SNDING  BUSY  REDY\n");
	for (AccumCnt; AccumCnt <= traceP->accumArrayPos; AccumCnt++) {
		for (bitPos = BitPos(PORT_READY); bitPos <= BitPos(PORT_MASTER); bitPos++) {
			stats[bitPos] = (traceP->accumulatedStats[AccumCnt] & SET_BIT(bitPos)) > 0;
		}
		printf("%2d%7d%6d%7d%9d%9d%7d%7d%6d\n", stats[8], stats[7], stats[6], stats[5], stats[4], stats[3], stats[2], stats[1], stats[0]);
	}
	traceP->accumArrayPos = 0;
}

u16 BitPos(u16 Bit)
{
	u16 res = 0;
	while ((Bit >> res) > 1) {
		res++;
	}
	return res;
}

#ifdef __cplusplus
}
#endif // !_cplusplus

#pragma region BLACKNOTE_AND_THOUGHTS
#define DISABLE_BLACKNOTE
#ifndef DISABLE_BLACKNOTE
#define IN_CASE_OF_8BIT_PORTION_DATAS
#define IN_CASE_OF_FIFO_TYPE //disable it after
#define no_required_now 0
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
HardwarePort_t HWPort;
#ifdef IN_CASE_OF_8BIT_PORTION_DATAS //mb IN_CASE_OF_8BIT_SINGLE_BUFFER
//send
_Write(u8 *InDatas, u16 Len) //Write()
{
	u16 all = 0;
	u16 res = 0;
	if (InterfacePort.Status & (PORT_READY | PORT_SENDING | PORT_BUSY) == ONLY PORT_READY) {
		memcpy(InterfacePort.BufferToSend, InDatas, Len);
		InterfacePort.LenDataToSend = Len;
		InterfacePort.outCursor = 0;
		InterfacePort.outCursor++;
		HWPort.someSettings = 0xFF;
		HWPort.BUFFER = InterfacePort.BufferToSend[0];
		HWPort.TXInterruptEnable = 1;
		HWPort.StartRX = 0;
		HWPort.StartTX = 1;
		InterfacePort.Status |= PORT_BUSY;
		InterfacePort.Status |= PORT_SENDING;
		InterfacePort.Status &= ~PORT_SENDED;
		InterfacePort.Status &= ~PORT_RECEIVED;
		LaunchTimerWP(InterfacePort.SendingTimer.setVal, &InterfacePort.SendingTimer);
	}
	else if (InterfacePort.Status & (all = PORT_BUSY | PORT_SENDING_LAST_BYTE /* | ???*/) == all ONLY) {
		HWPort.clearOrResetSomeFlags = 0;
		HWPort.TXInterruptEnable = 0;
		HWPort.BUFFER = 0;
		HWPort.someSettings = 0xFF; //mb
		InterfacePort.Status clearBITS(PORT_SENDING_LAST_BYTE | PORT_SENDING | PORT_BUSY); //?whaaaa!?
		InterfacePort.Status &= ~(PORT_SENDING_LAST_BYTE | PORT_SENDING | PORT_BUSY);
		InterfacePort.outCursor = 0;
		StopTimerWP(&InterfacePort.SendingTimer);
	}
	else if (InterfacePort.Status & (all = PORT_BUSY | PORT_SENDED) == all ONLY) { //?is it has good speed execution by MCU processor if we call it from inside of interrupt?
		HWPort.clearOrResetSomeFlags = 0;
		if (InterfacePort.outCursor >= InterfacePort.LenDataToSend - 1)
			InterfacePort.Status |= PORT_SENDING_LAST_BYTE;
		HWPort.BUFFER = &InterfacePort.BufferToSend[InterfacePort.outCursor];
		InterfacePort.outCursor++;
		InterfacePort.Status &= ~PORT_SENDED;
		InterfacePort.Status |= PORT_SENDING;
		HWPort.StartTX = 1;
	}
	//else {
	//	res = -4;
	//}
	if (InterfacePort.Status & PORT_SENDING)
		res = InterfacePort.outCursor;
	//return res;
}

_Receive(InterfacePortHandle_t *ifsPort, /*const u8* outBuff, mb not needed even//?!*/ u16 maxPossibleLen)
{
	u16 both = 0, all = 0, res = 0;
	u8 IsRecvTimerRinging = IsTimerWPRinging(&InterfacePort.ReceivingTimer);
	if (InterfacePort.Status & (PORT_READY | PORT_RECEIVING | PORT_BUSY) == ONLY PORT_READY) {
		InterfacePort.inCursor = 0;
		InterfacePort.LenDataToRecv = maxPossibleLen;
		LaunchTimerWP(InterfacePort.ReceivingTimer.setVal, &InterfacePort.ReceivingTimer);
		InterfacePort.Status |= PORT_BUSY | PORT_RECEIVING;
		InterfacePort.Status &= ~PORT_RECEIVED;
		HWPort.clearOrResetSomeFlags = 0;
		HWPort.someSettings = 0xFF;
		//IsTheseDatasForMe;
		HWPort.RXInterruptEnable = 1;
		HWPort.StartRX = 1;
		//HWPort.StartTX = 0;
	}
	else if((InterfacePort.Status & ((both = PORT_BUSY | PORT_RECEIVED))) && !IsRecvTimerRinging == both ONLY)
	{
		HWPort.clearOrResetSomeFlags = 0;
		HWPort.RXInterruptEnable = 0;
		HWPort.someSettings = 0xff;

		//Recv timeouts
		InterfacePort.BufferRecved[InterfacePort.inCursor++] = HWPort.BUFFER;
		InterfacePort.Status clearBITS(PORT_RECEIVED);
		HWPort.RXInterruptEnable = 1;
		HWPort.StartRX = 1;
	}
	else {
		res = -4;
	}
	if (IsRecvTimerRinging && (InterfacePort.Status & PORT_RECEIVING)) { //?mb
		HWPort.clearOrResetSomeFlags = 0;
		HWPort.RXInterruptEnable = 0;
		HWPort.StartRX = 0;
		HWPort.someSettings = 0xff;
		StopTimerWP(&InterfacePort.ReceivingTimer);
		InterfacePort.Status clearBITS(PORT_RECEIVING | PORT_BUSY);
		InterfacePort.Status |= PORT_RECEIVED; //? mb RECEIVED_TIMEOUT or RECEIVED_ALL?
		InterfacePort.LenDataToRecv = InterfacePort.inCursor;
	}
	return res;
}
#endif //IN_CASE_OF_8BIT_PORTION_DATAS

#ifdef IN_CASE_OF_FIFO_TYPE
__Write(u8* InDatas, u16 Len)
{
	int res = 0;
	//IsSendingTimerRinging = IsTimerWPRinging(&InterfacePort.SendingTimer);
	if (InterfacePort.Status & (PORT_READY | PORT_SENDING | PORT_BUSY) == ONLY PORT_READY) {
		memcpy(InterfacePort.BufferToSend, InDatas, Len);
		InterfacePort.LenDataToSend = Len;
		//InterfacePort.outCursor++;
		memcpy(HWPort.FIFO_BUFFER, InterfacePort.BufferToSend, InterfacePort.LenDataToSend);
		HWPort.someSettings = 0xFF;
		HWPort.TXInterruptEnable = 1;
		HWPort.StartRX = 0;
		HWPort.StartTX = 1;
		InterfacePort.Status |= PORT_BUSY;
		InterfacePort.Status |= PORT_SENDING;
		InterfacePort.Status &= ~PORT_SENDED;
		InterfacePort.Status &= ~PORT_RECEIVED;
		LaunchTimerWP(InterfacePort.SendingTimer.setVal, &InterfacePort.SendingTimer);
	}else if (InterfacePort.Status & (PORT_BUSY | PORT_SENDING_LAST_BYTE) == ONLY PORT_BUSY | PORT_SENDING_LAST_BYTE) {
		;;
	}else if (InterfacePort.Status & (PORT_BUSY | PORT_SENDED) == ONLY PORT_BUSY | PORT_SENDED) {
		HWPort.clearOrResetSomeFlags = 0;
		HWPort.TXInterruptEnable = 0;
		HWPort.clearFIFO = 1;
		HWPort.someSettings = 0xFF;
		InterfacePort.Status clearBITS(PORT_SENDING_LAST_BYTE | PORT_SENDING | PORT_BUSY);
		StopTimerWP(&InterfacePort.SendingTimer);
	}
	else {
		res = -4;
	}
	//if (InterfacePort.Status & PORT_SENDING)
		//res = InterfacePort.outCursor;
	return res;
}

__Receive(InterfacePortHandle_t* ifsPort, u16 maxPossibleLen)
{
	int res = 0;
	u8 IsRecvTimerRinging = IsTimerWPRinging(&InterfacePort.ReceivingTimer);
	if (InterfacePort.Status & (PORT_READY | PORT_RECEIVING | PORT_BUSY) == ONLY PORT_READY) {
		InterfacePort.LenDataToRecv = maxPossibleLen;
		LaunchTimerWP(InterfacePort.ReceivingTimer.setVal, &InterfacePort.ReceivingTimer);
		InterfacePort.Status |= PORT_BUSY | PORT_RECEIVING;
		InterfacePort.Status &= ~PORT_RECEIVED;
		HWPort.clearOrResetSomeFlags = 0;
		HWPort.someSettings = 0xFF;
		HWPort.clearFIFO = 1; //?mb
		HWPort.RXInterruptEnable = 1;
		HWPort.StartRX = 1;
	}
	else if ((InterfacePort.Status & (PORT_BUSY | PORT_RECEIVED)) && !IsRecvTimerRinging == ONLY PORT_BUSY | PORT_RECEIVED) {
		//InterfacePort.LenDataToRecv ..
		InterfacePort.inCursor += sizeof(HWPort.FIFO_BUFFER);
		memcpy(InterfacePort.BufferRecved, HWPort.FIFO_BUFFER, sizeof(HWPort.FIFO_BUFFER));
		InterfacePort.Status clearBITS(PORT_RECEIVED);
		//if(InterfacePort.inCursor >= InterfacePort.LenDataToRecv){
		//}
		HWPort.clearFIFO = 1;//?mb
		HWPort.clearOrResetSomeFlags = 0;
		HWPort.RXInterruptEnable = 0;
		HWPort.someSettings = 0xff;
		HWPort.RXInterruptEnable = 1;
		HWPort.StartRX = 1;
	}
	else if (InterfacePort.Status & (PORT_BUSY | PORT_SENDING | PORT_RECEIVING) == ONLY PORT_BUSY | PORT_SENDING) {
		//DoDelayedCall This function
		InterfacePort.DelayedRecv.DelayedRecv = (DelayedRecv_fn *)_Receive;
		InterfacePort.DelayedRecv.ifsArg = ifsPort;
		InterfacePort.DelayedRecv.maxLen = maxPossibleLen;
	}
	else {
		res = -4;
	}
	if (IsRecvTimerRinging && (InterfacePort.Status & PORT_RECEIVING)) {
		HWPort.clearOrResetSomeFlags = 0;
		HWPort.RXInterruptEnable = 0;
		HWPort.StartRX = 0;
		HWPort.someSettings = 0xff;
		HWPort.clearFIFO = 1;
		//memset(HWPort.FIFO_BUFFER, 0, sizeof(HWPort.FIFO_BUFFER /*.LenDataToRecv*/));
		StopTimerWP(&InterfacePort.ReceivingTimer);
		InterfacePort.Status clearBITS(PORT_RECEIVING | PORT_BUSY);
		InterfacePort.Status |= PORT_RECEIVED; //? mb RECEIVED_TIMEOUT or RECEIVED_ALL?
		InterfacePort.LenDataToRecv = InterfacePort.inCursor;
	}
	return res;
}

___HardwareSentInterrupt()
{

}

___HardwareReceiveInterrupt()
{

}
#endif // IN_CASE_OF_FIFO_TYPE


__HardwareSentInterrupt()
{
	u16 both = 0;
	u16 all = 0;
	if (InterfacePort.Status & (both = (PORT_READY | PORT_SENDING)) == both ONLY) { //? check this!
		InterfacePort.Status |= PORT_SENDED; //?mb it better when flags are being handled in one place? That would be more readable
		_Write(NULL, no_required_now);
	}
}

__HardwareReceiveInterrupt()
{
	u16 both = 0, all = 0;
	if (InterfacePort.Status & (both = (PORT_READY | PORT_RECEIVING)) == both ONLY) {
		InterfacePort.Status |= PORT_RECEIVED;
		_Receive(NULL, no_required_now);
	}
	return;
}

/*
		 _____________
		||_|  BUS__|__\
		|___ ______ ___|
		    O      O
*/

__TimerInterrupt() {}; //mb
#endif // !DISABLE_BLACKNOTE
#pragma endregion