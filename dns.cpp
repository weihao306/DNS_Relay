#pragma warning(disable:4996)

#include<cstdio>
#include<vector>
#include<string>
#include<cstring>
#include<iostream>
#include<fstream>
#include<winsock2.h>
#include<time.h>
#include<iomanip>
using namespace std;
#pragma comment(lib,"Ws2_32.lib")
#define NOTFIND -1
#define LOCAL_DNS "127.0.0.1"
#define DNS_PORT 53
#define BUF_SIZE 512
#define DOMAIN_LENGTH 67
#define MAX 1000
#define EXTERNAL_DNS "10.3.9.4"
typedef struct
{
	string ip;        //save the ip
	string domain;    //save the domain
}IP_domain;
IP_domain IP_table[MAX];

typedef struct
{
	unsigned short id;
	SOCKADDR_IN client;
}ID_client;
ID_client ID_table[MAX];

int idnum = 0;
int key;
char domain[DOMAIN_LENGTH];
string file_name;		//本地DNStxt

SYSTEMTIME sys;
int Year, Month, Day, Hour, Minute, Second, Milliseconds;//保存系统时间的变量

int Get_Table(int t)    //从文件中加载域名-IP表，返回表的项数
{                                    //参数t=1，使用其他文本，t!=1，使用默认的dnsrelay.txt
	fstream infile;
	string ip_do;
	IP_domain temp;
	int i = 0;
	if (t == 1)
		infile.open(file_name.c_str(), ios::in);
	else
		infile.open("dnsrelay.txt", ios::in);
	if (!infile)
	{
		cout << "Can't open the dns file" << endl;
		exit(1);
	}
	while (getline(infile, ip_do))
	{
		//cout<<ip_do<<endl;
		int k = ip_do.find(" ");
		IP_table[i].ip = ip_do.substr(0, k);
		IP_table[i].domain = ip_do.substr(k + 1);

		cout << "i=" << i << "  " << IP_table[i].ip << "  " << IP_table[i].domain << endl;
		i++;
	}
	infile.close();
	return i;//域名解析表项数
}

void Get_Domain(char* recv_buf)  //获得客户端请求域名
{
	int n = 12, i = 0, len = 0, k = 0;
	char tempt[DOMAIN_LENGTH];
	memset(&domain, 0, DOMAIN_LENGTH);
	memset(&tempt, 0, DOMAIN_LENGTH);

	while (recv_buf[n] != 0)
	{
		len += recv_buf[n++];
		for (; i < len; i++)
			tempt[i] = recv_buf[n++];
		if (recv_buf[n] != 0)
		{
			tempt[i++] = '.';
			len++;
		}
	}
	if (len > 0)
		tempt[len] = '\0';
	else
	{
		cout << "Get domainname error" << endl;
		exit(1);
	}

	unsigned short a;
	memcpy(&a, recv_buf + (len + 14), sizeof(unsigned short));
	a = ntohs(a);
	//cout<<a<<endl;
	if (a == 0x001c)
		key = -1;
	else
		key = 0;

	for (i = 0; i < len; i++)
		domain[i] = tempt[i];
	domain[i] = '\0';
}

int GetTableNum(string temp, int num)   //寻找temp对应的域名表项数
{
	int n = -1;
	for (int i = 0; i < num; i++)
	{
		if (strcmp(temp.c_str(), IP_table[i].domain.c_str()) == 0)
		{
			n = i;
			break;
		}
	}
	return n;
}

unsigned short TransID(unsigned short OldID, SOCKADDR_IN temp)
{
	ID_table[idnum].id = OldID;	//存储原有ID
	ID_table[idnum].client = temp;
	idnum++;
	return (unsigned short)(idnum - 1);//用ID转换表中对应项数作为新的ID
}

int main()
{
	WSADATA wsaData;
	int wsa = WSAStartup(MAKEWORD(2, 2), &wsaData);			//初始化ws2_32.dll动态链接库
	if (wsa != 0)
	{
		cout << "WSAStartup error" << endl;
		exit(1);
	}
	cout << "Please input command：" << endl;
	cout << "dnsrelay [-d | -dd] [dns-server-ipaddr] [filename]" << endl;
	string str, tem = "";
	int argc = 1;
	getline(cin, str);
	for (int i = 0; i < str.size(); i++)
		if (str[i] == ' ')
			argc++;				//确定参数数量

	int j = 0;
	vector<string> argv(argc);
	for (int i = 0; i < argc; i++)
	{
		argv[i] = "";
		while (j < str.size() && str[j] != ' ')
		{
			argv[i] += str[j];
			j++;
		}
		j++;
	}
	/*
	for (int i = 0; i < argc; i++)
		cout << argv[i]<<" ";
	*/

	char send_buf[BUFSIZ], recv_buf[BUF_SIZE];    //缓存
	int num = 0, flag = -1;
	string external_dns;     //外部DNS

	if (argc == 1)
	{
		cout << "无调试信息输出" << endl;
		external_dns = EXTERNAL_DNS;
		num = Get_Table(2);
		flag = 1;
	}
	else if (argc == 4)
	{
		cout << "调试信息级别1" << endl;
		external_dns = argv[2];
		file_name = argv[3];
		cout << file_name << endl;
		num = Get_Table(1);
		flag = 2;
	}
	else if (argc == 3)
	{
		cout << "调试信息级别2" << endl;
		external_dns = argv[2];
		num = Get_Table(2);
		flag = 3;
	}

	SOCKET  SERVER, HOST;				//本地DNS和外部DNS两个套接字
	SOCKADDR_IN serv_addr, client_addr, host_addr;	//外部DNS、请求端和本地DNS三个网络套接字地址
	SERVER = socket(AF_INET, SOCK_DGRAM, 0);
	HOST = socket(AF_INET, SOCK_DGRAM, 0);
	if (SERVER == INVALID_SOCKET || HOST == INVALID_SOCKET)
	{
		cout << "Socket creation failed " << endl;
		WSACleanup();
		exit(1);
	}
	memset(&host_addr, 0, sizeof(host_addr));		//每个字节都用0填充
	host_addr.sin_family = AF_INET;		//使用IPv4地址
	host_addr.sin_addr.s_addr = inet_addr(LOCAL_DNS);		//具体的IP地址
	host_addr.sin_port = htons(DNS_PORT);	 //端口

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(DNS_PORT);
	serv_addr.sin_addr.s_addr = inet_addr(external_dns.c_str());

	if (bind(HOST, (SOCKADDR*)& host_addr, sizeof(host_addr)))
	{
		cout << "connect Port 53 error" << endl;
		exit(1);
	}
	else
		cout << "connect Port 53 success" << endl;

	if (flag == 2)
	{
		GetLocalTime(&sys);
		Year = sys.wYear;
		Month = sys.wMonth;
		Day = sys.wDay;
		cout << "Current date : " << Year << "-" << Month << "-" << Day << endl;
		cout << "time(h:m:s:ms)\t\tnum\tdomain" << endl;
	}
	int len = 0, paknum = 0;
	while (true)
	{
		len = sizeof(client_addr);
		memset(recv_buf, 0, BUF_SIZE);
		//接收DNS请求,recvfrom返回接到的字节数
		int recv = recvfrom(HOST, recv_buf, sizeof(recv_buf), 0, (SOCKADDR*)& client_addr, &len);
		/*
		UDP接收数据使用 recvfrom() 函数：
			int recvfrom(SOCKET sock, char *buf, int nbytes, int flags, const struct sockaddr *from, int *addrlen);
			参数说明：
				sock：用于接收UDP数据的套接字；
				buf：保存接收数据的缓冲区地址；
				nbytes：可接收的最大字节数（不能超过buf缓冲区的大小）；
				flags：可选项参数，若没有可传递0；
				from：存有发送端地址信息的sockaddr结构体变量的地址；
				addrlen：保存参数 from 的结构体变量长度的变量地址值。
		*/
		if (recv == SOCKET_ERROR)
		{
			cout << "receive packet error: " << WSAGetLastError() << endl;
			continue;
		}
		else if (recv == 0)
		{
			cout << "receive an impossible packet" << endl;
			break;
		}
		else
		{
			Get_Domain(recv_buf);
			if (key != -1)
				key = GetTableNum(domain, num);
			paknum++;
			if (flag == 2)
			{
				GetLocalTime(&sys);
				Hour = sys.wHour;
				Minute = sys.wMinute;
				Second = sys.wSecond;
				Milliseconds = sys.wMilliseconds;
				cout << Hour << ':' << Minute << ':' << Second << ':' << Milliseconds << "\t\t" << paknum << "\t" << domain << endl;
			}
			if (flag == 3)
			{
				cout << "recevie a packet from client" << endl;
				for (int i = 0; i < recv; i++)
					printf("%.2x  ", (unsigned char)recv_buf[i]);
				cout << endl << "domain:" << domain << endl;
			}

			if (key == NOTFIND)     //没找到对应IP
			{
				unsigned short pID;
				memcpy(&pID, recv_buf, sizeof(unsigned short));
				unsigned short nID = htons(TransID(ntohs(pID), client_addr));
				memcpy(recv_buf, &nID, sizeof(unsigned short));
				/*
					DNS报文的标识数（0-1字节）
				*/
				if (flag == 3)
				{
					printf("ID转换:%.2x %.2x -->%.2x %.2x \n\n", (pID) % 0x100, (pID) / 0x100, nID % 0x100, nID / 0x100);
					cout << endl;
				}
				int len = sizeof(serv_addr);
				int send = sendto(SERVER, recv_buf, recv, 0, (SOCKADDR*)& serv_addr, len);
				/*
				UDP发送数据使用 sendto() 函数：
					int sendto(SOCKET sock, const char *buf, int nbytes, int flags, const struct sockadr *to, int addrlen)
					参数说明：
						sock：用于传输UDP数据的套接字；
						buf：保存待传输数据的缓冲区地址；
						nbytes：带传输数据的长度（以字节计）；
						flags：可选项参数，若没有可传递0；
						to：存有目标地址信息的 sockaddr 结构体变量的地址；
						addrlen：传递给参数 to 的地址值结构体变量的长度。
				*/
				if (send == SOCKET_ERROR)
				{
					cout << "send external dns query error: " << WSAGetLastError() << endl;
					continue;
				}
				if (flag == 3)
				{
					cout << "send client packet：" << endl;
					for (int i = 0; i < recv; i++)
						printf("%.2x  ", (unsigned char)recv_buf[i]);
					cout << endl << "************************" << endl;
				}
				int recv = recvfrom(SERVER, recv_buf, sizeof(recv_buf), 0, (SOCKADDR*)& serv_addr, &len);
				if (recv == SOCKET_ERROR)
				{
					cout << "recvice external dns datagram error: " << WSAGetLastError() << endl;
					continue;
				}
				if (flag == 3)
				{
					cout << "packet from external dns server：" << endl;
					for (int i = 0; i < recv; i++)
						printf("%.2x  ", (unsigned char)recv_buf[i]);
					cout << endl << "************************" << endl;
				}
				memcpy(&pID, recv_buf, sizeof(unsigned short));
				int n = ntohs(pID);
				client_addr = ID_table[n].client;
				nID = htons(ID_table[n].id);
				memcpy(recv_buf, &nID, sizeof(unsigned short));

				if (flag == 3)
				{
					printf("ID转换:%.2x %.2x -->%.2x %.2x \n\n", (pID) % 0x100, (pID) / 0x100, nID % 0x100, nID / 0x100);
					cout << endl;
				}

				len = sizeof(client_addr);
				send = sendto(HOST, recv_buf, recv, 0, (SOCKADDR*)& client_addr, len);
				if (send == SOCKET_ERROR)
				{
					cout << "send error: " << WSAGetLastError() << endl;
					//continue;
				}
				if (flag == 3)
				{
					cout << "send client packet：" << endl;
					for (int i = 0; i < recv; i++)
					{
						printf("%.2x  ", (unsigned char)recv_buf[i]);
					}
					cout << endl << "************************" << endl;
				}
			}
			else
			{
				//找到了对应IP，对返回值进行判断
				unsigned short a;
				memcpy(send_buf, recv_buf, recv);
				paknum++;
				if (flag == 2)
				{
					GetLocalTime(&sys);
					Hour = sys.wHour;
					Minute = sys.wMinute;
					Second = sys.wSecond;
					Milliseconds = sys.wMilliseconds;
					cout << Hour << ':' << Minute << ':' << Second << ':' << Milliseconds << "\t\t" << paknum << "\t" << domain << endl;
				}

				if (strcmp(IP_table[key].ip.c_str(), "0.0.0.0") == 0)
				{
					a = htons(0X8183);
					memcpy(&send_buf[2], &a, sizeof(unsigned short));
					/*
						dns报文首部区域的标志位（2-3字节）,
						QR(1比特)：查询/响应的标志位，1为响应，0为查询。
						opcode(4比特)：定义查询或响应的类型(若为0则表示是标准的，若为1则是反向的，若为2则是服务器状态请求)。
						AA(1比特)：授权回答的标志位。该位在响应报文中有效，1表示名字服务器是权限服务器(关于权限服务器以后再讨论)
						TC(1比特)：截断标志位。1表示响应已超过512字节并已被截断(依稀好像记得哪里提过这个截断和UDP有关，先记着)
						RD(1比特)：该位为1表示客户端希望得到递归回答(递归以后再讨论)
						RA(1比特)：只能在响应报文中置为1，表示可以得到递归响应。
						zero(3比特)：不说也知道都是0了，保留字段。
						rcode(4比特)：返回码。

						a=1000 0001 1000 0011
					*/
					cout <<endl<<domain<< " : this website has been shielded" << endl;
					a = htons(0X0000);
					//拦截功能
				}
				else
				{
					a = htons(0X8180);
					/*
						a=1000 0001 1000 0000
					*/
					memcpy(&send_buf[2], &a, sizeof(unsigned short));
					cout <<endl<< "Domain name : " << domain << "  " << "   IP：" << IP_table[key].ip << endl;
					a = htons(0X0001);
					//服务器功能
				}
				memcpy(&send_buf[6], &a, sizeof(unsigned short));

				//资源记录(RR)区域（包括回答区域，授权区域和附加区域）
				int cur_len = 0;
				char answer[16];
				/*
					在资源记录中，域名通常是查询问题部分的域名的重复，就需要用指针指向查询问题部分的域名。
					2字节的指针，最前面的两个高位是11，用于识别指针。其他14位从报文开始处计数(从0开始)，指出该报文中的相应字节数。
					注意，DNS报文的第一个字节是字节0，第二个报文是字节1。
					一般响应报文中，资源部分的域名都是指针C00C(1100000000001100，12正好是首部区域的长度)，刚好指向请求部分的域名
				*/
				unsigned short name = htons(0xc00c);
				memcpy(answer, &name, sizeof(unsigned short));
				cur_len += sizeof(unsigned short);

				//查询类型：表明资源纪录的类型
				unsigned short TypeA = htons(0x0001);
				memcpy(answer + cur_len, &TypeA, sizeof(unsigned short));
				cur_len += sizeof(unsigned short);

				// 查询类：对于Internet信息，总是IN
				unsigned short ClassA = htons(0x0001);
				memcpy(answer + cur_len, &ClassA, sizeof(unsigned short));
				cur_len += sizeof(unsigned short);

				/*
					 生存时间（TTL）：以秒为单位，表示的是资源记录的生命周期，一般用于当地址解析程序取出资源记
					 录后决定保存及使用缓存数据的时间，它同时也可以表明该资源记录的稳定程度，极为稳定的信息会被分
					 配一个很大的值（比如86400，这是一天的秒数）。
				*/
				unsigned long timelive = htons(0x00001000);
				memcpy(answer + cur_len, &timelive, sizeof(unsigned long));
				cur_len += sizeof(unsigned long);

				//资源数据长度(2字节),表示资源数据的长度(以字节为单位，如果资源数据为IP则为0004)。
				unsigned short IPlen = htons(0x0004);
				memcpy(answer + cur_len, &IPlen, sizeof(unsigned short));
				cur_len += sizeof(unsigned short);

				//资源数据,该字段是可变长字段，表示按查询段要求返回的相关资源记录的数据。
				unsigned long IP = (unsigned long)inet_addr(IP_table[key].ip.c_str());
				memcpy(answer + cur_len, &IP, sizeof(unsigned long));
				cur_len += sizeof(unsigned long);

				memcpy(&send_buf[recv], answer, cur_len);
				cur_len += recv;
				int send = 0;
				len = sizeof(client_addr);
				send = sendto(HOST, send_buf, cur_len, 0, (SOCKADDR*)& client_addr, len);
				if (send == SOCKET_ERROR)
				{
					cout << "send error: " << WSAGetLastError() << endl;
					continue;
				}
				if (flag == 3)
				{
					cout << "send client-packet：" << endl;
					for (int i = 0; i < cur_len; i++)
					{
						printf("%.2x  ", (unsigned char)send_buf[i]);
					}
					cout << endl << "************************" << endl;
				}
			}
		}
	}
	closesocket(SERVER);	//关闭套接字
	closesocket(HOST);
	WSACleanup();				//释放ws2_32.dll动态链接库初始化时分配的资源
	return 0;
}
