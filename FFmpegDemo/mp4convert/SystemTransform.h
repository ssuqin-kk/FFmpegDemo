/** @file    SystemTransform.h
* @note    HANGZHOU Hikvison Software Co.,Ltd.All Right Reserved.
* @brief   SystemTransform header file
*
*  @version    V2.5.0
*  @author     heyuanqing
*  @date       2014/12/05
*  @note       增加详细信息输出回调
*
* @note
*
* @warning  Windows 32bit /Linux32 bit version
*/

#ifndef _SYSTEM_TRANSFORM_H_
#define _SYSTEM_TRANSFORM_H_

#ifdef WIN32
#if defined(_WINDLL)
#define SYSTRANS_API  __declspec(dllexport) 
#else 
#define SYSTRANS_API  __declspec(dllimport) 
#endif
#else
#ifndef __stdcall
#define __stdcall
#endif

#ifndef SYSTRANS_API
#define  SYSTRANS_API
#endif
#endif

#define SWITCH_BY_FILESIZE      1       //暂不支持
#define SWITCH_BY_TIME          2

#define SUBNAME_BY_INDEX        1       //暂不支持
#define SUBNAME_BY_GLOBALTIME   2

/************************************************************************
* 状态码定义
************************************************************************/
#define SYSTRANS_OK             0x00000000
#define SYSTRANS_E_HANDLE       0x80000000  //转换句柄错误
#define SYSTRANS_E_SUPPORT      0x80000001  //类型不支持
#define SYSTRANS_E_RESOURCE     0x80000002  //资源申请或释放错误
#define SYSTRANS_E_PARA         0x80000003  //参数错误
#define SYSTRANS_E_PRECONDITION 0x80000004  //前置条件未满足，调用顺序
#define SYSTRANS_E_OVERFLOW     0x80000005  //缓存溢出
#define SYSTRANS_E_STOP         0x80000006  //停止状态
#define SYSTRANS_E_FILE         0x80000007  //文件错误
#define SYSTRANS_E_MAX_HANDLE   0x80000008  //最大路数限制
#define SYSTRANS_E_OTHER        0x800000ff  //其他错误

/************************************************************************
* 输出数据类型定义
************************************************************************/
#define TRANS_SYSHEAD               1   //系统头数据
#define TRANS_STREAMDATA            2   //视频流数据（包括复合流和音视频分开的视频流数据）
#define TRANS_AUDIOSTREAMDATA       3   //音频流数据
#define TRANS_PRIVTSTREAMDATA       4   //私有数据类型
#define TRANS_DECODEPARAM           5   //解码参数类型

/************************************************************************
* 数据结构定义
************************************************************************/
/**	@enum	 SYSTEM_TYPE
*	@brief   封装格式类型
*	@note
*/
typedef enum SYSTEM_TYPE
{
	TRANS_SYSTEM_NULL = 0x0,
	TRANS_SYSTEM_HIK = 0x1,  //海康文件层，可用于传输和存储
	TRANS_SYSTEM_MPEG2_PS = 0x2,  //PS文件层，主要用于存储，也可用于传输
	TRANS_SYSTEM_MPEG2_TS = 0x3,  //TS文件层，主要用于传输，也可用于存储
	TRANS_SYSTEM_RTP = 0x4,  //RTP文件层，用于传输
	TRANS_SYSTEM_MPEG4 = 0x5,  //MPEG4文件层，用于存储
	TRANS_SYSTEM_AVI = 0x7,  //AVI文件层，用于存储
	TRANS_SYSTEM_GB_PS = 0x8,  //国标PS，主要用于国标场合
	TRANS_SYSTEM_RAW = 0x10,
}SYSTEM_TYPE;

/**	@enum	  DATA_TYPE
*	@brief    输入数据类型
*	@note	  通常情况使用复合流数据类型，特殊情况下需要音视频数据分开，且需要设置参数
*/
typedef enum DATA_TYPE
{
	MULTI_DATA,             //复合流数据
	VIDEO_DATA,             //视频流数据
	AUDIO_DATA,             //音频流数据
	PRIVATE_DATA,           //私有数据
	VIDEO_PARA,             //视频流打包参数,定义见HK_VIDEO_PACK_PARA
	AUDIO_PARA,             //音频流打包参数,定义见HK_AUDIO_PACK_PARA
	PRIVATE_PARA            //私有流打包参数,定义见HK_PRIVATE_PACK_PARA
}DATA_TYPE;

/** @enum   ST_FRAME_TYPE
*  @brief  帧类型
*  @note
*/
typedef enum _ST_FRAME_TYPE_
{
	ST_VIDEO_BFRAME = 0,
	ST_VIDEO_PFRAME = 1,
	ST_VIDEO_EFRAME = 2,
	ST_VIDEO_IFRAME = 3,
	ST_AUDIO_FRAME = 4,
	ST_PRIVA_FRAME = 5,
}ST_FRAME_TYPE;

/**	@enum	 ST_PROTOCOL_TYPE
*	@brief   会话协议类型
*	@note
*/
typedef enum _ST_PROTOCOL_TYPE_
{
	ST_PROTOCOL_RTSP = 1,       //RTSP协议
	ST_PROTOCOL_HIK = 2        //海康私有协议
}ST_PROTOCOL_TYPE;

/**	@enum	 ST_SESSION_INFO_TYPE
*	@brief   会话信息类型
*	@note
*/
typedef enum _ST_SESSION_INFO_TYPE_
{
	ST_SESSION_INFO_SDP = 1,    //SDP信息
	ST_HIK_HEAD = 2     //海康40字节头
}ST_SESSION_INFO_TYPE;

/**	@enum	 ST_ENCRYPT_TYPE
*	@brief   加密类型
*	@note
*/
typedef enum _ST_ENCRYPT_TYPE_
{
	ST_ENCRYPT_NONE = 0,       //不加密
	ST_ENCRYPT_AES = 1        //AES 加密
}ST_ENCRYPT_TYPE;

/** @enum   ST_MARKBIT
*  @brief  标记位类型
*  @note
*/
typedef enum _ST_MARKBIT_TYPE_
{
	ST_UNMARK = 0,    //没有标记
	ST_FRAME_END = 1,    //帧结束标记(目标封装为PS、TS、RTP时有效）
	ST_NEW_FILE = 2,    //新文件标记(目标封装为MP4有效)
}ST_MARKBIT_TYPE;

/**	@struct    SYS_TRANS_PARA
*	@brief     转封装参数结构
*	@note	   用于SYSTRANS_Create接口，只支持海康私有协议
*/
typedef struct SYS_TRANS_PARA
{
	unsigned char*  pSrcInfo;       //海康设备出的媒体信息头
	unsigned int    dwSrcInfoLen;   //当前固定为40
	SYSTEM_TYPE     enTgtType;
	unsigned int    dwTgtPackSize;  //如果目标为RTP，PS/TS等封装格式时，设定每个包大小的上限。如果目标为AVI，设定最大帧长
} SYS_TRANS_PARA;

/**	@struct    ST_SESSION_PARA
*	@brief     转封装会话信息参数
*	@note	   用于SYSTRANS_CreateEx，增加支持RTSP协议的SDP信息
*/
typedef struct _ST_SESSION_PARA_
{
	ST_SESSION_INFO_TYPE  nSessionInfoType;   //会话信息类型，支持海康媒体头、SDP信息
	unsigned int          nSessionInfoLen;    //会话信息长度
	unsigned char*        pSessionInfoData;   //会话信息数据
	SYSTEM_TYPE           eTgtType;           //目标封装类型
	unsigned int          nTgtPackSize;       //如果目标为RTP，PS/TS等封装格式时，设定每个包大小的上限
} ST_SESSION_PARA;

/**	@struct    AUTO_SWITCH_PARA
*	@brief     自动切换参数
*	@note
*/
typedef struct AUTO_SWITCH_PARA
{
	unsigned int    dwSwitchFlag;       //SWITCH_BY_TIME：通过时间来切换
	unsigned int    dwSwitchValue;      //时间以分钟为单位
	unsigned int    dwSubNameFlag;      //SUBNAME_BY_GLOBALTIME: 文件名以全局时间区分
	char            szMajorName[128];   //如szMajorName = c:\test,切换文件后的名称 = c:\test_年月日时分秒.mp4
} AUTO_SWITCH_PARA;

/**	@struct   OUTPUTDATA_INFO
*	@brief    输出数据定义
*	@note
*/
typedef struct OUTPUTDATA_INFO
{
	unsigned char*  pData;              //回调数据缓存，该指针请勿用于异步的传递
	unsigned int    dwDataLen;          //回掉数据长度
	unsigned int    dwDataType;         //数据类型，如TRANS_SYSHEAD,TRANS_STREAMDATA
} OUTPUTDATA_INFO;

/** @struct   DETAIL_DATA_INFO
*  @brief    详细数据信息
*  @note
*/
typedef struct _DETAIL_DATA_INFO_
{
	unsigned char*  pData;             //回调数据缓存，该指针请勿用于异步的传递
	unsigned int    nDataLen;          //回掉数据长度
	unsigned short  nDataType;         //输出数据类型，见宏定义
	unsigned short  nFrameType;        //帧类型，见枚举类型
	unsigned int    nTimeStamp;        //时间戳
	unsigned int    nTimeStampHigh;    //时间戳高位,用于时间戳超过四字节的格式
	unsigned short  nMarkbit;          //标记(目前支持帧结束、新建文件两种标记,参见ST_MARKBIT_TYPE）
	unsigned short  nVersion;          //结构体版本号
	unsigned int    reserved[26];      //保留字段，用于扩展
}DETAIL_DATA_INFO;

/**	@struct   HK_SYSTEM_TIME
*	@brief    系统时间
*	@note
*/
typedef struct _HK_SYSTEM_TIME_
{
	unsigned int           dwYear;          //年
	unsigned int           dwMonth;         //月
	unsigned int           dwDay;           //日
	unsigned int           dwHour;          //小时
	unsigned int           dwMinute;        //分
	unsigned int           dwSecond;        //秒
	unsigned int           dwMilliSecond;   //毫秒
	unsigned int           dwReserved;      //保留
} HK_SYSTEM_TIME;

/**	@struct   HK_VIDEO_PACK_PARA
*	@brief    视频流打包参数
*	@note
*/
typedef struct _HK_VIDEO_PACK_PARA_
{
	unsigned int   dwFrameNum;             //帧号
	unsigned int   dwTimeStamp;            //时间戳
	float          fFrameRate;             //帧率
	unsigned int   dwReserved;             //保留
	HK_SYSTEM_TIME stSysTime;              //全局时间    
} HK_VIDEO_PACK_PARA;

/**	@struct    HK_AUDIO_PACK_PARA
*	@brief     音频打包参数
*	@note
*/
typedef struct _HK_AUDIO_PACK_PARA_
{
	unsigned int       dwChannels;         //声道数
	unsigned int	   dwBitsPerSample;    //位样率
	unsigned int       dwSampleRate;       //采样率
	unsigned int       dwBitRate;          //比特率
	unsigned int       dwTimeStamp;        //时间戳
	unsigned int       dwReserved[3];      //保留
} HK_AUDIO_PACK_PARA;

/**	@struct    HK_PRIVATE_PACK_PARA
*	@brief     私有数据打包参数
*	@note
*/
typedef struct _HK_PRIVATE_PACK_PARA_
{
	unsigned int          dwPrivateType;    //私有类型
	unsigned int          dwDataType;       //数据类型
	unsigned int          dwSycVideoFrame;  //同步视频帧
	unsigned int          dwReserved;       //保留
	unsigned int          dwTimeStamp;      //时间戳
	unsigned int          dwReserved1[2];   //保留
} HK_PRIVATE_PACK_PARA;


#ifdef __cplusplus
extern "C" {
#endif
	/************************************************************************
	* 函数名：SYSTRANS_Create
	* 功能：  通过源和目标的封装类型来创建封装格式转换句柄
	* 参数：  phTrans	        - 返回的句柄
	*		 pstTransInfo       - 转换信息数据指针
	* 返回值：状态码
	************************************************************************/
	SYSTRANS_API int __stdcall SYSTRANS_Create(void** phTrans, SYS_TRANS_PARA* pstTransInfo);

	/************************************************************************
	* 函数名：SYSTRANS_Start
	* 功能：  开始封装格式转换
	* 参数：  hTrans	            - 转换句柄
	*        szSrcPath          - 源文件路径，如果置NULL，表明为流
	*        szTgtPath          - 目标文件路径，如果置NULL，表明为流
	* 说明：  源文件为TS不支持文件模式，可由外部读文件以流的模式输入
	* 返回值：状态码
	************************************************************************/
	SYSTRANS_API int __stdcall SYSTRANS_Start(void* hTrans, const char* szSrcPath, const char* szTgtPath);

	/************************************************************************
	* 函数名：SYSTRANS_AutoSwitch
	* 功能：  目标为文件时，自动切换存储文件
	* 参数：  hTrans	        - 转换句柄
	*		  pstPara       - 自动切换文件的参数结构指针
	* 说明：  仅支持设置一次，设置多次返回不支持
	* 返回值：状态码
	************************************************************************/
	SYSTRANS_API int __stdcall SYSTRANS_AutoSwitch(void* hTrans, AUTO_SWITCH_PARA* pstPara);

	/************************************************************************
	* 函数名：SYSTRANS_ManualSwitch
	* 功能：  目标为文件时，手动切换存储文件
	* 参数：  hTrans	        - 转换句柄
	*		 szTgtPath      - 下一存储文件的路径
	* 返回值：状态码
	************************************************************************/
	SYSTRANS_API int __stdcall SYSTRANS_ManualSwitch(void* hTrans, const char* szTgtPath);

	/************************************************************************
	* 函数名：SYSTRANS_InputData
	* 功能：  源为流模式，塞入数据
	* 参数：  hTrans	- 转换句柄
	*		  pData		        - 源，流数据指针
	*		  dwDataLen         - 流数据大小
	*		  enType	        - 码流类型，暂未使用，统一用MULTI_DATA
	* 返回值：状态码
	************************************************************************/
	SYSTRANS_API int __stdcall SYSTRANS_InputData(void* hTrans, DATA_TYPE enType, unsigned char* pData, unsigned int dwDataLen);

	/************************************************************************
	* 函数名：SYSTRANS_GetTransPercent
	* 功能：  转文件模式时，获得转换百分比，暂时只支持源是HIK，PS和MPEG4
	* 参数：  hTrans	        - 转换句柄
	*		 pdwPercent     - 转换百分比
	* 返回值：状态码
	************************************************************************/

	SYSTRANS_API int __stdcall SYSTRANS_GetTransPercent(void* hTrans, unsigned int* pdwPercent);

	/************************************************************************
	* 函数名：SYSTRANS_RegisterOutputDataCallBack
	* 功能：  目标为流模式，注册转换后数据回调
	* 参数：  hTrans				    - 转换句柄
	*		 OutputDataCallBack     - 函数指针
	*		 dwUser				    - 用户数据
	* 返回值：状态码
	* 说明：  3GPP不支持回调
	************************************************************************/
	SYSTRANS_API int __stdcall SYSTRANS_RegisterOutputDataCallBack(void* hTrans, void(__stdcall * pfnOutputDataCallBack)(OUTPUTDATA_INFO* pDataInfo, void* pUser), void* pUser);


	/************************************************************************
	* 函数名：SYSTRANS_RegisterOutputDataCallBackEx
	* 功能：  目标为流模式，注册转换后数据回调
	* 参数：  hTrans				    - 转换句柄
	*		 OutputDataCallBack	    - 函数指针
	*		 dwUser				    - 用户数据
	* 返回值：状态码
	* 说明：  该接口与SYSTRANS_RegisterOutputDataCallBack功能一致，为兼容
	*           V2.3.0.6之前的版本，保留此接口，建议使用SYSTRANS_RegisterOutputDataCallBack
	************************************************************************/
	SYSTRANS_API int __stdcall SYSTRANS_RegisterOutputDataCallBackEx(void* hTrans, void(__stdcall * OutputDataCallBack)(OUTPUTDATA_INFO* pDataInfo, void* pUser), void* pUser);


	/************************************************************************
	* 函数名：SYSTRANS_RegisterDetailDataCallBack
	* 功能：  目标为流模式，注册转换后数据回调
	* 参数：  hTrans             - 转换句柄
	*        pfDetailCbf        - 函数指针
	*        dwUser             - 用户数据
	* 返回值：状态码
	* 说明：  输出数据的详细信息
	************************************************************************/
	SYSTRANS_API int __stdcall SYSTRANS_RegisterDetailDataCallBack(void* hTrans, void(__stdcall * pfDetailCbf)(DETAIL_DATA_INFO* pDataInfo, void* pUser), void* pUser);

	/************************************************************************
	* 函数名：SYSTRANS_Stop
	* 功能：  停止转换
	* 参数：  hTrans	 - 转换句柄
	* 返回值：状态码
	************************************************************************/
	SYSTRANS_API int __stdcall SYSTRANS_Stop(void* hTrans);

	/************************************************************************
	* 函数名：SYSTRANS_Release
	* 功能：  释放转换句柄
	* 参数：  hTrans	        - 转换句柄
	* 返回值：状态码
	************************************************************************/
	SYSTRANS_API int __stdcall SYSTRANS_Release(void* hTrans);

	/************************************************************************
	* 函数名：SYSTRANS_CreateEx
	* 功能：  SDP信息创建转封装库句柄
	* 参数：  hTrans             - 转换句柄
	* 参数：  eType              - 协议类型
	* 参数：  pstInfo            - 转封装会话参数
	* 返回值：状态码
	* 说明：  该接口是SYSTRANS_Create的扩展，增加对使用SDP信息创建句柄的支持
	************************************************************************/
	SYSTRANS_API int __stdcall SYSTRANS_CreateEx(void** phTrans, ST_PROTOCOL_TYPE eType, ST_SESSION_PARA* pstInfo);

	/************************************************************************
	* 函数名：SYSTRANS_SetGlobalTime
	* 功能：  设置全局时间
	* 参数：  hTrans	            - 转换句柄
	* 参数：  pstGlobalTime	    - 全局时间
	* 返回值：状态码
	************************************************************************/
	SYSTRANS_API int __stdcall SYSTRANS_SetGlobalTime(void* hTrans, HK_SYSTEM_TIME* pstGlobalTime);

	/************************************************************************
	* 函数名：SYSTRANS_SetEncryptKey
	* 功能：  设置密钥
	* 参数：  hTrans             - 转换句柄
	* 参数：  eType              - 加密类型
	* 参数：  pKey               - 密钥缓冲区
	* 参数：  nKeyLen            - 密钥长度，单位为Bit
	* 返回值：状态码
	************************************************************************/
	SYSTRANS_API int __stdcall SYSTRANS_SetEncryptKey(void* hTrans, ST_ENCRYPT_TYPE eType, char* pKey, unsigned int nKeyLen);

	/************************************************************************
	* 函数名：SYSTRANS_GetVersion
	* 功能：  获取版本号
	* 参数：  无
	* 返回值：版本号
	************************************************************************/
	SYSTRANS_API int __stdcall SYSTRANS_GetVersion();

#ifdef __cplusplus
}
#endif

#endif //_SYSTEM_TRANSFORM_H_