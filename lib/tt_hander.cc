/*
*
* author: ChenFawang
* tt_hander.cc
* 2018 - 11 - 12
* mail: cfwang_9984@163.com
*
*/
#include "tt_hander.h"
#include "tt_log.h"
#include "tt_global.h"
#include "tt_path.h"

#include <fstream>
#include <sstream>
namespace task_top {

TaskTopHandler::TaskTopHandler(const std::list<int>& pids) {
  pids_ = pids;
  sys_stat_.idle = -1;
  apps_stat_.clear();
}
TaskTopHandler::TaskTopHandler(const int * pids, int pid_count) {
  pids_.clear();
  int i;
  for (i = 0; i < pid_count; i++) {
    pids_.push_back(pids[i]);
  }
  sys_stat_.idle = -1;
  apps_stat_.clear();
}
TaskTopHandler::~TaskTopHandler() {
}
void TaskTopHandler::GetCpuInfo(std::list<AppCpuInfoAll>& infos, SystemCpuInfo & sys_info) {
  SystemStatData cur_sys_stat;
  std::list<AppStatAll> cur_app_stat;
  infos.clear();
  memset(&sys_info, 0, sizeof(SystemCpuInfo));
  if (pids_.size() <= 0) {
    return;
  }
  cur_app_stat.clear();
  GetAllStat(cur_sys_stat, cur_app_stat, pids_);
  GetAllCpu(infos, sys_info, cur_sys_stat, cur_app_stat);

}
bool TaskTopHandler::GetAllStat(SystemStatData & sys_stat, std::list<AppStatAll>& apps_stat, std::list<int> &pids) {
  int ret;
  memset(&sys_stat, 0, sizeof(SystemStatData));
  apps_stat.clear();
  std::string sys_stat_str;
  std::list<AppStatStrAll> apps_stat_str;
  if (!GetAllStatString(pids, sys_stat_str, apps_stat_str)) {
    return false;
  }
  ret = task_top::SystemStatResolv::Resolv(sys_stat_str, sys_stat);
  if (ret != 0) {
    LogError("resolv system stat string failed\n");
    return false;
  }
  for (auto app = apps_stat_str.begin(); app != apps_stat_str.end(); app++) {
    AppAndTaskStatData app_stat_data;
    ret = task_top::AppAndTaskStatResolv::Resolv(app->stat_str, app_stat_data);
    if (ret != 0) {
      LogError("resolv app(pid:%d) stat string failed:%d\n",app->pid, ret);
      continue;
    }
    AppStatAll app_stat;
    app_stat.tasks_stat.clear();
    for (auto task = app->thrs_stat.begin(); task != app->thrs_stat.end(); task++) {
      AppAndTaskStatData task_stat_data;
      ret = task_top::AppAndTaskStatResolv::Resolv(task->stat_str, task_stat_data);
      if (ret != 0) {
        LogError("resolv app(pid:%d) thread(tid:%d) stat string failed:%d\n", app->pid, task->tid, ret);
        continue;
      }
      TaskStat task_stat;
      task_stat.tid = task->tid;
      memcpy(&task_stat.stat, &task_stat_data, sizeof(AppAndTaskStatData));
      app_stat.tasks_stat.push_back(task_stat);
    }
    if (app_stat.tasks_stat.size() > 0) {
      apps_stat.push_back(app_stat);
    }
  }
  if (apps_stat.size() <= 0) {
    return false;
  }
  return true;
}
void TaskTopHandler::GetAllCpu(std::list<AppCpuInfoAll>& infos_out, SystemCpuInfo & sys_info_out, SystemStatData & sys_stat_in, std::list<AppStatAll>& apps_stat_in) {
  infos_out.clear();
  int total_diff = 1;
  if (sys_stat_.idle == -1) {
    memset(&sys_info_out, 0, sizeof(SystemCpuInfo));
    memcpy(&sys_stat_, &sys_stat_in, sizeof(SystemStatData));
    return;
  } else {
    total_diff = sys_stat_in.total - sys_stat_.total;
    if (total_diff == 0) {
      memset(&sys_info_out, 0, sizeof(SystemCpuInfo));
      memcpy(&sys_stat_, &sys_stat_in, sizeof(SystemStatData));
      return;
    } else {
      sys_info_out.idle_ = ((sys_stat_in.idle - sys_stat_.idle) * 1.0) / total_diff;
      sys_info_out.io_ = ((sys_stat_in.io - sys_stat_.io) * 1.0) / total_diff;
      sys_info_out.irq_ = ((sys_stat_in.irq - sys_stat_.irq) * 1.0) / total_diff;
      sys_info_out.nic_ = ((sys_stat_in.nice - sys_stat_.nice) * 1.0) / total_diff;
      sys_info_out.sirq_ = ((sys_stat_in.sirq - sys_stat_.sirq) * 1.0) / total_diff;
      sys_info_out.sys_ = ((sys_stat_in.sys - sys_stat_.sys) * 1.0) / total_diff;
      sys_info_out.user_ = ((sys_stat_in.user - sys_stat_.user) * 1.0) / total_diff;
    }
  }
  memcpy(&sys_stat_, &sys_stat_in, sizeof(SystemStatData));


  for (auto app = apps_stat_in.begin(); app != apps_stat_in.end(); app++) {
    AppCpuInfoAll app_cpu_info;
    app_cpu_info.pid = app->pid;
    GetAppCpuInfo(app->pid, app->app_stat, total_diff, app_cpu_info.app_cpu);
    app_cpu_info.name = task_top::AppTaskScan::GetAppName(app->pid);
    app_cpu_info.task.clear();
    for (auto task = app->tasks_stat.begin(); task != app->tasks_stat.end(); task++) {
      TaskInfo task_cpu_info;
      task_cpu_info.tid = task->tid;
      GetTaskCpuInfo(app->pid, task->tid, task->stat, total_diff, task_cpu_info.cpu);
      task_cpu_info.name = task_top::AppTaskScan::GetThrName(app->pid, task->tid);
      app_cpu_info.task.push_back(task_cpu_info);
    }
    infos_out.push_back(app_cpu_info);
  }
  apps_stat_.clear();
  apps_stat_ = apps_stat_in;
}
bool TaskTopHandler::GetAllStatString(std::list<int>& pids, std::string & sys_stat_str, std::list<AppStatStrAll>& apps_stat_str) {
  if (!GetSystemStatString(sys_stat_str)) {
    return false;
  }
  int ret;
  apps_stat_str.clear();
  for (auto pid = pids.begin(); pid != pids.end();) {
    AppStatStrAll app_stat_str;
    app_stat_str.pid = *pid;
    if (!GetOneAppStatString(app_stat_str.pid, app_stat_str.stat_str)) {
      LogError("pid: %d exe get stat info failed", *pid);
      pid = pids.erase(pid);
      continue;
    }
    std::list<int> tids;
    ret = task_top::AppTaskScan::GetTids(*pid, tids);
    if (ret != 0) {
      LogError("get tids for pid:%d failed:%d\n", *pid, ret);
      pid = pids.erase(pid);
      continue;
    }
    for (auto tid = tids.begin(); tid != tids.end(); tid++) {
      TaskStatStr task_stat;
      task_stat.tid = *tid;
      if (!GetOneTaskStatString(app_stat_str.pid, task_stat.tid, task_stat.stat_str)) {
        continue;
      }
      app_stat_str.thrs_stat.push_back(task_stat);
    }
    if (tids.size() == 0) {
      LogError("pid:%d tids num is zero\n", *pid);
      pid = pids.erase(pid);
      continue;
    }
    apps_stat_str.push_back(app_stat_str);
    pid++;
  }
  if (apps_stat_str.size() <= 0) {
    return false;
  }
  return true;
}
bool TaskTopHandler::GetSystemStatString(std::string & stat_data) {
  std::ifstream ifs(task_top::global::g_system_stat_path_, std::ios::in);
  if (!ifs.is_open()) {
    LogError("open cpu stat file: %s failed\n", task_top::global::g_system_proc_path_.c_str());
    return false;
  }
  if (!std::getline(ifs, stat_data)) {
    LogError("get file %s one line string failed\n", task_top::global::g_system_proc_path_.c_str());
    ifs.close();
    return false;
  }
  ifs.close();
  return true;
}
bool TaskTopHandler::GetOneAppStatString(int pid, std::string & stat_data) {
  Path path(task_top::global::g_system_proc_path_);
  path.AppendFolder(task_top::to_string(pid));
  path.SetFile(task_top::global::g_stat_filename_);
  return GetOneAppOrTaskStatString(path.Pathname(), stat_data);
}
bool TaskTopHandler::GetOneTaskStatString(int pid, int tid, std::string & stat_data) {
  Path path(task_top::global::g_system_proc_path_);
  path.AppendFolder(task_top::to_string(pid));
  path.AppendFolder(task_top::global::g_task_folder_);
  path.AppendFolder(task_top::to_string(tid));
  path.SetFile(task_top::global::g_stat_filename_);
  return GetOneAppOrTaskStatString(path.Pathname(), stat_data);
}
bool TaskTopHandler::GetOneAppOrTaskStatString(const std::string & stat_path, std::string & stat_data) {
  std::ifstream ifs(stat_path);
  if (!ifs.is_open()) {
    LogError("open file: %s failed\n", stat_path.c_str());
    return false;
  }
  if (!std::getline(ifs, stat_data)) {
    ifs.close();
    return false;
  }
  return true;
}
bool TaskTopHandler::GetOldAppStatData(int pid, AppAndTaskStatData & old_stat) {
  for (auto app = apps_stat_.begin(); app != apps_stat_.end(); app++) {
    if (app->pid == pid) {
      memcpy(&old_stat, &app->app_stat, sizeof(AppAndTaskStatData));
      return true;
    }
  }
  return false;
}
bool TaskTopHandler::GetOldTaskStatData(int pid, int tid, AppAndTaskStatData & old_stat) {
  for (auto app = apps_stat_.begin(); app != apps_stat_.end(); app++) {
    if (app->pid == pid) {
      for (auto task = app->tasks_stat.begin(); task != app->tasks_stat.end(); task++) {
        if (task->tid == tid) {
          memcpy(&old_stat, &task->stat, sizeof(AppAndTaskStatData));
          return true;
        }
      }
    }
  }
  return false;
}
void TaskTopHandler::GetAppCpuInfo(int pid, AppAndTaskStatData & cur_stat_data, int total_diff, AppAndTaskCpuInfo & cpu_info) {
  AppAndTaskStatData old_stat;
  if (!GetOldAppStatData(pid, old_stat)) {
    memset(&cpu_info, 0, sizeof(AppAndTaskCpuInfo));
  } else {
    cpu_info.cstime = ((cur_stat_data.cstime - old_stat.cstime)*1.0) / total_diff;
    cpu_info.cutime = ((cur_stat_data.cutime - old_stat.cutime)*1.0) / total_diff;
    cpu_info.stime = ((cur_stat_data.stime - old_stat.stime)*1.0) / total_diff;
    cpu_info.utime = ((cur_stat_data.utime - old_stat.utime)*1.0) / total_diff;
  }
}
void TaskTopHandler::GetTaskCpuInfo(int pid, int tid, AppAndTaskStatData & cur_stat_data, int total_diff, AppAndTaskCpuInfo & cpu_info) {
  AppAndTaskStatData old_stat;
  if (!GetOldTaskStatData(pid, tid, old_stat)) {
    memset(&cpu_info, 0, sizeof(AppAndTaskCpuInfo));
  } else {
    cpu_info.cstime = ((cur_stat_data.cstime - old_stat.cstime)*1.0) / total_diff;
    cpu_info.cutime = ((cur_stat_data.cutime - old_stat.cutime)*1.0) / total_diff;
    cpu_info.stime = ((cur_stat_data.stime - old_stat.stime)*1.0) / total_diff;
    cpu_info.utime = ((cur_stat_data.utime - old_stat.utime)*1.0) / total_diff;
  }
}
}