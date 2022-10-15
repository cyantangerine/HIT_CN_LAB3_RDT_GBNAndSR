#include "../ftcommon.h"
#include <io.h>
#include <direct.h>
#define WINDOW_SIZE 5
#define AUTO_START true
#define ENABLE_MISSING true
#define MISSING_PR 1000 //max=32767

char *cachedBuffer[MAX_PACKAGE_NUM] = { 0 };
int cachedBufferSize[MAX_PACKAGE_NUM] = { 0 };

void printStatus(int total, int curr, int totalb, int currb[]) {
	printf("[");
	for (int i = 1; i <= totalb; i++) {
		
			if (currb[i] == true)printf("#");
			else if (currb[i] == 2)printf("-");
			else printf(" ");
		

	}
	printf("] %d/%d | %2.2f%%\n", curr, total, curr / (double)total * 100.0);
}
unsigned int __stdcall timeThread(void* s) {
	TIME* st = (TIME*)s;
	SOCKET as = st->as;
	char* buffer = st->data;
	int* stop = st->stop;
	time_t t = 0;
	timer(&t);
	printf("启动定时器：%s %d\n", st->method, st->seq);
	Sleep(1000);
	while (!*stop) {
		printf("定时器查询：%s %d %lld：", st->method, st->seq, t);
		if (timer(&t)) {
			//超时了，重传
			printf("定时器超时，重发数据：%s %d\n", st->method, st->seq);

			int sendSize = packData(as, st->method, st->seq, st->data, st->readSize);
			if (sendSize <= 0) {
				printf("发送失败，错误代码为: %s\n", getWSAErrorText());
				proexit(1);
			}
			timer(&t);
		}
		Sleep(1000);
	}
	printf("定时器已结束：%s %d\n", st->method, st->seq);
	delete[]buffer;
	delete st;
	return 0;
}
void deleteCacheBuffer() {
	for (int i = 0; i < MAX_PACKAGE_NUM; i++) {
		if (cachedBuffer[i]) {
			delete[]cachedBuffer[i];
			cachedBuffer[i] = NULL;
		}
	}
}
unsigned __stdcall srrecv(void* param) {
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
	int seq = base - 1;
	int totalRecv = 0;
	int totalBlock = Filesize / MAXSIZE + 1;
	char buffer[MAXSIZE + 1] = { 0 };
	int writeSize = 0;
	int stoparr[1000] = { 0 };
	ZeroMemory(cachedBuffer, MAX_PACKAGE_NUM);
	memset(cachedBufferSize,0, MAX_PACKAGE_NUM);
	while (totalRecv < Filesize) {
		ZeroMemory(&bufrecv, MAXSIZE);
		recvSize = unPackData(&bufrecv, as);
		if (recvSize <= 0) {
			printf("接收数据失败，错误代码为: %s\n", getWSAErrorText());
			deleteCacheBuffer();
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

			if (recvseq == base) {
				writeSize = fwrite(bufrecv.dat, sizeof(char), recvSize + MAXSIZE - MAXPACK, f);
				totalRecv += writeSize;
				base++;
				maxseq++;
				seq++;
				stoparr[recvseq] = true;
			}
			else {
				if (!stoparr[recvseq] && cachedBuffer[recvseq]==NULL) {
					printf("缓存乱序包[%d]\n", recvseq);
					cachedBuffer[recvseq] = new char[MAXSIZE+1];
					strcpy_s(cachedBuffer[recvseq], MAXSIZE + 1,bufrecv.dat);
					cachedBufferSize[recvseq] = recvSize + MAXSIZE - MAXPACK;
					stoparr[recvseq] = 2;
				}
				else {
					printf("丢弃重复乱序包[%d]\n", recvseq);
				}
				
			}
			
			sendSize = packData(as, METHOD_ACK_TEXT, recvseq, NULL, 0);
			if (sendSize <= 0) {
				printf("发送ACK失败，错误代码为: %s\n", getWSAErrorText());
				deleteCacheBuffer();
				fclose(f);
				proexit(1);
			}

			//滑动窗口
			while (cachedBuffer[base]) {
				
				writeSize = fwrite(cachedBuffer[base], sizeof(char), cachedBufferSize[base], f);
				totalRecv += writeSize;
				delete[]cachedBuffer[base];
				cachedBuffer[base] = NULL;
				stoparr[base] = true;
				maxseq++;
				seq++;

				base++;
			}
			printStatus(Filesize, totalRecv, totalBlock, stoparr);
		}
		else {
			printf("收到未知报文，退出！[%s]", bufrecv.control);
			deleteCacheBuffer();
			Sleep(5000);
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
int Gbase;
int Gmaxseq;
int Gseq;
int Gstoparr[MAX_PACKAGE_NUM];
int GtotalSend;
int GtotalBlock;
int GreadSizeArr[MAX_PACKAGE_NUM];

int GFilesize;
FILE* Gf;
SOCKET Gas;

unsigned __stdcall thread_sendPackage(void* param) {
	int sendSize = 0, recvSize = 0, readSize = 0;
	char* bufp;
	char buffer[MAXSIZE + 1];
	ZeroMemory(buffer, MAXSIZE+1);
	readSize = fread_s(buffer, MAXSIZE, sizeof(char), MAXSIZE, Gf);
	while (GtotalSend < GFilesize) {
		Sleep(200);
		while (readSize != 0 && Gseq < Gmaxseq) {
			printf("窗口为: [%d,%d]\n", Gbase, Gmaxseq);
			//读入并发送   直到文件末尾或达到最大窗口
			printf("文件成功读入数据：%d\n", readSize);
			sendSize = packData(Gas, METHOD_DATA_TEXT, Gseq, buffer, readSize);
			if (sendSize <= 0) {
				printf("发送DATA失败，错误代码为: %s\n", getWSAErrorText());
				proexit(1);
			}
			
			//设置定时器			
			int* stop = &Gstoparr[Gseq];
			*stop = false;
			bufp = new char[MAXSIZE + 1];
			memcpy_s(bufp, MAXSIZE, buffer, MAXSIZE);
			HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, &timeThread, new TIME(Gas, bufp, METHOD_DATA_TEXT, Gseq, stop, readSize, NULL, NULL), 0, 0);
			if (hThread != NULL) CloseHandle(hThread);
			GreadSizeArr[Gseq] = readSize;

			Gseq++;
			ZeroMemory(buffer, MAXSIZE);
			readSize = fread_s(buffer, MAXSIZE, sizeof(char), MAXSIZE, Gf);

		}
		//printf("窗口已满！等待中。\n");
	}
	return 0;
}

void srtransfer(FILE* f, char* filename, int namestart) {
	if (!job.connected) {
		printf("请先连接后再尝试发送！\n");
		//_getch();
		printf("WBXFileTransfer> ");
		return;
	}
	GFilesize = getFileSize(f);
	printf("准备发送文件：%s [大小：%d]\n", filename + namestart, GFilesize);
	Gf = f;
	Gas = job.socketSend;
	int sendSize = 0, recvSize = 0, readSize = 0;

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
	memset(Gstoparr, 0, MAX_PACKAGE_NUM);
	GtotalSend = 0;
	//memset(Gbuffer, 0, MAXSIZE+1);
	GtotalBlock = GFilesize / MAXSIZE + 1;
	memset(GreadSizeArr, 0, MAX_PACKAGE_NUM);
	
	HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, &thread_sendPackage, NULL, 0, 0);
	if (hThread != NULL) CloseHandle(hThread);

	while (GtotalSend < GFilesize) {
		{
			ZeroMemory(&bufrecv, MAXSIZE);
			recvSize = unPackData(&bufrecv, Gas);
			if (recvSize <= 0) {
				printf("接收ACK失败，错误代码为: %s\n", getWSAErrorText());
				proexit(1);
			}
			else if (!_strnicmp(METHOD_ACK_TEXT, bufrecv.control, 3)) {
				int recvseq = atoi(bufrecv.seq);
				//告诉timer线程如果没发送则可以退出了
				if (recvseq == Gbase) {
					Gbase++;
					Gmaxseq++;
				}
				GtotalSend += GreadSizeArr[recvseq];
				Gstoparr[recvseq] = true;
				while (Gstoparr[Gbase]) {
					Gbase++;
					Gmaxseq++;
				}
				//printf("base=%d\n",base);
				printStatus(GFilesize, GtotalSend, GtotalBlock, Gstoparr);
				if (GtotalSend >= GFilesize) break;
			}
			else {
				printf("收到未知报文，退出！[%s]", bufrecv.control);
				Sleep(5000);
				proexit(1);
			}

		}

	}
	Sleep(1500);
	printf("文件已传输完成！\n");
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
	srrecv(NULL);
	//HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, &srrecv, NULL, 0, 0);
	//if (hThread != NULL) CloseHandle(hThread);
}

int main(int argc, char* argv[]) {
	init(argc, argv, L"../LAB3/Debug/SRServer.exe");
	char cmd[100] = { 0 };
	char opt = '\0';
	int start = 0;
	if (AUTO_START) {
		start = 2;
	}
	printf("WBXFileTransfer> ");
	while (true) {
		if (start == 0) opt = getchar();
		else if (start == 2) {
			Sleep(1000);
			opt = 'S';
			start = 1;
		}
		else if (start == 1) {
			opt = 'C';
			start = 0;
		}

		if (opt == '\n') {
			printf("\rWBXFileTransfer> ");
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

			waitForConnect();
			Sleep(100);
			printf("WBXFileTransfer> ");

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
			srtransfer(file, cmd, namestart);
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
			srtransfer(file, cmd, strlen(FILE_DIR) + 1);
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