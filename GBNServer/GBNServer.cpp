#include "../ftcommon.h"
#include <io.h>
#include <direct.h>
#define WINDOW_SIZE 5
#define AUTO_START true
#define ENABLE_MISSING true
#define MISSING_PR 1000 //max=32767

typedef struct Package {
	int stoparr = 0;			//是否已收到ACK，并关闭计时器
	bool cancelarr = 0;			//是否因为收到更前的错误ACK，将统一发送新的（从更前到最新seq），需要取消当前计时器
	const char* method = NULL;	//发送数据的控制字段类型
	int targetseq = 0;			//统一发送新的时，最新的seq
	int size = 0;				//发送数据的内容大小
	char* FILEBUFFER =0;		//发送数据的内容
}PACKAGE;
PACKAGE PACK[MAX_PACKAGE_NUM];
void printStatus(int total, int curr, int totalb, bool currb[]) {
	printf("[");
	for (int i = 1; i <= totalb; i++) {
		if (currb == NULL) {
			if (PACK[i].stoparr)printf("#");
			else printf(" ");
		}
		else {
			if (currb[i])printf("#");
			else printf(" ");
		}

	}
	printf("] %d/%d | %2.2f%%\n", curr, total, curr / (double)total * 100.0);
}


int Gbase;		//窗口的最小序号
int Gmaxseq;	//窗口最大的序号
int Gseq;		//当前发送的序号
int Gstoparr[MAX_PACKAGE_NUM];//已完成发送的数据报序号（桶排序，用于停止计时器），本来使用bool，但因SR共用改成int，在SR中用0表示未发送，1表示已正确接收，2（仅接收方）表示已缓存
int GtotalSend;	//已正确发送的字节
int GtotalBlock;//总数据报数
int GreadSizeArr[MAX_PACKAGE_NUM];//每个数据报的数据大小
int GFilesize;	//文件总大小
FILE* Gf;		//文件符
SOCKET Gas;		//套接字符

unsigned int __stdcall timeThread(void* s) {
	TIME* st = (TIME*)s;
	SOCKET as = st->as;
	char* buffer = st->data;
	int* stop = st->stop;
	time_t t = 0;
	timer(&t);
	printf("启动定时器：%s %d\n", st->method, st->seq);
	Sleep(1000);
	while (!*stop && !*st->cancel) {
		printf("定时器查询：%s %d %lld：", st->method, st->seq,t);
		if (timer(&t)) {
			//超时了，重传
			for (int i = st->seq; i <= PACK[i].targetseq; i++) {
				printf("定时器超时，重发数据：%s %d\n", PACK[i].method, i);
				int sendSize = packData(as, PACK[i].method, i, PACK[i].FILEBUFFER, PACK[i].size);
				if (sendSize <= 0) {
					printf("发送失败，错误代码为: %s\n", getWSAErrorText());
					proexit(1);
				}
				if (i == st->seq) continue;
				int* stop = &PACK[i].stoparr;
				*stop = false;
				bool* cancel = &PACK[i].cancelarr;
				*cancel = false;
				PACK[i].targetseq = i;
				HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, &timeThread, new TIME(Gas, PACK[i].FILEBUFFER, PACK[i].method, i, stop, PACK[i].size, cancel, &PACK[i].targetseq), 0, 0);
				if (hThread != NULL) CloseHandle(hThread);
			}
			timer(&t);
		}
		Sleep(1000);
	}
	*st->cancel = false;
	printf("定时器已结束：%s %d\n", st->method, st->seq);
	//delete[]buffer;
	delete st;
	return 0;
}

void releaseAllFileBuffer(int count) {
	for (int i = 1; i < count; i++) {
		if (PACK[i].FILEBUFFER != NULL) {
			delete[]PACK[i].FILEBUFFER;
			PACK[i].FILEBUFFER = NULL;
		}
	}
}

unsigned __stdcall thread_sendPackage(void* param) {
	int sendSize = 0, recvSize = 0, readSize = 0;
	//char* bufp;
	char buffer[MAXSIZE + 1] = {0};
	ZeroMemory(buffer, MAXSIZE + 1);
	readSize = fread_s(buffer, MAXSIZE, sizeof(char), MAXSIZE, Gf);
	while (GtotalSend < GFilesize) {
		Sleep(200);
		while (readSize != 0 && Gseq < Gmaxseq) {
			printf("窗口为: [%d,%d]\n", Gbase, Gmaxseq);
			//读入并发送   直到文件末尾或达到最大窗口
			printf("文件成功读入数据：%d\n", readSize);
			PACK[Gseq].FILEBUFFER = new char[MAXSIZE + 1];
			strcpy_s(PACK[Gseq].FILEBUFFER, MAXSIZE + 1, buffer);
			sendSize = packData(Gas, METHOD_DATA_TEXT, Gseq, buffer, readSize);
			if (sendSize <= 0) {
				printf("发送DATA失败，错误代码为: %s\n", getWSAErrorText());
				releaseAllFileBuffer(Gseq);
				proexit(1);
			}
			//printf("成功发送数据[%d]%d\n", seq, sendSize);

			//设置定时器			
			int* stop = &PACK[Gseq].stoparr;
			*stop = false;
			bool* cancel = &PACK[Gseq].cancelarr;
			PACK[Gseq].method = METHOD_DATA_TEXT;
			PACK[Gseq].size = readSize;
			*cancel = false;
			PACK[Gseq].targetseq = Gseq;
			HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, &timeThread, new TIME(Gas, PACK[Gseq].FILEBUFFER, METHOD_DATA_TEXT, Gseq, stop, readSize, cancel, &PACK[Gseq].targetseq), 0, 0);
			if (hThread != NULL) CloseHandle(hThread);
			GreadSizeArr[Gseq] = readSize;

			Gseq++;
			ZeroMemory(buffer, MAXSIZE);
			readSize = fread_s(buffer, MAXSIZE, sizeof(char), MAXSIZE, Gf);
		}
		//printf("窗口已满，等待中！\n");
		//printf("读入数据：%d，窗口为: [%d,%d]\n", readSize,Gbase, Gmaxseq);
		//printf("正在等待ACK: ");
		//for (int i = Gbase; i < Gmaxseq; i++) {
		//	if(!PACK[i].stoparr) printf("%d,", i);
		//}
		//printf("\n正在等待发送: %d\n", Gseq);
	}
	return 0;
}

unsigned __stdcall gbnrecv(void* param) {
	SOCKET as = job.socketSend;
	int SEQ = 0;
	int sendSize = 0, recvSize = 0;
	Data bufrecv;
	recvSize = unPackData(&bufrecv, as);
	if (recvSize <= 0) {
		printf("接收LEN数据失败！%s\n", getWSAErrorText());
		proexit(1);
	}
	else if (_strnicmp(METHOD_LEN_TEXT, bufrecv.control, 3)) {
		printf("接收LEN失败！接收到非法字符\n");
		proexit(1);
	}
	FILE* f;
	printf("准备接收文件：%s [大小：%s]\n", bufrecv.dat, bufrecv.seq);
	char filename[100] = { 0 };
	sprintf_s(filename, "%s/%s", FILE_DIR, bufrecv.dat);
	fopen_s(&f, filename, "w");
	if (f == 0) {
		printf("打开文件失败：%s", filename);
		proexit(1);
	}
	printf("打开文件成功：%s\n", filename);
	sendSize = packData(as, METHOD_ACK_TEXT, 0, NULL, 0);
	if (sendSize <= 0) {
		printf("发送ACK失败，错误代码为: %s\n", getWSAErrorText());
		fclose(f);
		proexit(1);
	}
	printf("发送ACK成功\n");
	srand((unsigned)time(NULL));
	//Sleep(10000);
	int Filesize = atoi(bufrecv.seq);
	int base = 1;
	int maxseq = base + WINDOW_SIZE;
	int seq = base-1;
	int totalRecv = 0;
	int totalBlock = Filesize / MAXSIZE + 1;
	char buffer[MAXSIZE + 1] = { 0 };
	int writeSize = 0;
	bool stoparr[1000] = { 0 };
	while (totalRecv < Filesize) {
		ZeroMemory(&bufrecv, MAXSIZE);
		recvSize = unPackData(&bufrecv, as);
		if (recvSize <= 0) {
			printf("接收数据失败，错误代码为: %s\n", getWSAErrorText());
			fclose(f);
			proexit(1);
			return 0;
		}
		else if (!_strnicmp(METHOD_DATA_TEXT, bufrecv.control, 3)) {
			int recvseq = atoi(bufrecv.seq);

			if (ENABLE_MISSING) {
				if (rand() < MISSING_PR) {
					printf("模拟丢包！[%d]\n", recvseq);
					continue;
				}
			}

			writeSize = fwrite(bufrecv.dat, sizeof(char), recvSize + MAXSIZE - MAXPACK, f);
			if (recvseq == base) {
				totalRecv += writeSize;
				base++;
				maxseq++;
				seq++;
				stoparr[recvseq] = true;
			}
			else {
				printf("GBN不接受该包[%d]\n", recvseq);
			}
			printStatus(Filesize, totalRecv, totalBlock, stoparr);
		}
		else {
			printf("收到未知报文，退出！[%s]", bufrecv.control);
			Sleep(5000);
			fclose(f);
			proexit(1);
		}
		sendSize = packData(as, METHOD_ACK_TEXT, seq, NULL, 0);
		if (sendSize <= 0) {
			printf("发送ACK失败，错误代码为: %s\n", getWSAErrorText());
			fclose(f);
			proexit(1);
		}
		//printf("成功发送ACK[%d]\n", seq);
		
		ZeroMemory(buffer, MAXSIZE);
	}

	Sleep(1000);
	printf("文件已传输完成！\n");
	/*sendSize = packData(as, METHOD_FIN_TEXT, 0, NULL, 0);
	if (sendSize <= 0) {
		printf("发送FIN失败，错误代码为: %s\n", getWSAErrorText());
		fclose(f);
		proexit(1);
	}*/
	fclose(f);
	printf("WBXFileTransfer> ");
	return 0;
}
void gbntransfer(FILE* f, char* filename, int namestart) {
	if (!job.connected) {
		printf("请先连接后再尝试发送！\n");
		//_getch();
		printf("WBXFileTransfer> ");
		return;
	}
	Gf = f;
	GFilesize = getFileSize(f);
	printf("准备发送文件：%s [大小：%d]\n", filename + namestart, GFilesize);
	Gas = job.socketSend;

	int sendSize = 0, recvSize = 0;

	sendSize = packData(Gas, METHOD_LEN_TEXT, GFilesize, filename + namestart, strlen(filename + namestart));
	if (sendSize <= 0) {
		printf("发送LEN失败，错误代码为: %s\n", getWSAErrorText());
		proexit(1);
	}
	Data bufrecv;
	recvSize = unPackData(&bufrecv, Gas);
	if (recvSize <= 0) {
		printf("接收ACK数据失败！%s\n", getWSAErrorText());
		proexit(1);
	}
	else if (_strnicmp(METHOD_ACK_TEXT, bufrecv.control, 3)) {
		printf("接收ACK失败！接收到非法字符\n");
		proexit(1);
	}
	Gbase = 1;
	Gmaxseq = Gbase + WINDOW_SIZE;
	Gseq = Gbase;
	ZeroMemory(PACK, sizeof(PACKAGE)*1000);
	GtotalSend = 0;
	char buffer[MAXSIZE + 1] = { 0 };
	GtotalBlock = GFilesize/MAXSIZE+1;
	memset(GreadSizeArr, 0, MAX_PACKAGE_NUM);
	int readSize = 0;//fread_s(buffer, MAXSIZE, sizeof(char), MAXSIZE, f);
	//int* stop;
	//bool* cancel;
	//HANDLE hThread;
	HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, &thread_sendPackage, NULL, 0, 0);
	if (hThread != NULL) CloseHandle(hThread);

	while (GtotalSend < GFilesize) {
		{
			ZeroMemory(&bufrecv, MAXSIZE);
			recvSize = unPackData(&bufrecv, Gas);
			if (recvSize <= 0) {
				printf("接收ACK失败，错误代码为: %s\n", getWSAErrorText());
				releaseAllFileBuffer(Gseq);
				proexit(1);
			}
			else if (!_strnicmp(METHOD_ACK_TEXT, bufrecv.control, 3)) {
				int recvseq = atoi(bufrecv.seq);
				//告诉timer线程如果没发送则可以退出了
				if (recvseq == Gbase) {
					GtotalSend += GreadSizeArr[recvseq];
					PACK[recvseq].stoparr = true;
					Gbase++;
					Gmaxseq=Gbase+WINDOW_SIZE;
				}
				else {
					if (!PACK[recvseq+1].cancelarr) {
						printf("收到错误序列号 %d，等待首个定时器超时，取消后续定时器\n", recvseq);
						printf("设置定时器[%d]的重发目标终点为%d\n", recvseq+1,Gseq-1);
						PACK[recvseq + 1].targetseq = Gseq;
						for (int i = recvseq + 2; i < Gseq; i++) {
							printf("取消定时器[%d]\n", i);
							PACK[i].cancelarr = true;
						}
					}
					else {
						printf("收到错误序列号 %d，正等待\n", recvseq);
					}
					
				}
				//
				printStatus(GFilesize, GtotalSend, GtotalBlock,NULL);
				if (GtotalSend >= GFilesize) break;
			}
			else {
				printf("收到未知报文，退出！[%s]", bufrecv.control);
				Sleep(5000);
				releaseAllFileBuffer(Gseq);
				proexit(1);
			}
			
		}
		
	}
	Sleep(1000);
	printf("文件已传输完成！\n");
	releaseAllFileBuffer(Gseq);
	/*sendSize = packData(as, METHOD_FIN_TEXT, 0, NULL, 0);
	if (sendSize <= 0) {
		printf("发送FIN失败，错误代码为: %s\n", getWSAErrorText());
		proexit(1);
	}*/
	rewind(file);
	return;
}

void waitForRecv() {
	if (!startRecv || !job.connected) {
		printf("请先完成连接！\n");
		return;
	}
	gbnrecv(NULL);
	//HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, &, NULL, 0, 0);
	//if (hThread != NULL) CloseHandle(hThread);
}

int main(int argc, char* argv[]) {
	init(argc, argv, L"../LAB3/Debug/GBNServer.exe");
	char cmd[100] = { 0 };
	char opt = '\0';
	int start = 0;
	if (AUTO_START) {
		start = 2;
	}
	printf("WBXFileTransfer> ");
	while (true) {
		if(start==0) opt = getchar();
		else if (start == 2) {
			Sleep(1000);
			opt = 'S';
			start = 1;
		}else if(start == 1) {
			opt = 'C';
			start = 0;
		}

		if (opt == '\n') {
			printf("WBXFileTransfer> ");
			continue;
		}


		/*if (scanf_s("%s", cmd, 100) != 1) {
			printf("无效命令！\n");
			continue;
		}
		else */
		if (opt == 'q' || opt == 'Q') {
			proexit(0);
		}
		else if (opt == 's') {
			scanf_s("%s", cmd, 100);
			if (_access(cmd, 0) == -1)
			{
				strcpy_s(FILE_DIR, cmd);
				_mkdir(cmd);
			}
			printf("正在启动传输，文件将保存到文件夹：%s\n", FILE_DIR);
			/*fopen_s(&file, cmd, "w");
			if (file == NULL) {
				printf("打开文件失败！\n");
				continue;
			}*/

			waitForConnect();
			Sleep(100);
			printf("WBXFileTransfer> ");
			//	transfer(file);
			//	fclose(file);

		}
		else if (opt == 'S') {
			printf("正在启动传输，文件将保存到文件夹：%s\n", FILE_DIR);
			if (_access(FILE_DIR, 0) == -1)
			{
				_mkdir(FILE_DIR);
			}
			waitForConnect();
			Sleep(100);
			printf("WBXFileTransfer> ");
		}
		else if (opt == 'R') {
			printf("正在等待接收，文件将保存到文件夹：%s\n", FILE_DIR);
			waitForRecv();
			Sleep(100);
			printf("WBXFileTransfer> ");
		}
		else if (opt == 'f') {
			int namestart = strlen(FILE_DIR) + 1;
			if (scanf_s("%s", cmd, 100) == 1) {
				namestart = 0;
			}
			else {
				sprintf_s(cmd, FILE_NAME_FORMAT, FILE_DIR, TRANS_FILE_DEFAULT, job.srcSendPort);

			}
			printf("准备发送文件：%s\n", cmd);
			fopen_s(&file, cmd, "r");
			if (file == NULL) {
				printf("打开文件失败！\n");
				continue;
			}
			gbntransfer(file, cmd, namestart);
			fclose(file);
			printf("WBXFileTransfer> ");
		}
		else if (opt == 'F') {
			sprintf_s(cmd, FILE_NAME_FORMAT, FILE_DIR, TRANS_FILE_DEFAULT, job.srcSendPort);
			printf("准备发送文件：%s\n", cmd);
			fopen_s(&file, cmd, "r");
			if (file == NULL) {
				printf("打开文件失败！\n");
				continue;
			}
			gbntransfer(file, cmd, strlen(FILE_DIR)+1);
			fclose(file);
			printf("WBXFileTransfer> ");
		}
		else if (opt == 'c') {
			int port = SUB_PROCESS ? SEND_PORT_DEFAULT - 2 : SEND_PORT_DEFAULT;
			scanf_s("%d", &port);
			connectPair(port, false);
		}
		else if (opt == 'C') {
			int port = SUB_PROCESS ? SEND_PORT_DEFAULT - 2 : SEND_PORT_DEFAULT;
			connectPair(port, false);
		}
		else {
			printf("无效命令！\n");
			printf("WBXFileTransfer> ");
		}
		fflush(stdin);

	}
	proexit(0);
}