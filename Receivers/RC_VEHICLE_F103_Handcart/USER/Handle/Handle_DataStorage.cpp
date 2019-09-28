#include "FileGroup.h"
#include "EEPROM.h"

#define STARTADDR 0x20
#define RegDataList_MAX 20

using namespace EEPROM_Chs;

typedef struct {
    uint16_t DataLength;
    uint16_t DataCntSum;
    uint32_t CheckSum;
} EEPROM_DataHead_t;

typedef struct {
    uint8_t *pData;
    uint16_t Size;
} EEPROM_RegList_t;

static EEPROM_RegList_t DataRegList[RegDataList_MAX];//�û������б�
static uint16_t RegListOffset = 0;

static uint16_t EEPROM_ReadNext(bool reset = false, uint16_t start = STARTADDR)
{
    static uint16_t OffsetAddr = STARTADDR;
    if(reset)
        OffsetAddr = start;

    uint16_t ret = EEPROM.read(OffsetAddr);
    OffsetAddr++;
    return ret;
}

static void EEPROM_WriteNext(uint16_t data, bool reset = false, uint16_t start = STARTADDR)
{
    static uint16_t OffsetAddr = STARTADDR;
    if(reset)
        OffsetAddr = start;

    EEPROM.update(OffsetAddr, data);
    OffsetAddr++;
}

/*ע���������ͱ���*/
bool EEPROM_Register(void *pdata, uint16_t size)
{
    if(RegListOffset == RegDataList_MAX)
        return false;
    
    DataRegList[RegListOffset].pData = (uint8_t*)pdata;
    DataRegList[RegListOffset].Size = size;
    RegListOffset++;
    return true;
}

/*
���ݽṹ:
|                     ֡ͷ                       |                 �û�����
| �û������ܳ� | �û������ܸ��� | �û�����У��� | ����1��С |  ����1  | ����2��С |  ����2  |...
|     2Byte    |     2Byte      |     4Byte      |   2Byte   |  xByte  |   2Byte   |  xByte  |...
*/
bool EEPROM_Handle(EEPROM_Chs_t chs)
{
    bool retval = false;
    EEPROM_DataHead_t head = {0};
    if(chs == ReadData)
    {
        /*��ȡ֡ͷ��Ϣ*/
        ((uint16_t*)(&head))[0] = EEPROM_ReadNext(true);//DataLength����ָ��ָ���׵�ַ
        ((uint16_t*)(&head))[1] = EEPROM_ReadNext();//DataCntSum
        ((uint16_t*)(&head))[2] = EEPROM_ReadNext();//CheckSum(HIGH 16bit)
        ((uint16_t*)(&head))[3] = EEPROM_ReadNext();//CheckSum(LOW  16bit)

        /*��ȡ�û�����У���*/
        uint32_t UserDataSum = 0;
        for(uint16_t i = 0; i < head.DataLength; i++)
            UserDataSum += EEPROM_ReadNext();

        /*У���û������Ƿ���ע���б�ƥ��*/
        if(UserDataSum != head.CheckSum || head.DataCntSum != RegListOffset)
            return false;

        /*��֡ͷ��Ϣĩ��ַ�Ļ�������ǰ�ƶ�һ����ַ������һ�ζ�ȡ�׵�ַָ���û������׵�ַ*/
        EEPROM_ReadNext(true, STARTADDR + sizeof(head) / 2 - 1);

        /*����ע���б�*/
        for(uint16_t cnt = 0; cnt < head.DataCntSum; cnt++)
        {
            uint16_t UserSize = EEPROM_ReadNext();

            /*/У���Ƿ����û�ע���б�ƥ��*/
            if(UserSize != DataRegList[cnt].Size)
                return false;

            /*����ȡ������д��ע���б���pDataָ��ָ�������*/
            for(uint16_t offset = 0; offset < UserSize; offset++)
                (DataRegList[cnt].pData)[offset] = EEPROM_ReadNext();
        }
        retval = true;
    }
    else if(chs == SaveData)
    {
        /*����֡ͷ��Ϣ*/

        /*�û������ܸ���*/
        head.DataCntSum = RegListOffset;

        /*����ע���б�*/
        for(uint16_t cnt = 0; cnt < RegListOffset; cnt++)
        {
            /*�û������ܳ�*/
            head.DataLength += sizeof(DataRegList[cnt].Size) / 2 + DataRegList[cnt].Size;
            /*�û�����У���*/
            head.CheckSum += DataRegList[cnt].Size;
            for(uint16_t offset = 0; offset < DataRegList[cnt].Size; offset++)
                head.CheckSum += (DataRegList[cnt].pData)[offset];
        }

        /*д��֡ͷ��Ϣ*/
        EEPROM_WriteNext(((uint16_t*)(&head))[0], true);//DataLength����ָ��ָ���׵�ַ
        EEPROM_WriteNext(((uint16_t*)(&head))[1]);//DataCntSum
        EEPROM_WriteNext(((uint16_t*)(&head))[2]);//CheckSum(HIGH 16bit)
        EEPROM_WriteNext(((uint16_t*)(&head))[3]);//CheckSum(LOW  16bit)

        /*����ע���б���д���û�����*/
        for(uint16_t cnt = 0; cnt < RegListOffset; cnt++)
        {
            EEPROM_WriteNext(DataRegList[cnt].Size);
            for(uint16_t offset = 0; offset < DataRegList[cnt].Size; offset++)
                EEPROM_WriteNext((DataRegList[cnt].pData)[offset]);
        }

        retval = EEPROM_Handle(ReadData);
    }
    return retval;
}