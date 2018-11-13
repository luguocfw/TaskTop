/*
*
* author: ChenFawang
* at_common.h
* 2018 - 11 - 12
* mail: cfwang_9984@163.com
*
*/
#ifndef __TT_COMM_H__
#define __TT_COMM_H__
#ifdef __cplusplus
extern "C" {
#endif
#define MAX_APP_NUM             (10)

typedef struct {
  int             pids[MAX_APP_NUM];
  int             pid_num;
}TaskTopAppInfo;


int TaskTopInit(const TaskTopAppInfo *apps_info);

void TaskTopExit();




#ifdef __cplusplus
}
#endif /* end of __cplusplus */

#endif