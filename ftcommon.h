#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include <process.h>
#include <Winsock.h>
#include <stdlib.h>
#include <conio.h>
#include <time.h>
#pragma comment(lib, "ws2_32.lib")
#define MAXSIZE 500 //�������ݱ��ĵ���󳤶�
#define MAX_PACKAGE_NUM 1000
#define RECV_PORT_DEFAULT 10242
#define SEND_PORT_DEFAULT 10241

#define TIME_OUT 5 //5s
#define DST_IP "127.0.0.1"
#define TRANS_FILE_DEFAULT "transfer"
#define FILE_NAME_FORMAT "%s/%s.%d.txt"
#define FILE_DIR_DEFAULT "../DATA/"

#define METHOD_SYN_TEXT "SYN" //��������
#define METHOD_LEN_TEXT "LEN" //�����ļ����ͳ���
#define METHOD_DATA_TEXT "DAT"//��������
#define METHOD_ACK_TEXT "ACK" //����ȷ��
#define METHOD_FIN_TEXT "FIN" //�ر�����

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
	printf("���ڹرտͻ��ˣ�\n");
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
	case 10004: return "Interrupted function";//call ��������ֹ
	case 10013: return "Permission denied";//c���ʱ��ܾ�
	case 10014: return "Bad address"; //c��ַ����
	case 10022: return "Invalid argument"; //��������
	case 10024: return "Too many open files";// ��̫���sockets
	case 10035: return "Resource temporarily unavailable"; // û�п��Ի�ȡ������
	case 10036: return "Operation now in progress"; // һ�������������ڽ�����
	case 10037: return "Operation already in progress";// �������ڽ�����
	case 10038: return "Socket operation on non-socket";//�Ƿ���socket�����ڲ���
	case 10039: return "Destination address required"; //Ŀ���ַ����
	case 10040: return "Message too long";//����̫��
	case 10041: return "Protocol wrong type for socket"; //Э�����ʹ���
	case 10042: return "Bad protocol option";// �����Э��ѡ��
	case 10043: return "Protocol not supported"; //Э�鲻��֧��
	case 10044: return "Socket type not supported"; //socket���Ͳ�֧��
	case 10045: return "Operation not supported"; //��֧�ָò���
	case 10046: return "Protocol family not supported";//Э���岻֧��
	case 10047: return "Address family not supported by protocol family";//ʹ�õĵ�ַ�岻��֧��֮��
	case 10048: return "Address already in use"; //��ַ�Ѿ���ʹ��
	case 10049: return "Cannot assign requested address";//��ַ����ʧ��
	case 10050: return "Network is down";//����ر�
	case 10051: return "Network is unreachable"; //���粻�ɴ�
	case 10052: return "Network dropped connection on reset";//���类����
	case 10053: return "Software caused connection abort";//������������˳�
	case 10054: return "connection reset by peer"; //���ӱ�����
	case 10055: return "No buffer space available"; //����������
	case 10056: return "Socket is already connected";// socket�Ѿ�����
	case 10057: return "Socket is not connected";//socketû������
	case 10058: return "Cannot send after socket shutdown";//socket�Ѿ��ر�
	case 10060: return "Connection timed out"; //��ʱ
	case 10061: return "Connection refused"; //���ӱ��ܾ�
	case 10064: return "Host is down";//�����ѹر�
	case 10065: return "No route to host";// û�пɴ��·��
	case 10067: return "Too many processes";//����̫��
	case 10091: return "Network subsystem is unavailable";//������ϵͳ������
	case 10092: return "WINSOCK.DLL version out of range"; //winsock.dll�汾������Χ
	case 10093: return "Successful WSAStartup not yet performed"; //û�гɹ�ִ��WSAStartup
	case 10094: return "Graceful shutdown in progress";//
	case 11001: return "Host not found"; //����û���ҵ�
	case 11002: return "Non-authoritative host not found"; // ����Ȩ������û���ҵ�
	case 11003: return "This is a non-recoverable error";//���Ǹ��޷��ָ��Ĵ���
	case 11004: return "Valid name, no data record of requested type";//��������͵����ֻ����ݴ���

	default:
		return "δ֪����";
	}
}
bool initWSA() {
	//�����׽��ֿ⣨���룩
	WORD wVersionRequested;
	WSADATA wsaData;
	//�׽��ּ���ʱ������ʾ
	int err;
	//�汾 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//���� dll �ļ� Scoket ��
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		//�Ҳ��� winsock.dll
		printf("���� winsock ʧ�ܣ��������Ϊ: %d\n", WSAGetLastError());
		return FALSE;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("�����ҵ���ȷ�� winsock �汾\n");
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
		printf("���������׽���ʧ�ܣ��������Ϊ��%d\n", WSAGetLastError());
		return FALSE;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(recvport);
	serverAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(*s, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		printf("�󶨽����׽���ʧ��\n");
		return FALSE;
	}*/
	SOCKET* s = &job.socketSend;
	*s = socket(AF_INET, SOCK_DGRAM, 0);
	if (INVALID_SOCKET == *s)
	{
		printf("���������׽���ʧ�ܣ��������Ϊ��%d\n", WSAGetLastError());
		return FALSE;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(recvport);
	serverAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(*s, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR)) == SOCKET_ERROR)
	{
		printf("�󶨷����׽���ʧ��\n");
		return FALSE;
	}
	return TRUE;
}


int unPackData(Data* Buffer, SOCKET s) {
	int len;
	char buf[MAXPACK+5] = { 0 };
	len = recv(s, buf, MAXPACK+5, 0);
	if (len < 0) {
		printf("��������ʧ�� %s\n", getWSAErrorText());
		return -1;
	}
	//printf("\r�������ݣ�%s\n", buf);
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
		printf("δ֪���������\n");
		return -1;
	}*/
	ZeroMemory(&Buffer, sizeof(Data));
	if (method == NULL) {
		printf("δ֪�������!\n");
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
		printf("��������ʧ�� %s\n", getWSAErrorText());
		return -1;
	}
	//printf("\r�������ݴ�С��%d [%d]\n", size, len);
	//printf("\r����ԭʼ���ݣ�%s\n", (char *)&Buffer);
	return len;
	
}

//����1Ϊ��ʱ���贫��last���Զ�����
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
		printf("��ʣ %lld s\n",TIME_OUT - t + *lastt+1);
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
	printf("��ʼ�����������...\n");
	for (int i = 0; i < argc; i++) {
		printf("������[%d]: %s\n", i, argv[i]);
	}
	int port = SEND_PORT_DEFAULT;
	bool subProcess = false;
	job.srcIp = inet_addr("0.0.0.0");
	if (argc == 1) {
		if (sscanf_s(argv[0], "PORT=%d", &port) == 1) {
			subProcess = true;
		}
	}
	//printf("�����׽��ֽ������ڶ˿ڣ�%d\n", port);
	printf("�׽��ֽ������ڶ˿ڣ�%d\n", port);

	sprintf_s(FILE_DIR, "../DATA_%d", port);
	printf("Ĭ���ļ�����·����%s\n", FILE_DIR);
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
	SetConsoleTitleA(title); //����һ���±���

	job.dstIp = inet_addr(DST_IP);

	printf("��ʼ�����\n");
}

int PASSIVE = true;
int startRecv = false;
void connectPair(int port, int passive) {
	if (!startRecv) {
		printf("����ʹ��S����ѡ�񱣴��ļ�·����\n");
		printf("WBXFileTransfer> ");
		return;
	}
	if (job.connected) {
		printf("�Ѿ��ɹ����ӣ������ظ����ӣ�\n");
		printf("WBXFileTransfer> ");
		return;
	}
	sockaddr_in serverAddr;
	serverAddr.sin_addr.S_un.S_addr = job.dstIp;
	serverAddr.sin_port = htons(port);
	serverAddr.sin_family = AF_INET;
	in_addr d;
	d.S_un.S_addr = job.dstIp;
	printf("������������ %s:%d\n", inet_ntoa(d),port);
	if (connect(job.socketSend, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
	{
		printf("��������ʧ��");
		printf("��%s\n", getWSAErrorText());
		proexit(1);
	}
	job.dstRecvPort = port;
	int recvSize = 0;
	PASSIVE = passive;
	recvSize = packData(job.socketSend, METHOD_SYN_TEXT, job.srcSendPort, inet_ntoa(d), 17);
	if (recvSize <= 0) {
		printf("����ʧ�ܣ�%s\n", getWSAErrorText());
		proexit(1);
	}
	if (passive) {
		printf("���ӳɹ���%s:%d\n", inet_ntoa(serverAddr.sin_addr), job.dstRecvPort);
		char title[100] = { 0 };
		sprintf_s(title, 100, "%d ��������%s:%d", job.srcSendPort, inet_ntoa(serverAddr.sin_addr), ntohs(serverAddr.sin_port));
		SetConsoleTitleA(title);
		job.connected = true;

		//printf("WBXFileTransfer> ");
	}
	
}

unsigned int __stdcall acceptPair(void* param) {
	SOCKET as = job.socketSend;
	int recvSize = 0;
	Data data;
	printf("����̨�ȴ��������ӣ���Ҳ������������\n");
	startRecv = true;
	recvSize = unPackData(&data, as);
	if (recvSize <= 0) {
		printf("�ȴ���������ʧ�ܣ�%s\n", getWSAErrorText());
		proexit(1);
	}
	else if (_strnicmp(METHOD_SYN_TEXT, data.control, 3)) {
		printf("�ȴ���������ʧ�ܣ����յ��Ƿ��ַ�\n");
		proexit(1);
	}
	//job.connected = true;
	printf("�յ������������� %s:%s\n", data.dat,data.seq);
	if(PASSIVE)	connectPair(atoi(data.seq),true);
	else {
		printf("���ӳɹ���%s:%s\n", data.dat, data.seq);
		char title[100] = { 0 };
		job.dstRecvPort = atoi(data.seq);

		in_addr d;
		d.S_un.S_addr = job.dstIp;
		sprintf_s(title, 100, "%d ��������%s:%d", job.srcSendPort, inet_ntoa(d), job.dstRecvPort);
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
	long cur_offset = ftell(f);	// ��ȡ��ǰƫ��λ��
	if (cur_offset == -1) {
		printf("ftell failed\n");
		return -1;
	}
	if (fseek(f, 0, SEEK_END) != 0) {	// �ƶ��ļ�ָ�뵽�ļ�ĩβ
		printf("fseek failed\n");
		return -1;
	}
	file_size = ftell(f);	// ��ȡ��ʱƫ��ֵ�����ļ���С
	if (file_size == -1) {
		printf("ftell failed\n");
	}
	if (fseek(f, cur_offset, SEEK_SET) != 0) {	// ���ļ�ָ��ָ���ʼλ��
		printf("fseek failed\n");
		return -1;
	}
	return file_size;
}