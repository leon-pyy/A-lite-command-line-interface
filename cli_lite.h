/*******************************************************************************
MIT License

Copyright (c) 2023 leon-pyy

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#ifndef __CLI_LITE_H__
#define __CLI_LITE_H__

#define USART_REC_LEN               128     //定义串口一次接收的最大字节数
#define CMD_HISTORY_NUM             10      //定义历史命令记录条数
#define CMD_MAX_LEN                 128     //命令一条命令最大的长度
#define CMD_PARMNUM                 8       //每条命令支持的最多参数个数
#define CMD_LONGTH                  16      //每条命令的参数的最大长度

#define CMD_NU		0x00    //空字符
#define CMD_ETX		0x03    //正文结束
#define CMD_BS		0x08    //退格键，BackSpace键
#define CMD_HT		0x09    //水平制表符，TAB键
#define CMD_LF		0x0a    //换行键
#define CMD_CR		0x0d    //回车键，Enter键

/* 命令结构体 */
typedef struct _cmd_table{
    void(*func)(void);          //命令执行函数
    const char* name;           //命令名字（查找字符串）
    const char* example;        //命令说明
}_cmd_table;

/* 状态枚举 */
typedef enum {
    ESC_IDLE = 0,
    ESC_START,
    ESC_BRACKET
} esc_state_t;

void process_cmd(void);

#endif
