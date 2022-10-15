#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <process.h>
#include <Winsock.h>
#include <stdlib.h>
#include <conio.h>
#include <time.h>
#pragma comment(lib, "ws2_32.lib")
#define MAXSIZE 500 //发送数据报文的最大长度
#define MAX_PACKAGE_NUM 1000
#define RECV_PORT_DEFAULT 10242
#define SEND_PORT_DEFAULT 10241

#define TIME_OUT 5 //5s
#define DST_IP "127.0.0.1"
#define TRANS_FILE_DEFAULT "transfer"
#define FILE_NAME_FORMAT "%s/%s.%d.txt"
#define FILE_DIR_DEFAULT "../DATA/"

#define METHOD_SYN_TEXT "SYN" //建立连接
#define METHOD_LEN_TEXT "LEN" //交换文件名和长度
#define METHOD_DATA_TEXT "DAT"//交换数据
#define METHOD_ACK_TEXT "ACK" //接收确认
#define METHOD_FIN_TEXT "FIN" //关闭连接

bool SUB_PROCESS = false;
char FILE_DIR[100];

struct Data {
	char control[4];
	char seq[8];
	char dat[MAXSIZE+2];
};
#define MAXPACK sizeof(Data)

struct Job {
	SOCKET socketSend;
	SOCKET socketRecv;
	unsigned long srcIp;
	unsigned long dstIp;
	unsigned short srcSendPort;
	unsigned short srcRecvPort;
	unsigned short dstRecvPort;
	bool connected;
} job;

FILE* file;
int proexit(int err) {
	printf("正在关闭客户端！\n");
	if (file != NULL) fclose(file);
	if (job.socketSend != NULL) closesocket(job.socketSend);
	if (job.socketRecv != NULL) closesocket(job.socketRecv);
	WSACleanup();
	Sleep(3000);
	exit(err);
}

const char* getWSAErrorText() {
	switch (WSAGetLastError())
	{
	case 0: return "Directly send error";
	case 10004: return "Interrupted function";//call 操作被终止
	case 10013: return "Permission denied";//c访问被拒绝
	case 10014: return "Bad address"; //c地址错误
	case 10022: return "Invalid argument"; //参数错误
	case 10024: return "Too many open files";// 打开太多的sockets
	case 10035: return "Resource temporarily unavailable"; // 没有可以获取的资料
	case 10036: return "Operation now in progress"; // 一个阻塞操作正在进行中
	case 10037: return "Operation already in progress";// 操作正在进行中
	case 10038: return "Socket operation on non-socket";//非法的socket对象在操作
	case 10039: return "Destination address required"; //目标地址错误
	case 10040: return "Message too long";//数据太长
	case 10041: return "Protocol wrong type for socket"; //协议类型错误
	case 10042: return "Bad protocol option";// 错误的协议选项
	case 10043: return "Protocol not supported"; //协议不被支持
	case 10044: return "Socket type not supported"; //socket类型不支持
	case 10045: return "Operation not supported"; //不支持该操作
	case 10046: return "Protocol family not supported";//协议族不支持
	case 10047: return "Address family not supported by protocol family";//使用的地址族不在支持之列
	case 10048: return "Address already in use"; //地址已经被使用
	case 10049: return "Cannot assign requested address";//地址设置失败
	case 10050: return "Network is down";//网络关闭
	case 10051: return "Network is unreachable"; //网络不可达
	case 10052: return "Network dropped connection on reset";//网络被重置
	case 10053: return "Software caused connection abort";//软件导致连接退出
	case 10054: return "connection reset by peer"; //连接被重置
	case 10055: return "No buffer space available"; //缓冲区不足
	case 10056: return "Socket is already connected";// socket已经连接
	case 10057: return "Socket is not connected";//socket没有连接
	case 10058: return "Cannot send after socket shutdown";//socket已经关闭
	case 10060: return "Connection timed out"; //超时
	case 10061: return "Connection refused"; //连接被拒绝
	case 10064: return "Host is down";//主机已关闭
	case 10065: return "No route to host";// 没有可达的路由
	case 10067: return "Too many processes";//进程太多
	case 10091: return "Network subsystem is unavailable";//网络子系统不可用
	case 10092: return "WINSOCK.DLL version out of range"; //winsock.dll版本超出范围
	case 10093: return "Successful WSAStartup not yet performed"; //没有成功执行WSAStartup
	case 10094: return "Graceful shutdown in progress";//
	case 11001: return "Host not found"; //主机没有找到
	case 11002: return "Non-authoritative host not found"; // 非授权的主机没有找到
	case 11003: return "This is a non-recoverable error";//这是个无法恢复的错误
	case 11004: return "Valid name, no data record of requested type";//请求的类型的名字或数据错误

	default:
		return "未知错误";
	}
}
bool initWSA() {
	//加载套接字库（必须）
	WORD wVersionRequested;
	WSADATA wsaData;
	//套接字加载时错误提示
	int err;
	//版本 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//加载 dll 文件 Scoket 库
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		//找不到 winsock.dll
		printf("加载 winsock 失败，错误代码为: %d\n", WSAGetLastError());
		return FALSE;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("不能找到正确的 winsock 版本\n");
		WSACleanup();
		return FALSE;
	}
	return true;
}

BOOL getSocket(int recvport)
{
	/*SOCKET* s = &job.socketRecv;
	*s = socket(AF_INET, SOCK_DGRAM, 0);
	if (INVALID_SOCKET == *s)
	{
		printf("创建接收套接字失败，错误代码为：%d\n", WSAGetLastError());
		return FALSE;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(recvport);
	serverAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(*s, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		printf("绑定接收套接字失败\n");
		return FALSE;
	}*/
	SOCKET* s = &job.socketSend;
	*s = socket(AF_INET, SOCK_DGRAM, 0);
	if (INVALID_SOCKET == *s)
	{
		printf("创建发送套接字失败，错误代码为：%d\n", WSAGetLastError());
		return FALSE;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(recvport);
	serverAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(*s, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		printf("绑定发送套接字失败\n");
		return FALSE;
	}
	return TRUE;
}


int unPackData(Data* Buffer, SOCKET s) {
	int len;
	char buf[MAXPACK+5] = { 0 };
	len = recv(s, buf, MAXPACK+5, 0);
	if (len < 0) {
		printf("接收数据失败 %s\n", getWSAErrorText());
		return -1;
	}
	//printf("\r接收数据：%s\n", buf);
	memcpy_s(Buffer, sizeof(Data), (void *)buf, len);
	Buffer->control[3] = '\0';
	Buffer->seq[7] = '\0';
	printf(">>>> %s %d [%d]\n", Buffer->control, atoi(Buffer->seq), len);
	return len;
}

int packData(SOCKET s,const char* method, int seq, char* data, int len) {
	Data Buffer;
	/*switch (method)
	{
	case METHOD_SYN:
		strcpy_s(Buffer.control, "SYN");
		break;
	case METHOD_LEN:
		strcpy_s(Buffer.control, "LEN");
		break;
	case METHOD_DATA:
		strcpy_s(Buffer.control, "DAT");
		break;
	case METHOD_ACK:
		strcpy_s(Buffer.control, "ACK");
		break;
	case METHOD_FIN:
		
		break;
	default:
		printf("未知打包方法！\n");
		return -1;
	}*/
	ZeroMemory(&Buffer, sizeof(Data));
	if (method == NULL) {
		printf("未知打包方法!\n");
		return -1;
	}
	memcpy_s(Buffer.control,4, method,4);
	char seqbuf[8] = { 0 };
	sprintf_s(seqbuf, "%07d", seq);
	memcpy_s(Buffer.seq, sizeof(Buffer.seq), seqbuf, 8);
	Buffer.seq[7] = '_';
	/*_itoa_s(seq, , 10);
	for (int i = 0; i < sizeof(Buffer.seq); i++) {
		if (Buffer.seq[i] == '\0') {
			Buffer.seq[i] = '0';
		}
	}*/
	if (data != NULL) {
		memcpy_s(Buffer.dat, MAXSIZE,data,len);
	}
	else {
		len = 0;
		memcpy_s(Buffer.dat, MAXSIZE, " \0", 1);
	}
	printf("<<<< %s %s [%d]\n", Buffer.control, seqbuf, len);
	Buffer.control[3] = '_';
	int size = len + MAXPACK - MAXSIZE;
	len = send(s, (char *)&Buffer, size, 0);
	if (len < 0) {
		printf("发送数据失败 %s\n", getWSAErrorText());
		return -1;
	}
	//printf("\r发送数据大小：%d [%d]\n", size, len);
	//printf("\r发送原始数据：%s\n", (char *)&Buffer);
	return len;
	
}

//返回1为超时，需传入last并自动更改
int timer(time_t* lastt) {
	time_t t = time(NULL);
	if (*lastt == 0) {
		*lastt = t;
		return 0;
	}
	else {
		if (t - *lastt > TIME_OUT) {
			*lastt = 0;
			return 1;
		}
		printf("还剩 %lld s\n",TIME_OUT - t + *lastt+1);
		return 0;
	}
}
struct time {
	SOCKET as;
	char* data;
	const char* method;
	int seq;
	//int len;
	int* stop;
	int readSize;
	bool* cancel;
	int* targetseq;
	time(SOCKET s, char* b, const char* m,int seqq,int* st, int reS,bool* cancell,int *targets) {
		as = s;
		data = b;
		method = m;
		seq = seqq;
		//len = l;
		targetseq = targets;
		cancel = cancell;
		stop = st;
		readSize = reS;
	}
};
typedef struct time TIME;

void createSubProcess(int port,const WCHAR* exefile) {
	STARTUPINFOW startupInfo = { 0 };
	startupInfo.cb = sizeof(startupInfo);
	PROCESS_INFORMATION processInfo = { 0 };
	WCHAR cmd[1024] = { 0 };
	//char szStr[512] = "";
	//sprintf_s(szStr, "%p",(void *) NULL);
	swprintf_s(cmd, L"PORT=%d", port);
	//MultiByteToWideChar(CP_ACP, 0, szStr, strlen(szStr) + 1, cmd, sizeof(cmd) / sizeof(cmd[0]));
	if (!CreateProcess(exefile, cmd, 0, 0, true, CREATE_NEW_CONSOLE, 0, 0, &startupInfo, &processInfo))
	{
		printf("CreateProcess failed\n");
	}
	else
	{
		SUB_PROCESS = true;
		printf("CreateProcess sucessed\n");
	}
}

void init(int argc, char* argv[],const WCHAR* exefile) {
	printf("初始化传输服务中...\n");
	for (int i = 0; i < argc; i++) {
		printf("命令行[%d]: %s\n", i, argv[i]);
	}
	int port = SEND_PORT_DEFAULT;
	bool subProcess = false;
	job.srcIp = inet_addr("0.0.0.0");
	if (argc == 1) {
		if (sscanf_s(argv[0], "PORT=%d", &port) == 1) {
			subProcess = true;
		}
	}
	//printf("接收套接字将运行在端口：%d\n", port);
	printf("套接字将运行在端口：%d\n", port);

	sprintf_s(FILE_DIR, "../DATA_%d", port);
	printf("默认文件保存路径：%s\n", FILE_DIR);
	job.srcSendPort = port;
	job.connected = false;
	//job.srcRecvPort = port-1;
	if (!initWSA()) proexit(1);
	if (!getSocket(port)) proexit(1);
	if (!subProcess) {
		createSubProcess(port - 2,exefile);
	}

	char title[100] = { 0 };
	sprintf_s(title, "%d", port);
	SetConsoleTitleA(title); //设置一个新标题

	job.dstIp = inet_addr(DST_IP);

	printf("初始化完成\n");
}

int PASSIVE = true;
int startRecv = false;
void connectPair(int port, int passive) {
	if (!startRecv) {
		printf("请先使用S命令选择保存文件路径！\n");
		printf("WBXFileTransfer> ");
		return;
	}
	if (job.connected) {
		printf("已经成功连接，无需重复连接！\n");
		printf("WBXFileTransfer> ");
		return;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_addr.S_un.S_addr = job.dstIp;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_family = AF_INET;
	in_addr d;
	d.S_un.S_addr = job.dstIp;
	printf("正在连接主机 %s:%d\n", inet_ntoa(d),port);
	if (connect(job.socketSend, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		printf("连接主机失败");
		printf("：%s\n", getWSAErrorText());
		proexit(1);
	}
	job.dstRecvPort = port;
	int recvSize = 0;
	PASSIVE = passive;
	recvSize = packData(job.socketSend, METHOD_SYN_TEXT, job.srcSendPort, inet_ntoa(d), 17);
	if (recvSize <= 0) {
		printf("连接失败！%s\n", getWSAErrorText());
		proexit(1);
	}
	if (passive) {
		printf("连接成功：%s:%d\n", inet_ntoa(serverAddr.sin_addr), job.dstRecvPort);
		char title[100] = { 0 };
		sprintf_s(title, 100, "%d 已连接至%s:%d", job.srcSendPort, inet_ntoa(serverAddr.sin_addr), ntohs(serverAddr.sin_port));
		SetConsoleTitleA(title);
		job.connected = true;

		//printf("WBXFileTransfer> ");
	}
	
}

unsigned int __stdcall acceptPair(void* param) {
	SOCKET as = job.socketSend;
	int recvSize = 0;
	Data data;
	printf("正后台等待主机连接，您也可以主动连接\n");
	startRecv = true;
	recvSize = unPackData(&data, as);
	if (recvSize <= 0) {
		printf("等待主机连接失败！%s\n", getWSAErrorText());
		proexit(1);
	}
	else if (_strnicmp(METHOD_SYN_TEXT, data.control, 3)) {
		printf("等待主机连接失败！接收到非法字符\n");
		proexit(1);
	}
	//job.connected = true;
	printf("收到主机连接请求 %s:%s\n", data.dat,data.seq);
	if(PASSIVE)	connectPair(atoi(data.seq),true);
	else {
		printf("连接成功：%s:%s\n", data.dat, data.seq);
		char title[100] = { 0 };
		job.dstRecvPort = atoi(data.seq);

		in_addr d;
		d.S_un.S_addr = job.dstIp;
		sprintf_s(title, 100, "%d 已连接至%s:%d", job.srcSendPort, inet_ntoa(d), job.dstRecvPort);
		SetConsoleTitleA(title);
		job.connected = true;
		printf("WBXFileTransfer> ");
	}
	return 0;
}

void waitForConnect() {
	if (startRecv) return;
	//printf("WBXFileTransfer> ");
	HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, &acceptPair, NULL, 0, 0);
	if (hThread != NULL) CloseHandle(hThread);
}

long getFileSize(FILE* f) {
	long file_size = -1;
	long cur_offset = ftell(f);	// 获取当前偏移位置
	if (cur_offset == -1) {
		printf("ftell failed\n");
		return -1;
	}
	if (fseek(f, 0, SEEK_END) != 0) {	// 移动文件指针到文件末尾
		printf("fseek failed\n");
		return -1;
	}
	file_size = ftell(f);	// 获取此时偏移值，即文件大小
	if (file_size == -1) {
		printf("ftell failed\n");
	}
	if (fseek(f, cur_offset, SEEK_SET) != 0) {	// 将文件指针恢复初始位置
		printf("fseek failed\n");
		return -1;
	}
	return file_size;
}