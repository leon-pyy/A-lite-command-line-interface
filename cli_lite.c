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

#include "stdio.h"
#include "stdbool.h"
#include "string.h"
#include "stdlib.h"
#include "stdarg.h"
#include "cli_lite.h"
#include "usart.h"

void caclu_add(void);
void caclu_sub(void);
void caclu_mul(void);
void caclu_div(void);

/* 命令列表，新的命令请添加到这里 */
/* 命令函数-命令名-命令说明 */
_cmd_table cmd_table[]={
    (void *)caclu_add,"add","add [parm1] [parm2]",
    (void *)caclu_sub,"sub","sub [parm1] [parm2]",
    (void *)caclu_mul,"mul","mul [parm1] [parm2]",
    (void *)caclu_div,"div","div [parm1] [parm2]",
};

int cmdnum = sizeof(cmd_table)/sizeof(_cmd_table);	//命令的数量
char token[CMD_PARMNUM][CMD_LONGTH]={0};	//一条命令的解析缓存
static uint16_t rx_index = 0;     // 命令长度
static uint16_t cursor_pos = 0;   // 光标位置

static char cmd_history[CMD_HISTORY_NUM][CMD_MAX_LEN];	//历史命令缓存
static int history_count = 0;	//历史命令计数
static int history_index = -1;	//历史命令索引

uint8_t rx_buffer[USART_REC_LEN]={0};	//串口接收的buffer
uint8_t rx_data;	//串口接收的一个字节数据
static esc_state_t esc_state = ESC_IDLE;	//方向键状态
bool cmd_deal_ok = 1;	//命令执行锁

static char log_buf[128]={0};	//日志buffer

/* 串口发送回显 */
void uart_echo(uint8_t *data,uint16_t len){
    HAL_UART_Transmit(&huart1,data,len,50);
}

/* 判断前缀 */
static bool str_start_with(const char *str, const char *prefix){
    while (*prefix){
        if (*str++ != *prefix++)
            return false;
    }
    return true;
}

/* 保存历史命令 */
static void history_save(const char *cmd){
    if (cmd[0] == '\0') return;

    int idx = history_count % CMD_HISTORY_NUM;
    strncpy(cmd_history[idx], cmd, CMD_MAX_LEN - 1);
    cmd_history[idx][CMD_MAX_LEN - 1] = '\0';

    history_count++;
    history_index = history_count;
}

/* 清除当前行 */
static void clear_line(void){
    while (cursor_pos > 0){
        uint8_t bs_seq[3] = {'\b', ' ', '\b'};
        uart_echo(bs_seq, 3);
        cursor_pos--;
    }
}

/* 方向键←处理 */
static void cursor_left(void){
    if (cursor_pos > 0){
        uart_echo((uint8_t *)"\b", 1);
        cursor_pos--;
    }
}

/* 方向键→处理 */
static void cursor_right(void){
    if (cursor_pos < rx_index){
        uart_echo(&rx_buffer[cursor_pos], 1);
        cursor_pos++;
    }
}

/* 方向键↑处理 */
static void cmd_history_up(void){
    if (history_count == 0) return;
    int oldest = history_count - CMD_HISTORY_NUM;
    if (oldest < 0) oldest = 0;

    if (history_index <= oldest)
        return;

    history_index--;

    while (cursor_pos < rx_index){
        cursor_right();
    }
        
    clear_line();

    strcpy((char *)rx_buffer,
           cmd_history[history_index % CMD_HISTORY_NUM]);

    rx_index = strlen((char *)rx_buffer);
    cursor_pos = rx_index;
    uart_echo(rx_buffer, rx_index);
}

/* 方向键↓处理 */
static void cmd_history_down(void){
    if (history_count == 0) return;

    /* 已经在空行，不能再往下 */
    if (history_index >= history_count)
        return;
    
    history_index++;

    /* 到达空行 */
    if (history_index == history_count)
    {
        while (cursor_pos < rx_index)
            cursor_right();

        clear_line();
        rx_index = 0;
        cursor_pos = 0;
        rx_buffer[0] = '\0';
        return;
    }

    while (cursor_pos < rx_index){
        cursor_right();
    }

    clear_line();
    strcpy((char *)rx_buffer,
           cmd_history[history_index % CMD_HISTORY_NUM]);

    rx_index = strlen((char *)rx_buffer);
    cursor_pos = rx_index;
    uart_echo(rx_buffer, rx_index);
}

/* 串口接收回调 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance != USART1) return;

    if (!cmd_deal_ok) goto rx_exit;

    /* 方向键 */
    if (esc_state != ESC_IDLE){
        if (esc_state == ESC_START){
            esc_state = (rx_data == '[') ? ESC_BRACKET : ESC_IDLE;
            goto rx_exit;
        }
        else if (esc_state == ESC_BRACKET){
            esc_state = ESC_IDLE;
            switch (rx_data){
                case 'A': cmd_history_up();   break;
                case 'B': cmd_history_down(); break;
                case 'C': cursor_right();     break;
                case 'D': cursor_left();      break;
                default: break;
            }
            goto rx_exit;
        }
    }

    if (rx_data == 0x1B){
        esc_state = ESC_START;
        goto rx_exit;
    }

    /* 回车 */
    if (rx_data == CMD_CR || rx_data == CMD_LF){
        rx_buffer[rx_index] = '\0';
        history_save((char *)rx_buffer);

        rx_index = 0;
        cursor_pos = 0;
        esc_state = ESC_IDLE;
        cmd_deal_ok = 0;
        goto rx_exit;
    }

    /* 退格（支持行内删除）*/
    if (rx_data == CMD_BS){
        if (cursor_pos > 0){
            memmove(&rx_buffer[cursor_pos - 1],
                    &rx_buffer[cursor_pos],
                    rx_index - cursor_pos);

            rx_index--;
            cursor_pos--;

            uart_echo((uint8_t *)"\b", 1);
            uart_echo(&rx_buffer[cursor_pos],
                       rx_index - cursor_pos);
            uart_echo((uint8_t *)" ", 1);

            for (int i = 0; i <= rx_index - cursor_pos; i++)
                uart_echo((uint8_t *)"\b", 1);
        }
        goto rx_exit;
    }

    /* TAB补全 */
    if (rx_data == CMD_HT){
        uint8_t match = 0;
        const char *last = NULL;

        rx_buffer[rx_index] = '\0';

        for (int i = 0; i < cmdnum; i++){
            if (str_start_with(cmd_table[i].name,
                               (char *)rx_buffer)){
                match++;
                last = cmd_table[i].name;
            }
        }

        if (match == 1 && last){
            const char *p = last + rx_index;
            while (*p && rx_index < USART_REC_LEN - 1){
                rx_buffer[rx_index++] = *p;
                uart_echo((uint8_t *)p, 1);
                p++;
            }
            cursor_pos = rx_index;
        }
        else if (match > 1){
            const char nl[] = "\r\n";
            uart_echo((uint8_t *)nl, 2);
            for (int i = 0; i < cmdnum; i++){
                if (str_start_with(cmd_table[i].name,
                                   (char *)rx_buffer)){
                    uart_echo((uint8_t *)cmd_table[i].name,
                               strlen(cmd_table[i].name));
                    uart_echo((uint8_t *)nl, 2);
                }
            }
            const char prompt[] = "[leon]@leon:";
            uart_echo((uint8_t *)prompt, strlen(prompt));
            uart_echo(rx_buffer, rx_index);
        }
        goto rx_exit;
    }

    /* 普通可显示字符 */
    if (rx_data >= 0x20 && rx_data <= 0x7E){
        if (rx_index < USART_REC_LEN - 1){
            memmove(&rx_buffer[cursor_pos + 1],
                    &rx_buffer[cursor_pos],
                    rx_index - cursor_pos);

            rx_buffer[cursor_pos++] = rx_data;
            rx_index++;

            uart_echo(&rx_buffer[cursor_pos - 1],
                       rx_index - cursor_pos + 1);

            for (int i = 0; i < rx_index - cursor_pos; i++)
                uart_echo((uint8_t *)"\b", 1);
        }
    }

rx_exit:
    HAL_UART_Receive_IT(&huart1, &rx_data, 1);
}

/* 执行命令 */
void execute_cmd(void){
    int cmd_is_find = 0;	//标记是否找到命令
    if(strlen(token[0])!=0){
        if(!strcmp(token[0],"ls")){	//如果输入的是打印命令列表
			printf("-------------------- cmd table --------------------\r\n");
            for(int i=0;i<cmdnum;i++){
                printf("cmd:%s    eg:%s\r\n",cmd_table[i].name,cmd_table[i].example);
            }
            printf("---------------------------------------------------\r\n");
        }
        else{
            for(int j=0;j<cmdnum;j++){	//找寻需要执行的命令
                if(!strcmp(token[0],cmd_table[j].name)){
                    cmd_table[j].func();	//执行命令函数
                    cmd_is_find++;
                }
            }
            if(cmd_is_find==0){	//未找到命令
                printf("cmd error!\r\n");
            }
        }
    }

    //命令执行完毕，清空命令缓存
	memset(token,0,CMD_PARMNUM*CMD_LONGTH);
}

/* 命令处理函数，放在程序主循环中 */
void process_cmd(void){
    char tmp_data='\0';
    int buf_count=0;
    int parm_count=0;
    int str_count=0;

    if(cmd_deal_ok==0){
		int rx_len = strlen((const char*)rx_buffer);
        for(int i=0;i<rx_len;i++){
            tmp_data = rx_buffer[buf_count];
            if((tmp_data!='\r')||(tmp_data!='\n')||(tmp_data!='\0')){
                if(tmp_data!=' '){
                    token[parm_count][str_count] = tmp_data;
                    if(str_count<CMD_LONGTH-1){
                        str_count++;
                    }
                }
                else{
                    token[parm_count][str_count] = '\0'; //将空格替换为字符串结束符
                    if(parm_count<CMD_PARMNUM-1){
                        parm_count++;
                    }
                    str_count = 0;
                }
            }
            if(buf_count<USART_REC_LEN-1){
                buf_count++;
            }
        }
		printf("\r\n");
        execute_cmd();	//执行命令
        memset(rx_buffer,0,USART_REC_LEN);	//清空串口buffer
        cmd_deal_ok = 1;	//解锁
        printf("[leon]@leon:");
    }
}

/* 命令的具体示例 */
void caclu_add(void){
    int parm1=0,parm2=0;
    //参数有效性判定
    if(strlen(token[1])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }
    if(strlen(token[2])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }

    parm1 = atoi(token[1]);
    parm2 = atoi(token[2]);
    printf("add = %d \r\n",parm1+parm2);
}

void caclu_sub(void){
    int parm1=0,parm2=0;
    //参数有效性判定
    if(strlen(token[1])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }
    if(strlen(token[2])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }

    parm1 = atoi(token[1]);
    parm2 = atoi(token[2]);
    printf("sub = %d \r\n",parm1-parm2);
}

void caclu_mul(void){
    int parm1=0,parm2=0;
    //参数有效性判定
    if(strlen(token[1])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }
    if(strlen(token[2])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }

    parm1 = atoi(token[1]);
    parm2 = atoi(token[2]);
    printf("mul = %d \r\n",parm1*parm2);
}

void caclu_div(void){
    int parm1=0,parm2=0;
    //参数有效性判定
    if(strlen(token[1])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }
    if(strlen(token[2])==0){
        printf("cmd parm invalid!\r\n");
        return;
    }

    parm1 = atoi(token[1]);
    parm2 = atoi(token[2]);
    printf("div = %d \r\n",parm1/parm2);
}
