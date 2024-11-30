#ifndef _MASTERIMMITATIONCFG_H_
#define _MASTERIMMITATIONCFG_H_
#include <stdint.h>

typedef struct{
  uint8_t Status;
  uint32_t lastReadedLine; //from IO File
  uint32_t currentIOfileLine;
  uint16_t SlavesAddressToTalk;
  uint8_t  function;
  uint16_t AddressOfSlavesMemoryToTalk;
  uint16_t LenDataToTalk;
  uint16_t communicationPeriod;
  uint8_t* dataToWrite;
  uint8_t localTimeEn;
}thisMastercfgs_t;
/*
typedef struct {
	uint8_t Status;
	uint32_t lastReadedLine;
	uint32_t currentIOfileLine;
}commonMasterSlaveCfgs_t;*/

extern thisMastercfgs_t ThisMastersConfigs;
#endif //_MASTERIMMITATIONCFG_H_