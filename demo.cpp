#include <ctime>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string.h>
#include <math.h>

using namespace std;

#define LEAP_SECONDS 18 // 润秒
#define TIMESTAMP_1980_01_06 315964800.0

// ","分割的rtk数据中周、周内秒、经纬高的位置
#define WEEK_NUM   5
#define WIS_NUM    6
#define LAT_NUM   12
#define LONG_NUM  13
#define H_NUM     14

#define BUF_SIZE 512
const char* MATCH_STR_1 = "#BESTPOSA";  // #BESTPOSA
const char* MATCH_STR_2 = "NARROW_INT"; // NARROW_INT
const char* SPLIT = ",;"; // 以,;分割

/**
 * @brief 周、周内秒转时间戳
 * @param[in] rtk_week     周
 * @param[in] rtk_wis      周内秒
 * @param[in] leap_seconds 润秒，默认为18s
 * @return 时间戳
*/
double weekwis2timestamp(double rtk_week, double rtk_wis, int leap_seconds = LEAP_SECONDS) {
    const int week_seconds = 604800; // = 7*24*60*60; // 一周的秒数
    /**
     * @note GPS周、周内秒是从1980.01.06开始的，时间戳的计算是从1970.01.01开始，
     * 所以需要加上315964800（两个时间起始相差的固定秒数）；
    */
    return rtk_week*week_seconds + rtk_wis + TIMESTAMP_1980_01_06 - leap_seconds;
}

/**
 * @brief UTC转时间戳
 * @param[in] utc_str UTC格式的时间
 * @param[in] seconds_after_point 导入的数据中秒是含小数的，单独处理小数
 * @return 时间戳
*/
double utc2timestamp(string utc_str, double seconds_after_point) {
    istringstream itm(utc_str);
    tm tm_time = {};
    itm >> get_time(&tm_time, "%Y-%m-%d %H:%M:%S"); // ?
    return mktime(&tm_time) + seconds_after_point;
}

/**
 * @brief 从UTC文件中提取时间戳
 * @param[in] file_addr 文件路径
 * @return 全部时刻的 时间戳
*/
vector<double> get_ts_from_utc_file(string file_addr) {
    ifstream fin;
    fin.open(file_addr, ios::in);
    char c_buffer[BUF_SIZE] = {0}; // 提取文件中某一行的全部字符
    vector<double> ts;             // 全部时刻的 时间戳
    if(!fin) { // 确保文件可打开
        cerr << "cannot open the file from " << file_addr << endl;
    } else {
        while(fin.getline(c_buffer, BUF_SIZE)) { // 循环获取文件中的数据
            char *p_buffer = strtok(c_buffer, "."); // 用.分割字符串
            string utc = p_buffer; // 规范的UTC格式的数据
            p_buffer = strtok(NULL, ".");
            double seconds_after_point = 0.001*stof(p_buffer); // 小数部分
            ts.push_back(utc2timestamp(utc, seconds_after_point));
        }
    }
    fin.close();
    return ts;
}

/**
 * @brief 从文件中提取时间戳、经纬高
 * @param[in] file_addr 文件路径
 * @return 全部时刻的 时间戳 维度 经度 高
*/
vector<vector<double>> get_t_and_enu_from_file(string file_addr) {
    ifstream fin;
    fin.open(file_addr, ios::in);
    char c_buffer[BUF_SIZE] = {0}; // 提取文件中的某一行的全部字符
    vector<vector<double>> t_and_enu; // 全部时刻的 时间戳 维度 经度 高
    if(!fin) { // 确保文件可打开
        cerr << "cannot open the file from " << file_addr << endl;
    } else {
        while(fin.getline(c_buffer, BUF_SIZE)) { // 循环获取文件中的数据
            // 只提取特定行的数据
            if(strstr(c_buffer, MATCH_STR_1) && strstr(c_buffer, MATCH_STR_2)) {
                vector<double> one_t_and_enu;              // 某一时刻的 时间戳 维度 经度 高
                char *p_buffer = strtok(c_buffer, SPLIT);  // 用SPLIT分割字符串
                double week = 0, wis = 0;                  // 用于暂存周、周内秒
                // for(int i = 0; p_buffer != NULL; i++) {
                for(int i = 0; i <= H_NUM; i++) { // 循环获取被分割字符串的每一部分
                    // 提取周、周内秒、经纬高
                    if(i == WEEK_NUM)       week = stod(p_buffer);
                    else if(i == WIS_NUM)   wis  = stod(p_buffer);
                    else if(i == LAT_NUM)   one_t_and_enu.push_back(stod(p_buffer));
                    else if(i == LONG_NUM)  one_t_and_enu.push_back(stod(p_buffer));
                    else if(i == H_NUM)     one_t_and_enu.push_back(stod(p_buffer));
                    p_buffer = strtok(NULL, SPLIT);
                }
                // 将时间戳插入最前
                one_t_and_enu.insert(one_t_and_enu.begin(), weekwis2timestamp(week, wis));
                // 某一时刻 时间戳 维度 经度 高 -> 全部时刻
                t_and_enu.push_back(one_t_and_enu);
            } else continue;
        }
    }
    fin.close();
    return t_and_enu;
}

/**
 * @brief 使两个不同起始、不同结束的时间戳序列相同起始和结束
 * @param 两个时间戳序列
*/
void align_ts_series(vector<vector<double>>* ts_1, vector<vector<double>>* ts_2) {
    // 起始对齐
    vector<vector<double>>::iterator ts_1_b = ts_1->begin(), ts_2_b = ts_2->begin();
    while(1) {
        if(ts_1_b->front() == ts_2_b->front()) break;
        else {
            if(ts_1_b->front() < ts_2_b->front()) ts_1_b++;
            else ts_2_b++;
        }
    }
    // 去除序列开头时间戳不一致的部分
    ts_1->erase(ts_1->begin(), ts_1_b);
    ts_2->erase(ts_2->begin(), ts_2_b);
    // 结束对齐
    vector<vector<double>>::iterator ts_1_e = ts_1->end() - 1, ts_2_e = ts_2->end() - 1;
    while(1) {
        if(ts_1_e->front() == ts_2_e->front()) break;
        else {
            if(ts_1_e->front() > ts_2_e->front()) ts_1_e--;
            else ts_2_e--;
        }
    }
    // 去除序列末尾时间戳不一致的部分
    ts_1->erase(++ts_1_e, ts_1->end());
    ts_2->erase(++ts_2_e, ts_2->end());
}

/**
 * @brief 使两个不同起始、不同结束的时间戳序列相同起始和结束
 * @param 两个时间戳序列
*/
void align_ts_series(vector<vector<double>>* ts_1, vector<double>* ts_2) {
    // 起始对齐
    vector<vector<double>>::iterator ts_1_b = ts_1->begin();
    vector<double>::iterator ts_2_b = ts_2->begin();
    while(1) {
        double abs_ = abs(ts_1_b->front() - *ts_2_b);
        if(abs_ < 0.2) break;
        else {
            if(ts_1_b->front() < *ts_2_b) ts_1_b++;
            else ts_2_b++;
        }
    }
    // 去除序列开头时间戳不一致的部分
    ts_1->erase(ts_1->begin(), ts_1_b);
    ts_2->erase(ts_2->begin(), ts_2_b);
    // 结束对齐
    vector<vector<double>>::iterator ts_1_e = ts_1->end() - 1;
    vector<double>::iterator ts_2_e = ts_2->end() - 1;
    while(1) {
        double abs_ = abs(ts_1_e->front() - *ts_2_e);
        if(abs_ < 0.2) break;
        else {
            if(ts_1_e->front() > *ts_2_e) ts_1_e--;
            else ts_2_e--;
        }
    }
    // 去除序列末尾时间戳不一致的部分
    ts_1->erase(++ts_1_e, ts_1->end());
    ts_2->erase(++ts_2_e, ts_2->end());
}

/**
 * @brief 使两个不同起始、不同结束的时间戳序列相同起始和结束
 * @param 两个时间戳序列
*/
void align_ts_series(vector<double>* ts_1, vector<vector<double>>* ts_2) {
    align_ts_series(ts_2, ts_1);
}

/**
 * @brief  计算某一时刻的航向角和俯仰角
 * @param[in] t_enu_f 机体前段某一时刻的rtk数据
 * @param[in] t_enu_b 机体后段某一时刻的rtk数据
 * @note   enu_f和enu_b的格式均为：[时间戳, 维度, 经度, 高]
 * @return 某一时刻的 时间戳 航向角 俯仰角
*/
vector<double> one_time_head_and_pitch(vector<double> t_enu_f, vector<double> t_enu_b) {
    double head = 0, pitch = 0;  // 航向 俯仰
    vector<double> t_h_p;        // 某一时刻的 时间戳 航向 俯仰
    head = atan2(t_enu_f[2] - t_enu_b[2], t_enu_f[1] - t_enu_b[1]);
    pitch = atan2(
        sqrt(pow(t_enu_f[2] - t_enu_b[2], 2) + pow(t_enu_f[1] - t_enu_b[1], 2)),
        t_enu_f[3] - t_enu_b[3]
    );
    t_h_p.push_back(t_enu_f[0]); // 存入时间戳
    t_h_p.push_back(head);       // 存入航向角
    t_h_p.push_back(pitch);      // 存入俯仰角
    return t_h_p;
}

/**
 * @brief  计算全部时刻的航向角和俯仰角
 * @param[in] t_enu_f 机体前段全部时刻的rtk数据
 * @param[in] t_enu_b 机体后段全部时刻的rtk数据
 * @note   t_enu_f和t_enu_b格式均为：[[时间戳, 维度, 经度, 高], ...]
 * @return 全部时刻的 时间戳 航向角 俯仰角
*/
vector<vector<double>> compute_head_and_pitch(
    vector<vector<double>> t_enu_f, // 机体前段全部时刻的rtk数据
    vector<vector<double>> t_enu_b  // 机体后段全部时刻的rtk数据
) {
    vector<vector<double>> t_h_p;   // 全部时刻的 时间戳 航向角 俯仰角
    vector<vector<double>>::iterator t_enu_f_iter = t_enu_f.begin(); // 前段rtk数据迭代器
    vector<vector<double>>::iterator t_enu_b_iter = t_enu_b.begin(); // 后段rtk数据迭代器
    if(t_enu_f_iter->front() != t_enu_b_iter->front()) { // 确保时间戳一致
        cerr << "时间戳不一致, 请先调用align_ts_series..." << endl;
        exit(-1);
    } else { // 若时间戳一致，则
        while(t_enu_f_iter != t_enu_f.end()) {
            t_h_p.push_back( // 计算此时的航向角和俯仰角，并记录
                one_time_head_and_pitch(*t_enu_f_iter, *t_enu_b_iter)
            );
            t_enu_f_iter++;
            t_enu_b_iter++;
        }
    }
    return t_h_p;
}

/**
 * @brief  根据前后时刻的 时间戳 (数据...) 计算中间时刻的 时间戳 (数据...)
 * @note   t_h_p_past[0]和t_h_p_future[0]需为时间戳
 * @param[in] t            中间时刻
 * @param[in] t_h_p_past   前一时刻的 时间戳 (数据...)
 * @param[in] t_h_p_future 后一时刻的 时间戳 (数据...)
 * @return 中间时刻的 时间戳 (数据...)
*/
vector<double> middle_t_data(double t, vector<double> t_data_past, vector<double> t_data_future) {
    vector<double> middle_t_d;
    vector<double>::iterator t_d_p_iter = t_data_past.begin(), t_d_f_iter = t_data_future.begin();
    double t_p = *t_d_p_iter, t_f = *t_d_f_iter;
    middle_t_d.push_back(t);
    t_d_p_iter++;
    t_d_f_iter++;
    while(t_d_p_iter != t_data_past.end()) {
        middle_t_d.push_back(
            (*t_d_f_iter - *t_d_p_iter) * (t - t_p) / (t_f - t_p) + *t_d_p_iter
        );
        t_d_p_iter++;
        t_d_f_iter++;
    }
    return middle_t_d;
}

/**
 * @brief  rtk时刻下的 时间戳 航向角 俯仰角 -> 相机时刻下的
 * @param[in] ts        相机返回的时间戳序列
 * @param[in] rtk_t_h_p rtk时刻下的 时间戳 航向角 俯仰角
 * @return 相机时刻下的 时间戳 航向角 俯仰角
*/
vector<vector<double>> compute_camera_t_h_p(vector<double> ts, vector<vector<double>> rtk_t_h_p) {
    vector<vector<double>> camera_t_h_p;           // 相机时刻下的 时间戳 航向角 俯仰角
    vector<double>::iterator ts_iter = ts.begin(); // 相机时间戳的迭代器
    int index = 0; // rtk记录的 时间戳 航向角 俯仰角 的下标
    while(ts_iter != ts.end()) {
        if(*ts_iter == rtk_t_h_p[index].front()) {
            camera_t_h_p.push_back(rtk_t_h_p[index]);
            ts_iter++;
        } else if(*ts_iter > rtk_t_h_p[index].front()) {
            double tmp = *ts_iter - rtk_t_h_p[index].front();
            if(tmp < 0.1) {
                camera_t_h_p.push_back(
                    middle_t_data(*ts_iter, rtk_t_h_p[index], rtk_t_h_p[index + 1])
                );
                ts_iter++;
            }
            index++;
        } else {
            camera_t_h_p.push_back(
                middle_t_data(*ts_iter, rtk_t_h_p[index - 1], rtk_t_h_p[index])
            );
            ts_iter++;
        }
    }
    return camera_t_h_p;
}

int main(int argc, char **argv) {
    string file_addr_f = "/media/yuxuan/我的文档/image1/前rtk数据.txt";
    string file_addr_b = "/media/yuxuan/我的文档/image1/hou1.txt";
    vector<vector<double>> t_and_enu_f = get_t_and_enu_from_file(file_addr_f);
    vector<vector<double>> t_and_enu_b = get_t_and_enu_from_file(file_addr_b);
    cout << "# " <<   "前后rtk的数据:"   << endl;
    cout << "| " << t_and_enu_f.size() << endl;
    cout << "| " << t_and_enu_b.size() << endl;

    align_ts_series(&t_and_enu_f, &t_and_enu_b);
    cout << "# " << "对齐前后rtk数据的时间戳:" << endl;
    cout << "| " << t_and_enu_f.size() << endl;
    cout << "| " << t_and_enu_b.size() << endl;

    vector<vector<double>> t_h_p = compute_head_and_pitch(t_and_enu_f, t_and_enu_b);
    cout << "# " << "时间戳 航向角 俯仰角:" << endl;
    cout << "| " <<     t_h_p.size()     << endl;

    string file_addr_3 = "/media/yuxuan/我的文档/image1/times.txt";
    vector<double> ts  = get_ts_from_utc_file(file_addr_3);
    cout << "# " <<   "相机记录的时间戳:"   << endl;
    cout << "| " <<      ts.size()       << endl;

    vector<vector<double>> camera_t_h_p = compute_camera_t_h_p(ts, t_h_p);
    cout << "# " <<   "相机记录时间戳下的航向、俯仰:"   << endl;
    cout << "| " <<      camera_t_h_p.size()       << endl;
    return 0;
}
