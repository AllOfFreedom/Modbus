/******************************************************************************/
/** 模块名称：Modbus通讯                                                     **/
/** 文件名称：mbascii.c                                                      **/
/** 版    本：V1.0.0                                                         **/
/** 简    介：用于实现Modbus ASCII总线协议栈ADU的封装                        **/
/**           声明和定义Modbus总线在串行链路上的实现的相关属性和方法         **/
/**           对串行链路ASCII方式进行具体描述                                **/
/**                                                                          **/
/* 一个典型的Modbus数据帧有如下部分组成：                                     */
/* <------------------- MODBUS串行链路应用数据单元（ADU） ----------------->  */
/*              <------ MODBUS简单协议数据单元（PDU） ------->                */
/*  +-----------+---------------+----------------------------+-------------+  */
/*  |  地址域   |    功能码     | 数据域                     | CRC/LRC     |  */
/*  +-----------+---------------+----------------------------+-------------+  */
/*  对于ASCII编码则要在前面加上起始符“：”（0x3A）以及结束符“回车符”（0x0D）和 */
/*  “换行符”（0x0A）                                                         **/
/**--------------------------------------------------------------------------**/
/** 修改记录：                                                               **/
/**     版本      日期              作者              说明                   **/
/**     V1.0.0  2018-09-11          尹家军            创建文件               **/
/**                                                                          **/
/******************************************************************************/

#include "string.h"
#include "mbascii.h"

/*LRC校验函数*/
static uint8_t GenerateLRCCheckCode(uint8_t *usData, uint16_t usLength);
/*将0到F的16进制数转化为ASCII码*/
static uint8_t HexToASCII(uint8_t hData);
/*将ASCII码转化为0到F的16进制数*/
static uint8_t ASCIIToHex(uint8_t aData);
/*将ASCII消息列转为16进制消息列*/
static ModbusStatus CovertAsciiMsgToHexMsg(uint8_t *aMsg, uint8_t *hMsg, uint16_t aLen);
/*将16进制消息列转换为ASCII消息列*/
static ModbusStatus CovertHexMsgToAsciiMsg(uint8_t *hMsg, uint8_t *aMsg, uint16_t hLen);

/*生成读写从站的命令，应用于主站，含校验及起始结束符*/
uint16_t SyntheticReadWriteAsciiSlaveCommand(ObjAccessInfo slaveInfo, bool *statusList, uint16_t *registerList, uint8_t *commandBytes)
{
    uint8_t command[256];
    uint16_t bytesCount = GenerateReadWriteCommand(slaveInfo, statusList, registerList, command);
    uint8_t lcr = GenerateLRCCheckCode(command, bytesCount);
    command[bytesCount++] = lcr;

    uint8_t AsciiCommand[256];
    bool status = CovertHexMsgToAsciiMsg(command, AsciiCommand, bytesCount);
    if (status != MB_OK)
    {
        return 0;
    }

    uint16_t index = 0;
    commandBytes[index++] = 0x3A;
    for (int i = 0; i < bytesCount * 2; i++)
    {
        commandBytes[index++] = command[i];
    }
    commandBytes[index++] = 0x0D;
    commandBytes[index++] = 0x0A;

    return index;
}

/*生成应答主站的响应，应用于从站*/
uint16_t SyntheticAsciiSlaveAccessRespond(uint8_t *receivedMessage, bool *statusList, uint16_t *registerList, uint8_t *respondBytes)
{
    uint16_t respondLength = 0;
    uint8_t respond[256];
    uint16_t length = GenerateMasterAccessRespond(receivedMessage, statusList, registerList, respond);
    uint8_t lcr = GenerateLRCCheckCode(respond, length);
    respond[length++] = lcr;

    uint8_t AsciiCommand[256];
    bool status = CovertHexMsgToAsciiMsg(respond, AsciiCommand, length);
    if (status != MB_OK)
    {
        return 0;
    }

    respondBytes[respondLength++] = 0x3A;
    for (int i = 0; i < length * 2; i++)
    {
        respondBytes[respondLength++] = AsciiCommand[i];
    }
    respondBytes[respondLength++] = 0x0D;
    respondBytes[respondLength++] = 0x0A;
    
    return respondLength;
}

/*接收到的ASCII消息转换为16进制*/
bool CovertAsciiMessageToHex(uint8_t *aMsg, uint8_t *hMsg, uint16_t aLen)
{
    bool checkResult = CovertAsciiMsgToHexMsg(aMsg, hMsg, aLen);

    if (checkResult == MB_OK)
    {
        return true;
    }

    return false;
}

/*将16进制消息列转换为ASCII消息列*/
static ModbusStatus CovertHexMsgToAsciiMsg(uint8_t *hMsg, uint8_t *aMsg, uint16_t hLen)
{
    if (hLen < 1)
    {
        return OperationFail;
    }

    for (uint16_t i = 0; i < hLen; i++)
    {
        aMsg[2 * i] = HexToASCII(hMsg[i] >> 4);
        aMsg[2 * i + 1] = HexToASCII(hMsg[i] & 0x0F);
    }

    return MB_OK;
}

/*将ASCII消息列转为16进制消息列*/
static ModbusStatus CovertAsciiMsgToHexMsg(uint8_t *aMsg, uint8_t *hMsg, uint16_t aLen)
{
    if ((aLen < 2) || (aLen % 2 != 0))
    {
        return OperationFail;
    }

    uint8_t msb, lsb;
    for (uint16_t i = 0; i < aLen; i = i + 2)
    {
        msb = ASCIIToHex(aMsg[i]);
        lsb = ASCIIToHex(aMsg[i + 1]);

        if ((msb == 0xFF) || (lsb == 0xFF))
        {
            return OperationFail;
        }

        hMsg[i / 2] = (msb << 4) + lsb;
    }

    return MB_OK;
}

/*将0到F的16进制数转化为ASCII码*/
static uint8_t HexToASCII(uint8_t hData)
{
    uint8_t aData;

    if ((hData <= 0x9))
    {
        aData = hData + 0x30;
    }
    else if ((hData >= 0xA) && (hData <= 0xF))
    {
        aData = hData + 0x37;
    }
    else
    {
        aData = 0xFF;
    }

    return aData;
}

/*将ASCII码转化为0到F的16进制数*/
static uint8_t ASCIIToHex(uint8_t aData)
{
    uint8_t hData;

    if ((aData >= 0x30) && (aData <= 0x39))
    {
        hData = aData - 0x30;
    }
    else if ((aData >= 0x41) && (aData <= 0x46))
    {
        hData = aData - 0x37;
    }
    else if ((aData >= 0x61) && (aData <= 0x66))
    {
        hData = aData - 0x57;
    }
    else
    {
        hData = 0xFF;
    }

    return hData;
}

/*LRC校验函数*/
static uint8_t GenerateLRCCheckCode(uint8_t *usData, uint16_t usLength)
{
    uint8_t lrcResult = 0;
    uint8_t sum = 0;

    for (uint16_t i = 0; i < usLength; i++)
    {
        sum = sum + usData[i];
    }

    lrcResult = ~sum + 1;

    return lrcResult;
}

/*判断ASCII数据信息是否正确*/
bool CheckASCIIMessageIntegrity(uint8_t *usMsg, uint16_t usLength)
{
    bool checkResult;
    checkResult = (GenerateLRCCheckCode(usMsg, usLength) == 0x00) ? true : false;
    return checkResult;
}

/*********** (C) COPYRIGHT 1999-2018 Moonan Technology *********END OF FILE****/