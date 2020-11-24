#include "Tester_UD.h"
#include <thread>
#include <Windows.h>

using std::string;

#pragma comment (lib, "Ws2_32.lib")

#define LOCAL_PORT 8002
static int receive_socket;
char local_ip[] = "127.0.0.1";

static bool running = false;
static Tester_UD* instance = nullptr;

static char log_string[] = "";
//static char error_code_string[] = "";
//static string log_string, errcode_string;

 
static void enter_progress_indicator_thread(void* args);
static void flash_data();
static void InitAddr(struct sockaddr_in* addr);
static void FillLogInfo(char *dest, const char*source, int error_code);

Tester_UD::Tester_UD(QWidget *parent)
    : QMainWindow(parent), _download_progress_indicator_thread(nullptr), _running(false)
{
    ui.setupUi(this);

    struct sockaddr_in receive_addr; 

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
       
        /*strcat(log_string, "Server : WSAStartup failed with error ");
        snprintf(error_code_string, 4, "%.4d\n", WSAGetLastError());
        strcat(log_string, error_code_string);*/
        FillLogInfo(log_string, "Server : WSAStartup failed with error ", WSAGetLastError());
        //ui.textBrowser->append(log_string);
        return;
    }
    else {
        //printf("Server : The Winsock DLL status is % s.\n", wsaData.szSystemStatus);
        QString log = "Server : The Winsock DLL status is ";
        log += wsaData.szSystemStatus;
        log += "\n";
        //ui.textBrowser->append(log);
    }

    receive_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (receive_socket < 0) {
        //printf("Server : Error at socket() : %d\n", WSAGetLastError());
        FillLogInfo(log_string, "Server : Error at socket() : ", WSAGetLastError());
        //ui.textBrowser->append(log_string);
        WSACleanup();
        return;
    }
    else {
        InitAddr(&receive_addr);
        printf("Server : socket() is OK !\n");
    }
}

Tester_UD::~Tester_UD() {
    if ((nullptr == _download_progress_indicator_thread) || (true != _running)) {
        return;
    }
    _running = false;
    if (_download_progress_indicator_thread && _download_progress_indicator_thread->joinable()) {
        _download_progress_indicator_thread->join();
        delete _download_progress_indicator_thread;
        _download_progress_indicator_thread = nullptr;
    }
}

void Tester_UD::on_pushButton_clicked() {
   /* std::lock_guard<std::mutex> guard(ui_mutex);*/
    if ((nullptr != _download_progress_indicator_thread) || (true == _running)) {
        return;
    }
    _download_progress_indicator_thread = new std::thread(enter_progress_indicator_thread, this);
    //ui.textBrowser->append("DOWNLAOD START");
    FillLogInfo(log_string, "DOWNLAOD START", NULL);
    _running = true;
    flash_data();
    ui.pushButton->setEnabled(false);

}

static void flash_data() {
    sockaddr_in to;
    size_t len= sizeof(to);
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = htonl(inet_addr("127.0.0.1"));
    to.sin_port = htons(8000);
    size_t n = sendto(receive_socket, "36 01", 5, 0, (sockaddr*)&to, len); 
}

static void InitAddr(struct sockaddr_in* addr) {
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = htonl(INADDR_ANY);
    addr->sin_port = htons(LOCAL_PORT);
    if (bind(receive_socket, (sockaddr*)addr, sizeof(*addr)) == SOCKET_ERROR) {
        //printf("Server : bind() failed !Error : %d.\n", WSAGetLastError());
        FillLogInfo(log_string, "bind() failed !Error : ", WSAGetLastError());
        
        closesocket(receive_socket);
        WSACleanup();
        return;
    }
    else {
        //printf("Server : bind() is OK !\n");
        //FillLogInfo(log_string, "Server : bind() is OK !\n", NULL);
    }
}

static void enter_progress_indicator_thread(void *args) {
    instance = (Tester_UD*) args;
    instance->progress_indicator_thread();
}

void Tester_UD::progress_indicator_thread() {
    while (running) {
        /*std::lock_guard<std::mutex> guard(ui_mutex);*/
        //ui.textBrowser->append("dd");
        FillLogInfo(log_string, "dd", NULL);
        Sleep(1000);
    }
}

void Tester_UD::PrintStringInTextBrowser()
{
    if (sizeof(log_string) >= 1) {
        ui.textBrowser->append(log_string);
    }

}

static void FillLogInfo(char* dest, const char* source, int error_code) {
    strcat(dest, source);
    char error_string[] = "";
    snprintf(error_string, 4, "%.4d\n", error_code);
    strcat(dest, error_string);
}