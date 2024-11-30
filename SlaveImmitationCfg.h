#ifndef _SLAVEIMMITATIONCFG_H_
#define _SLAVEIMMITATIONCFG_H_
#include <stdint.h>

typedef struct{
  uint8_t Status;
  uint32_t lastReadedLine; //from IO File
  uint32_t currentIOfileLine;
  uint16_t MyAddress;
  uint16_t AddressOfMineMemoryToBeginTransmit;
  uint16_t LenDataToWrite;
  uint16_t ResponseTimeout;
  uint8_t* dataFromRead;
}thisSlavecfgs_t;

/*
typedef struct {
	uint8_t Status;
	uint32_t lastReadedLine; //from IO File
	uint32_t currentIOfileLine;
}commonMasterSlaveCfgs_t;*/

extern thisSlavecfgs_t ThisSlavesConfigs;
#endif //_SLAVEIMMITATIONCFG_H_