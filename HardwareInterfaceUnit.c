#ifdef __cplusplus
extern "C" {
#endif // !_cplusplus

#include "HardwareInterfaceUnit.h"
//#include "PortsBusMessages.h"
//#include "../../MultiThreadSupport.h"

#if defined(DEBUG_ON_VS) && defined(CMSIS_OS_ENABLE)
osPoolId mpool;
osMessageQId MsgBox;
#else
HardwarePort_t HWPort; //!?
#endif // !DEBUG_ON_VS && CMSIS_OS_ENABLE

#include "usart.h"

static int PortSend(InterfacePortHandle_t* PortHandle, uint8_t BUFFER);
static void ErrorPortSendingHandle(InterfacePortHandle_t *Port);

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
	PortHandle->ReceivingTimer.setVal = 50 x1ms;
	PortHandle->SendingTimer.setVal = 400 x1ms;
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
	else if ((PortHandle->Status & (PORT_BUSY | PORT_RECEIVED)) == ONLY (PORT_BUSY | PORT_RECEIVED)) {
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
#if MASTER_OR_SLAVE_IMMIT_EN
		if(PortHandle->Status & PORT_MASTER)
			PortHandle->BufferRecved[PortHandle->inCursor] = HWPortN[MASTERNO].BUFFER;
		else
			PortHandle->BufferRecved[PortHandle->inCursor] = HWPortN[SLAVENO].BUFFER;
#endif // !MASTER_OR_SLAVE_IMMIT_EN == 1
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
			Port->Status clearBITS(PORT_BUSY | PORT_SENDING | PORT_SENDED | PORT_SENDING_LAST_BYTE /*| PORT_SENDED_ALL*/);
			res = -1;
		}
		else if (IsSendingTimerRinging) {
			StopTimerWP(&Port->SendingTimer);
			DEBUG_PRINTF(1, ("Sending timeout occured!\n"));
			//Error occur. Successfull sending should not reach Sending timeout! If SendingTimer Ringed then:
			Port->Status clearBITS(PORT_SENDING_LAST_BYTE | PORT_SENDING | PORT_BUSY /*| PORT_SENDED_ALL*/);
			res = -1;
		}
		if(res == -1){
			Port->sendErrCnt++;
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
//#include "../debug_print.h"
#endif // DEBUG_ON_VS

int ReceivingTimerHandle(InterfacePortHandle_t* PortHandle)
{
	int res = 0;
	u8 IsRecvTimerRinging = IsTimerWPRinging(&PortHandle->ReceivingTimer);
	FUNCTION_EXECUTE_PRINT(/*TRACE_RECV_TIMER*/0);
	if ((PortHandle->Status & (PORT_BUSY | PORT_RECEIVING)) == STILL ONLY (PORT_BUSY | PORT_RECEIVING)) {
		if ((NOT IsTimerWPStarted(&PortHandle->ReceivingTimer))&&(NOT (PortHandle->Status & PORT_RECEIVING_CONTINIOUS))) {
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
				//PortHandle->recvErrCnt++;
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
			if(NOT (PortHandle->Status & PORT_RECEIVING_CONTINIOUS))
                USART_ReceiveINTDisable();
			/*-----------------------------------------------*/
		}
	}
	return res;
}

static void ErrorPortSendingHandle(InterfacePortHandle_t *Port)
{
	Port->sendErrCnt++;
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
	osMessagePut(MsgBox, (uint32_t)ifsPtr, 10/*osWaitForever*/);
	if (ifsPtr == NULL)
		res = -1;
#else
	UNUSED(PortHandle);
	res = USART_Transmit(BUFFER);
#endif // !CMSIS_OS_ENABLE
	return res;
}

int RecvContiniousStart(InterfacePortHandle_t* Port, uint8_t *outBuff)
{
	if ((Port->Status & (PORT_BUSY | PORT_SENDING)) == NOTHING) {
        //startRXframe=0;
		memset(Port->BufferRecved, 0, RECV_BUFFER_SIZE);
        Port->Status setBITS(PORT_RECEIVING_CONTINIOUS);
		//if ((PortHandle->Status & (PORT_READY | PORT_RECEIVING | PORT_BUSY)) == ONLY PORT_READY) {
		Port->LenDataToRecv = RECV_BUFFER_SIZE;
		Port->inCursor = 0;
		FUNCTION_EXECUTE_PRINT(/*TRACE_RECV_TIMER*/0);
		//! RestartTimerWP(&PortHandle->ReceivingTimer);
		Port->Status setBITS(PORT_BUSY | PORT_RECEIVING);
		Port->Status clearBITS(PORT_RECEIVED | PORT_RECEIVED_ALL);
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
	//}
	}
}

int StopRecvContinious(InterfacePortHandle_t* Port) 
{
    ///startRXframe=1;
	USART_ReceiveINTDisable();
    Port->Status clearBITS(PORT_RECEIVING_CONTINIOUS);
	if (Port->Status & PORT_RECEIVED_ALL)
		Port->Status clearBITS(PORT_RECEIVED_ALL);
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


/*
		 _____________
		||_|  BUS__|__\
		|___ ______ ___|
		    O      O
*/
#endif // !DISABLE_BLACKNOTE
#pragma endregion