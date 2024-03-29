#ifndef WIN32
   #include <unistd.h>
   #include <cstdlib>
   #include <cstring>
   #include <netdb.h>
   #include <stdio.h>
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <wspiapi.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <udt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/times.h>
#include <sys/mman.h>
#include <fstream>

#include "cc.h"

using namespace std;

// define control data
#define START_TRANS "start"
#define END_TRANS "end"

// define UNITS
#define UNITS_G 1000000000
#define UNITS_M 1000000
#define UNITS_K 1000
#define UNITS_BYTE_TO_BITS 8

// define packet size(bytes)
#define BUFFER_SIZE 10000
#define NUM_PACKET_LENGTH 1000

// define DEFAULT_PORT
#define CONTROL_DEFAULT_PORT "9000"
#define DATA_DEFAULT_PORT "8999"

// temporary store system time

char* sys_time;
// compute execution time
clock_t old_time, new_time;
// compute file writing time
clock_t old_w_time, new_w_time, tmp_w_time = 0;
// use for count executing time
struct tms time_start,time_end;
// use for count writing time
struct tms time_w_start, time_w_end;
double ticks;
// packets
int total_send_size = 0;
int seq_client = 0;
int totalbytes = 0;
double execute_time;
double final_execute_time = 0.0f; 
double throughput_bytes = 0.0f; 
string port_data_socket;
// finish recving flag
bool finish_recv = false;
// timer
int timer_counter = 1;
int total_timer_count = 0;
// finish connection
bool timeout = false;

// used to get mmap return address
void* file_addr;

int num_timeo = 0;
int continuous_timeo = 0;

// congestion control mode : 1 : UDT, 2 : CTCP,  CBicTCP
int mode;
// Maximu Segment Size
int mss = 0;
// Congetion contorl method
char method[15];
//socket ID
UDTSOCKET client_control;
UDTSOCKET client_data;
char recv_buf[BUFFER_SIZE];

// background TCP Number
int background_TCP_number = 0;

// temp variable
char* temp_MSS;
char* temp_BK_TCP;
// use to check if pkt is in-order or out-of-order when arrive receiver side
int current_seq;
int num_out_of_order = 0;

int monitor_time = 0;

//thread variable
pthread_t t1;

#ifndef WIN32
void* monitor(void*);
void* start_timer(void*);
void reset_timer();
void print_throughput(double);
void close_connection();
#else
DWORD WINAPI monitor(LPVOID);
#endif

int main(int argc, char* argv[])
{
    time_t now = time(0);
    sys_time = ctime(&now);
    cout << sys_time << endl;
    if ((6 != argc))
    {
        cout << "usage: ./udtclient [server_ip] [server_port] [MSS] [mode(1:UDT, 2:CTCP, 3:CBicTCP)] [Background TCP Number]" << endl;
        return 0;
    }
    temp_MSS = argv[3];
    mss = atoi(argv[3]); // 1us * 1Ms => 1s (because usleep use u as unit)
    cout << "mss: " << mss << endl;

    temp_BK_TCP = argv[5];
    background_TCP_number = atoi(argv[5]);

    mode = atoi(argv[4]);
    memset(method, '\0', sizeof(method));
    if(mode == 1)
        strcpy(method, "UDT");
    else if(mode == 2)
        strcpy(method, "CTCP");
    else
        strcpy(method, "CBiCTCP");

    // use this function to initialize the UDT library
    UDT::startup();

    struct addrinfo hints, *local;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    // hints.ai_socktype = SOCK_DGRAM;

    if (0 != getaddrinfo(argv[1], argv[2], &hints, &local))
    {
        cout << "incorrect network address.\n" << endl;
        return 0;
    }
    
    // exchange control packet
    client_control = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);

    // UDT Options
    //UDT::setsockopt(client, 0, UDT_CC, new CCCFactory<CUDPBlast>, sizeof(CCCFactory<CUDPBlast>));
    //UDT::setsockopt(client_control, 0, UDT_MSS, new int(9000), sizeof(int));
    //UDT::setsockopt(client, 0, UDT_SNDBUF, new int(10000000), sizeof(int));
    //UDT::setsockopt(client, 0, UDP_SNDBUF, new int(10000000), sizeof(int));


    cout << "connect to Server: " << argv[1] << ", port: " << argv[2] << endl;

    /*if (0 != getaddrinfo(argv[1], argv[2], &hints, &peer))
    {
        cout << "incorrect server/peer address. " << argv[1] << ":" << argv[2] << endl;
        return 0;
    }*/
    fflush(stdout);
    // connect to the server, implict bind
    if (UDT::ERROR == UDT::connect(client_control, local->ai_addr, local->ai_addrlen))
    {
        cout << "connect: " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }
    //freeaddrinfo(peer);
    freeaddrinfo(local);

    int ss;
    char control_data[sizeof(START_TRANS)];

    strcpy(control_data,START_TRANS);
    
    // send control packet(SOCK_STREAM)
    /*
    if(UDT::ERROR == (ss = UDT::send(client_control, control_data, sizeof(control_data), 0)))
    {
        cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
        exit(1);
    }
    int rs = 0;

    // receive seq_client_recv
    char seq_client_recv[NUM_PACKET_LENGTH];
    if (UDT::ERROR == (rs = UDT::recv(client_control, seq_client_recv, sizeof(seq_client_recv), 0)))
    {
        cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
        exit(1);
    }  
    if(rs > 0)
    {
        seq_client = atoi(seq_client_recv);
        //cout << "rs(seq_client_recv): " << rs << endl;
        cout << "seq_client: " << seq_client << endl;
    }
    */
    // receive port_data_socket
    /*if (UDT::ERROR == (rs = UDT::recv(client_control, (char *)port_data_socket.c_str(), sizeof(port_data_socket.c_str()), 0)))
    {
        cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
        exit(1);
    }
    
    string service_data(DATA_DEFAULT_PORT); 
    if(rs > 0)
    {
        cout << "port_data_socket: " << port_data_socket.c_str() << endl;
        service_data = port_data_socket;
        
    }*/

    /* create data tranfer socket(using reliable stream mode) */
    // reset hints
    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_flags = AI_PASSIVE;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (0 != getaddrinfo(argv[1], "5100", &hints, &local))
    {
        cout << "incorrect network address.\n" << endl;
        return 0;
    }
    
    // exchange data packet
    client_data = UDT::socket(local->ai_family, local->ai_socktype, local->ai_protocol);
     
    // UDT Options
    //UDT::setsockopt(client_data, 0, UDT_RCVTIMEO, &recv_timeo, sizeof(int));
    if (mode == 2)
    {
        UDT::setsockopt(client_data, 0, UDT_CC, new CCCFactory<CTCP>, sizeof(CCCFactory<CTCP>));
        cout << "Setting Congestion Control Method CTCP" << endl;

    }else if(mode == 3)
    {
        UDT::setsockopt(client_data, 0, UDT_CC, new CCCFactory<CBiCTCP>, sizeof(CCCFactory<CBiCTCP>));
        cout << "Setting Congestion Control Method CBiCTCP" << endl;

    }else if(mode ==1)
    {
        cout << "Using default Congestion Control Method UDT" << endl;
    }
    //UDT::setsockopt(client_data, 0, UDT_MSS, new int(mss), sizeof(int));
    //UDT::setsockopt(client_data, 0, UDT_SNDBUF, new int(10000000), sizeof(int));
    //UDT::setsockopt(client_data, 0, UDP_SNDBUF, new int(10000000), sizeof(int));
   	int sndbuf = 0;
    int oplen = sizeof(int);

    // setting Maximum Segment Size
    if (UDT::ERROR == UDT::setsockopt(client_data, 0, UDT_MSS, new int(mss), sizeof(int)))
    {
        cout << "setsockopt(mss): " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }else
    {
        cout << "Set MSS size : " << mss << endl;
    }
	if (UDT::ERROR == UDT::getsockopt(client_data, 0, UDT_MSS, (char *)&sndbuf, &oplen))
    {
        printf("getsockopt error\n");
    }else
    {
        cout << "UDT MSS : " << sndbuf << endl;
    }
   
    /*if (0 != getaddrinfo(argv[1], port_data_socket.c_str(), &hints, &peer))
    {
        cout << "incorrect server/peer address. " << argv[1] << ":" << argv[2] << endl;
        return 0;
    }*/
  
    //cout << "IP "<< argv[1] << " " << port_data_socket.c_str() << endl;
    //fflush(stdout);

    // connect to the server, implict bind
    if (UDT::ERROR == UDT::connect(client_data, local->ai_addr, local->ai_addrlen))
    {
        cout << "connect(client_data): " << UDT::getlasterror().getErrorMessage() << endl;
        return 0;
    }
    freeaddrinfo(local);
    cout << "connect to Server: " << argv[1] << ", port: " << port_data_socket.c_str() << endl;
    
    // using CC method
    /*
    if(mode == 3)
    {
        CUDPBlast* cchandle = NULL;
        int temp;
        UDT::getsockopt(client_data, 0, UDT_CC, &cchandle, &temp);
        if (NULL != cchandle)
        {
            cout << "enter set rate" << endl;
            //cchandle->setRate(500);
        }
    }
    */

   
    // Getting buffer info
    /*
    sndbuf = 0;
    if (UDT::ERROR == UDT::getsockopt(client_data, 0, UDT_SNDBUF, (char *)&sndbuf, &oplen))
    {
        printf("getsockopt error\n");
    }else
    {
        cout << "UDT Send Buffer size : " << sndbuf << endl;
    }
    sndbuf = 0;
    if (UDT::ERROR == UDT::getsockopt(client_data, 0, UDT_RCVBUF, (char *)&sndbuf, &oplen))
    {
        printf("getsockopt error\n");
    }else
    {
        cout << "UDT Recv Buffer size : " << sndbuf << endl;
    }
    sndbuf = 0;
    if (UDT::ERROR == UDT::getsockopt(client_data, 0, UDP_SNDBUF, (char *)&sndbuf, &oplen))
    {
        printf("getsockopt error\n");
    }else
    {
        cout << "UDP Send Buffer size : " << sndbuf << endl;
    }
    sndbuf = 0;
    if (UDT::ERROR == UDT::getsockopt(client_data, 0, UDP_RCVBUF, (char *)&sndbuf, &oplen))
    {
        printf("getsockopt error\n");
    }else
    {
        cout << "UDP Recv Buffer size : " << sndbuf << endl;
    }
    //freeaddrinfo(peer);
    */

    //-----------------------------------------------------------------------------------
    // Data Sending
    int i = 0;
    int fd;
    int ssize = 0;
    struct stat sb;
    char buffer[15] = {0};

    fd = open("/home/tony/實驗code/論文code/file.txt", O_RDONLY, S_IRWXU);
    // get file info
    if(fstat(fd, &sb) == -1)
    {
        printf("fstat error\n");
        exit(1);
    }
    // map file to memory
    file_addr = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if(file_addr == MAP_FAILED)
    {
        printf("mmap error\n");
        exit(1);
    }
    int transmit_size = 0;
    transmit_size = sb.st_size;
    
    // send file size to server
    sprintf(buffer, "%d", transmit_size);
    if(UDT::ERROR == (ss = UDT::send(client_data, (char *)buffer, sizeof(buffer), 0))) 
    {
        cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
        exit(1);
    }

    while(1)
    { 
        if(ssize < sb.st_size)
        {
            if(UDT::ERROR == (ss = UDT::send(client_data, (char *)file_addr + ssize, transmit_size, 0))) 
            {
                cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
                exit(1);
            }
            else
            {
                // record start time when send function first called
                if(i == 0)
                {
                    //start time
                    if((old_time = times(&time_start)) == -1)
                    {
                        printf("time error\n");
                        exit(1);
                    }
                    pthread_create(&t1, NULL, monitor, &client_data);
                }
                // record already sent data size
                ssize += ss;
                transmit_size -= ss;
            }
        }
        total_send_size = ssize;
        if(monitor_time >= 500)
            break;
        if(ssize == sb.st_size)
            break;
        
        i++;
    }
    cout << "end sending" << endl;
    //finish time
    if((new_time = times(&time_end)) == -1)
    {
        printf("time error\n");
        exit(1);
    }
    // close file
    close(fd);
    memset(buffer, '\0', sizeof(buffer));
    if(UDT::ERROR == (ss = UDT::recv(client_data, (char *)buffer, sizeof(buffer), 0))) 
    {
        cout << "recv:" << UDT::getlasterror().getErrorMessage() << endl;
        exit(1);
    }
    if(strncmp(buffer, "END", ss) == 0)
    {
        if(UDT::ERROR == (ss = UDT::send(client_data, (char *)buffer, sizeof(buffer), 0)))
        {
            cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
            exit(1);
        }
        close_connection();     
    }
    return 1;
}
void close_connection()
{
    char result_addr[50] = {0}; 
    fstream fout;
    // record result
    if (mode == 1)
    {
        strcat(result_addr, "UDT_Send_Result_");
        strcat(result_addr, "MSS");
        strcat(result_addr, temp_MSS);
        strcat(result_addr, "_TCP");
        strcat(result_addr, temp_BK_TCP);
        strcat(result_addr, ".csv");
        fout.open(result_addr, ios::out|ios::app);
    }
    else if(mode == 2)
    {
        strcat(result_addr, "CTCP_Send_Result_");
        strcat(result_addr, "MSS");
        strcat(result_addr, temp_MSS);
        strcat(result_addr, "_TCP");
        strcat(result_addr, temp_BK_TCP);
        strcat(result_addr, ".csv");
        fout.open(result_addr, ios::out|ios::app);
    }
    else if(mode == 3)
    {
        strcat(result_addr, "CBiCTCP_Send_Result_");
        strcat(result_addr, "MSS");
        strcat(result_addr, temp_MSS);
        strcat(result_addr, "_TCP");
        strcat(result_addr, temp_BK_TCP);
        strcat(result_addr, ".csv");
        fout.open(result_addr, ios::out|ios::app);
    }
    
    ticks = sysconf(_SC_CLK_TCK);
    printf("\n[Close Connection]\n");

    execute_time = (double)(new_time - old_time)/ticks; 
    printf("Total Sending Time (sec): %2.2f\n", execute_time);
    
    //sprintf(str, "UDT\nMss: %d\nTotal Execute Time (sec) : %f\n\n", mss, execute_time);
    
    // write info and close file
    fout << endl << endl;
    fout << "Method," << method << endl;
    fout << "BK TCP Number," << background_TCP_number << endl;
    fout << "程式執行時間," << sys_time << endl;
    fout << "MSS," << mss << endl;
    fout << "發送時間," << execute_time << endl;
    fout << endl << endl;
    fout.close();
    
    // close control message exchange
    throughput_bytes = (double)total_send_size / execute_time;
    print_throughput(throughput_bytes);  
    /*cout << "send END_TRANS" << endl;
    char control_data3[sizeof(END_TRANS)];
    int ss_control_data3 = 0;
    strcpy(control_data3,END_TRANS);
    if(UDT::ERROR == (ss_control_data3 = UDT::send(client_control, control_data3, sizeof(control_data3), 0))) 
    {
        cout << "send:" << UDT::getlasterror().getErrorMessage() << endl;
        exit(1);
    }
    */
    //if(ss_control_data3 > 0)
    //{
    cout << "END connection" << endl;
    pthread_join(t1, NULL);
    UDT::close(client_control);
    UDT::close(client_data);
    
    // use this function to release the UDT library
    UDT::cleanup();
    //}
    
}

void print_throughput(double throughput)
{
  //cout << "enter print throughput " << endl;
  cout << "througput " << throughput << endl;
  if(throughput >= UNITS_G)
  {
    throughput /= UNITS_G;
    printf("Throughput: %2.2f (Gbytes/s)\n", throughput);
  }
  else if(throughput >= UNITS_M)
  {
    throughput /= UNITS_M;
    printf("Throughput: %2.2f (Mbytes/s)\n", throughput);
  }
  else if(throughput >= UNITS_K)
  {
    throughput /= UNITS_K;
    printf("Throughput: %2.2f (Kbyte/s)\n", throughput);
  }
  else
  {
    printf("Throughput: %2.2f (bits/s)\n", throughput);
  }
}

#ifndef WIN32
void* monitor(void* s)
#else
DWORD WINAPI monitor(LPVOID s)
#endif
{
    char result_addr[50] = {0};
    UDTSOCKET u = *(UDTSOCKET*)s;
    int zero_times = 0;
    UDT::TRACEINFO perf;
    fstream fout;

    if(mode == 1)
    {
        strcat(result_addr, "Sender_UDT_Monitor_");
        strcat(result_addr, "MSS");
        strcat(result_addr, temp_MSS);
        strcat(result_addr, "_TCP");
        strcat(result_addr, temp_BK_TCP);
        strcat(result_addr, ".csv");
        fout.open(result_addr, ios::out|ios::app);
    }
    else if (mode == 2)
    {
        strcat(result_addr, "Sender_CTCP_Monitor_");
        strcat(result_addr, "MSS");
        strcat(result_addr, temp_MSS);
        strcat(result_addr, "_TCP");
        strcat(result_addr, temp_BK_TCP);
        strcat(result_addr, ".csv");
        fout.open(result_addr, ios::out|ios::app);
    }
    else if (mode == 3)
    {
        strcat(result_addr, "Sender_CBiCTCP_Monitor_");
        strcat(result_addr, "MSS");
        strcat(result_addr, temp_MSS);
        strcat(result_addr, "_TCP");
        strcat(result_addr, temp_BK_TCP);
        strcat(result_addr, ".csv");
        fout.open(result_addr, ios::out|ios::app);
    }
    
    //fout << endl << endl;
    //fout << "Method," << method << endl;
    //fout << "BK TCP Number," << background_TCP_number << endl;
    //fout << "程式執行時間," << sys_time << endl;
    //fout << "MSS," << mss << endl;
    fout << "SendPacket," <<"SendRate(Mb/s)," << "FlightSize," << "ReceiveRate(Mb/s)," << "Send Loss," << "Recv Loss," << "RTT(ms)," << "CWnd," << "FlowWindow," << "Retrans," << "PktSndPeriod(us)," << "RecvACK," << "RecvNAK," << "EstimatedBandwidth(Mb/s)," << "Retrans_Total," << "NAK_TotalRecv," << "Total Send Loss," << "Total Recv Loss" << endl;
    while (true)
    {
        #ifndef WIN32
            sleep(1);
        #else
            Sleep(1000);
        #endif
            
        if (UDT::ERROR == UDT::perfmon(u, &perf))
        {
            cout << "perfmon: " << UDT::getlasterror().getErrorMessage() << endl;
            break;
        }
        fout << perf.pktSent << "," << perf.mbpsSendRate << "," << perf.pktFlightSize << "," << perf.mbpsRecvRate << "," << perf.pktSndLoss << "," << perf.pktRcvLoss << ","<< perf.msRTT << "," << perf.pktCongestionWindow << ","
            << perf.pktFlowWindow << "," << perf.pktRetrans << "," << perf.usPktSndPeriod << "," << perf.pktRecvACK << "," << perf.pktRecvNAK << "," << perf.mbpsBandwidth << "," << perf.pktRetransTotal << "," << perf.pktRecvNAKTotal << "," << perf.pktSndLossTotal << "," << perf.pktRcvLossTotal << endl;
        monitor_time ++;
        if(monitor_time >= 600)
            break;
        if(perf.mbpsSendRate == 0 && perf.pktRecvACK == 0 && perf.pktRecvNAK == 0)
        {
            zero_times++;
        }else
        {
            zero_times = 0;
        }
        if(zero_times >= 5)
            break;
        /*cout << perf.mbpsSendRate << "\t\t" 
            << perf.msRTT << "\t" 
            << perf.pktCongestionWindow << "\t" 
            << perf.usPktSndPeriod << "\t\t\t" 
            << perf.pktRecvACK << "\t" 
            << perf.pktRecvNAK << endl;*/
    }
    fout << endl << endl;
    fout.close();
    #ifndef WIN32
        return NULL;
    #else
        return 0;
    #endif
}
