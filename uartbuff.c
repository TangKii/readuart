#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <getopt.h>

#define MAX_DATA_LENGTH 256
#define START_PATTERN_LEN 8  // 开始模式的长度
#define TIMEOUT_INTERVAL 500 // 超时时间间隔，单位毫秒


int main(int argc, char *argv[]) {
    HANDLE hSerial;
    DCB dcbSerialParams = { 0 };
    COMMTIMEOUTS timeouts = { 0 };
    DWORD bytesRead = 0, bytesTotalcnt = 0;
    char data[MAX_DATA_LENGTH];
    FILE *file;
    char startPattern[START_PATTERN_LEN] = {0x12, 0xFF, 0x02, 0x00, 0x50, 0xFF, 0x10, 0xFF}; // 开始模式
    int recording = 0;
    int startIdx = 0; // 开始模式中已匹配的字节数
    DWORD idle_time_cnt = 0; // 上次接收到数据的时间戳
    int opt;
    int baudRate = 500000;
    char *portName = "COM3";
    char *output_name = "framebuff";
    int continuous_flag = 0;

    // 解析命令行参数
    while ((opt = getopt(argc, argv, "b:p:co:")) != -1) {
        switch (opt) {
            case 'b':
                baudRate = atoi(optarg);
                break;
            case 'p':
                portName = optarg;
                break;
            case 'o':
                output_name = optarg;
                break;
            case 'c':
                continuous_flag = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-b] <baud rate> [-p] <port name> [-c] [-o] <output file name>\n", argv[0]);
                return 1;
        }
    }

    // 打开串口
    hSerial = CreateFile(portName, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Error opening serial port\n");
        fprintf(stderr, "Usage: %s [-b] <baud rate> [-p] <port name> [-o] <output file name>\n", argv[0]);
        fprintf(stderr, "default baud rate:500000, defaul port:COM3\n");
        return 1;
    }
    else {
        printf("Success open serial port\n");
    }

    // 配置串口参数
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        fprintf(stderr, "Error getting serial port state\n");
        CloseHandle(hSerial);
        return 1;
    }
    dcbSerialParams.BaudRate = baudRate;
    if (!SetCommState(hSerial, &dcbSerialParams)) {
        fprintf(stderr, "Error setting serial port state\n");
        CloseHandle(hSerial);
        return 1;
    }

    // 设置超时参数
    timeouts.ReadIntervalTimeout = 0;         // 读间隔超时
    timeouts.ReadTotalTimeoutConstant = TIMEOUT_INTERVAL;      // 读取操作的总超时时间
    timeouts.ReadTotalTimeoutMultiplier = 0;    // 每个字节的超时时间
    timeouts.WriteTotalTimeoutConstant = 0;     // 写操作的总超时时间
    timeouts.WriteTotalTimeoutMultiplier = 0;   // 每个字节的超时时间
    if (!SetCommTimeouts(hSerial, &timeouts)) {
        fprintf(stderr, "Error setting serial port timeouts\n");
        CloseHandle(hSerial);
        return 1;
    }

    // 读取串口数据并写入文件
    while (1) {
        if (!ReadFile(hSerial, data, MAX_DATA_LENGTH, &bytesRead, NULL)) {
            fprintf(stderr, "Error reading from serial port\n");
            CloseHandle(hSerial);
            if (recording) {
                fclose(file);
                recording = 0;
            }
            return 1;
        }
        if (bytesRead > 0) {
            idle_time_cnt = 0;
            // 检查接收到的数据是否符合模式
            for (int i = 0; i < bytesRead; i++) {
                if (recording) {
                    bytesTotalcnt++;
                    fwrite(&data[i], sizeof(char), 1, file);
                    //fflush(file); // 确保数据写入文件
                } else {
                    // 符合开始模式，开始记录数据
                    if (data[i] == startPattern[startIdx]) {
                        startIdx++;
                        if (startIdx == START_PATTERN_LEN) {
                            // 完全匹配开始模式，开始记录数据
                            char fileName[100];

                            recording = 1;
                            startIdx = 0;
                            bytesTotalcnt = 8;

                            if(continuous_flag) {
                                // 创建带有时间戳的文件名
                                time_t rawtime;
                                struct tm *timeinfo;
                                char timeBuffer[80];
                                time(&rawtime);
                                timeinfo = localtime(&rawtime);
                                strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d-%H-%M-%S", timeinfo);
                                
                                sprintf(fileName, "%s_%s.bin", output_name, timeBuffer);                                
                            }
                            else {
                                sprintf(fileName, "%s", output_name);
                            }

                            file = fopen(fileName, "wb");

                            if (file == NULL) {
                                fprintf(stderr, "Error opening file\n");
                                CloseHandle(hSerial);
                                return 1;
                            }
                            fwrite(startPattern, sizeof(char), START_PATTERN_LEN, file);
                            //fflush(file); // 确保数据写入文件

                            printf("\nstart to save framebuff in %s\n", fileName);
                        }
                    } else {
                        startIdx = 0; // 重置开始模式匹配索引
                        printf("%c", data[i]);
                    }
                }
            }
        } else {
            // 检查是否超时
            if (recording) {
                printf("%d bytes save completed\n", bytesTotalcnt);
                fflush(file); // 确保数据写入文件
                fclose(file);
                recording = 0;
                bytesTotalcnt = 0;

                if(continuous_flag == 0)
                    break;
            }
            else {
                if(idle_time_cnt == 0) {
                    printf("wait       \r");
                    idle_time_cnt++;  
                }
                else if(idle_time_cnt == 1) {
                    printf("wait .\r"); 
                    idle_time_cnt++; 
                }
                else if(idle_time_cnt == 2) {
                    printf("wait ..\r");
                    idle_time_cnt++;
                }
                else if(idle_time_cnt == 3) {
                    printf("wait ...\r");
                    idle_time_cnt++;
                }
                else if(idle_time_cnt == 4) {
                    printf("wait ....\r");
                    idle_time_cnt++;
                }
                else if(idle_time_cnt == 5) {
                    printf("wait .....\r");
                    idle_time_cnt = 0;
                }
                fflush(stdout);  
            }

        }
    }

    // 关闭文件和串口
    fclose(file);
    CloseHandle(hSerial);
    return 0;
}
