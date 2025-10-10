#include <iostream>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <utils.h>
#include <sys/stat.h>
#include <errno.h>
#include <algorithm>
#include "file_manager.h"
#include "print_stats_c.h"
#include "gcode_preview.h"
#include "hl_common.h"
#include "cJSON.h"

#define LOG_TAG "file_manager"
#undef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#include "log.h"

// singleton 
FileManager* FileManager::instance = nullptr;

FileManager::~FileManager()
{
    SaveFileInfo();  // 保存文件信息
}

void FileManager::DestroyInstance()
{
    if (instance)
    {
        delete instance;
        instance = nullptr;
    }
}


FileManager* FileManager::GetInstance()
{
    if(!instance){
        instance = new FileManager();
    }
    return instance;
}

/*
    @brief : 1.确保存放文件信息的目录存在，不存在则创建 
             2.文件信息文件存在则将其读取到内存数组中
*/
FileManager::FileManager()
{
    // 1. 确保目录 FILEMANAGER_FILEPATH 存在
    struct stat statbuf;
    if (stat(FILEMANAGER_FILEPATH, &statbuf) != 0) 
    {
        if (mkdir(FILEMANAGER_FILEPATH, 0755) == 0) 
            LOG_D("Directory(%s) create successfully\n",FILEMANAGER_FILEPATH);
        else 
            LOG_D("directory(%s) create Error\n",FILEMANAGER_FILEPATH);
    }

    // 2. 加载文件信息
    LoadFileInfo();
}

void FileManager::LoadFileInfo()
{
    // 1. 确保目录存在
    struct stat statbuf;
    if (stat(FILEMANAGER_FILEPATH, &statbuf) != 0) 
    {
        if (mkdir(FILEMANAGER_FILEPATH, 0755) == 0) 
            LOG_D("Directory(%s) create successfully\n", FILEMANAGER_FILEPATH);
        else 
            LOG_D("directory(%s) create Error\n", FILEMANAGER_FILEPATH);
    }

    // 2. 检查文件是否存在
    if(access(FILEMANAGER_FILE(FILEMANAGER_FILEPATH,FILEMANAGER_FILENAME), F_OK) != 0)
    {
        LOG_D("access file(%s) failed\n", FILEMANAGER_FILE(FILEMANAGER_FILEPATH,FILEMANAGER_FILENAME));
        return;
    }

    // 3. 读取文件内容
    std::ifstream file(FILEMANAGER_FILE(FILEMANAGER_FILEPATH,FILEMANAGER_FILENAME));
    if (!file.is_open()) 
    {
        LOG_E("Error opening file (%s)\n", FILEMANAGER_FILE(FILEMANAGER_FILEPATH,FILEMANAGER_FILENAME));
        return;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    std::string content = buffer.str();

    // 4. 尝试解析为JSON
    cJSON *root = cJSON_Parse(content.c_str());
    if (root != nullptr) 
    {
        // JSON 格式处理
        LOG_D("Processing JSON format file\n");
        cJSON *files = cJSON_GetObjectItem(root, "files");
        if (files && cJSON_IsArray(files))
        {
            int array_size = cJSON_GetArraySize(files);
            for (int i = 0; i < array_size; i++)
            {
                cJSON *item = cJSON_GetArrayItem(files, i);
                FileInfo file_info_temp;

                if (cJSON_HasObjectItem(item, "file_name"))
                    file_info_temp.m_file_name = cJSON_GetObjectItem(item, "file_name")->valuestring;
                if (cJSON_HasObjectItem(item, "est_time"))
                    file_info_temp.m_est_time = cJSON_GetObjectItem(item, "est_time")->valuestring;
                if (cJSON_HasObjectItem(item, "create_time"))
                    file_info_temp.m_create_time = cJSON_GetObjectItem(item, "create_time")->valueint;
                if (cJSON_HasObjectItem(item, "file_size"))
                    file_info_temp.m_file_size = cJSON_GetObjectItem(item, "file_size")->valueint;
                if (cJSON_HasObjectItem(item, "total_layers"))
                    file_info_temp.m_total_layers = cJSON_GetObjectItem(item, "total_layers")->valueint;
                if (cJSON_HasObjectItem(item, "thumbnail_width"))
                    file_info_temp.m_thumbnail_width = cJSON_GetObjectItem(item, "thumbnail_width")->valueint;
                if (cJSON_HasObjectItem(item, "thumbnail_height"))
                    file_info_temp.m_thumbnail_height = cJSON_GetObjectItem(item, "thumbnail_height")->valueint;
                if (cJSON_HasObjectItem(item, "est_filament_length"))
                    file_info_temp.m_est_filament_length = cJSON_GetObjectItem(item, "est_filament_length")->valuedouble;

                file_info.push_back(file_info_temp);
            }
        }
        cJSON_Delete(root);
    }
    else 
    {
        // 旧的TXT格式处理
        LOG_D("Found old TXT format file, converting to JSON format\n");
        std::istringstream iss(content);
        std::string line;
        
        while (std::getline(iss, line)) 
        {
            try {
                std::vector<std::string> tokens;
                size_t start = 0, end = 0;
                
                // 使用更安全的分割方法
                while ((end = line.find(",", start)) != std::string::npos) 
                {
                    tokens.push_back(line.substr(start, end - start));
                    start = end + 1;
                }
                tokens.push_back(line.substr(start));

                // 确保有足够的字段
                if (tokens.size() >= 8) 
                {
                    FileInfo file_info_temp;
                    file_info_temp.m_file_name = tokens[0];
                    file_info_temp.m_est_time = tokens[1];
                    file_info_temp.m_create_time = std::stol(tokens[2]);
                    file_info_temp.m_file_size = std::stol(tokens[3]);
                    file_info_temp.m_total_layers = std::stoul(tokens[4]);
                    file_info_temp.m_thumbnail_width = std::stoi(tokens[5]);
                    file_info_temp.m_thumbnail_height = std::stoi(tokens[6]);
                    file_info_temp.m_est_filament_length = std::stod(tokens[7]);
                    
                    file_info.push_back(file_info_temp);
                }
            } catch (const std::exception& e) {
                LOG_E("Error parsing line: %s\n", line.c_str());
                continue;
            }
        }

        // 删除旧的TXT文件
        std::string old_file = FILEMANAGER_FILE(FILEMANAGER_FILEPATH,FILEMANAGER_FILENAME);
        if (std::remove(old_file.c_str()) == 0) {
            LOG_I("Successfully deleted old TXT format file\n");
        } else {
            LOG_E("Failed to delete old TXT format file\n");
        }

        // 保存为新的JSON格式
        SaveFileInfo();
        LOG_I("Successfully converted to JSON format\n");
    }
}


std::vector<FileInfo>& FileManager::GetFileInfo()
{
    return file_info;
}


/*
    @brief: 根据传入文件名，在内存数组维护的文件信息中查找。如找到则删除该项信息，且删除对应的缩略图
*/
bool RemoveByName(std::vector<FileInfo>& file_info, const std::string& name) 
{
    auto it = std::find_if(file_info.begin(), file_info.end(),
        [&name](const FileInfo& info) { return info.m_file_name == name; });
    
    if (it != file_info.end()) 
    {
        // 删除缩略图
        std::string img_path = FILEMANAGER_FILEPATH + name + ".png";
        if(access(img_path.c_str(), F_OK) == 0)
            std::remove(img_path.c_str());
            
        file_info.erase(it);
        return true;
    }
    return false;
}


/*
    @brief: 传入文件路径返回文件名称
*/
std::string GetFileName(const std::string& path) {
    // 查找最后一个斜杠的位置
    size_t lastSlash = path.find_last_of("/");
    if (lastSlash == std::string::npos) {
        return path; // 如果没有斜杠，返回整个路径
    }
    return path.substr(lastSlash + 1); // 返回斜杠后的部分
}

/*
    @brief: 获取传入文件的信息并将其插入到内存数组中，同时将其缩略图保存
    @param:
        @ file_info: vector数组
        @ path: 需要插入信息的文件路径
*/
void InsertFileByPath(std::vector<FileInfo>& file_info, const std::string path) 
{
    FileInfo file_info_temp;
    file_info_temp.m_file_name = GetFileName(path);

    struct stat file_attr;
    stat(path.c_str(), &file_attr);
    file_info_temp.m_create_time = file_attr.st_mtime;
    file_info_temp.m_file_size = file_attr.st_size;

    slice_param_t slice_param = {0};
    char preview_path[PATH_MAX_LEN + 1];
    gcode_preview(path.c_str(), preview_path, 1, &slice_param, (char *)GetFileName(path).c_str());
    file_info_temp.m_total_layers = slice_param.total_layers;
    file_info_temp.m_est_filament_length = slice_param.est_filament_length;
    file_info_temp.m_est_time = slice_param.estimeated_time_str;
    file_info_temp.m_thumbnail_width = slice_param.thumbnail_width;
    file_info_temp.m_thumbnail_height = slice_param.thumbnail_heigh;

    file_info.push_back(file_info_temp);

    if(access(preview_path,F_OK) == 0)
    {
        utils_vfork_system("cp '%s' %s", preview_path, FILEMANAGER_FILEPATH);
        utils_vfork_system("sync");
    }
}


int FileManager::AddFile(std::string path)
{
    if(access(path.c_str(),F_OK) != 0)
    {
        LOG_E("Error access file %s\n",path.c_str());
        return -1;
    }

    //如果本地有维护的同名文件信息则删除
    if (RemoveByName(FileManager::GetInstance()->GetFileInfo(), GetFileName(path)))
        LOG_I("remove same file(%s) info from file_manager vector",path.c_str());

    //添加该文件信息
    InsertFileByPath(FileManager::GetInstance()->GetFileInfo(),path);

    //同步到文件
    SaveFileInfo();

    //输出当前文件信息
    // PrintFileInfo();
    return 0;
}

int FileManager::DeleteFile(std::string path)
{
    int ret = 0;

    if (RemoveByName(FileManager::GetInstance()->GetFileInfo(), GetFileName(path))) 
    {
        LOG_D("file(%s) info has been removed from file_manager'vector\n",GetFileName(path).c_str());
    }
    else 
    {
        LOG_D("file(%s) info not found in file_manager'vector\n",GetFileName(path).c_str());
        ret = -1;
    }

    //同步到文件
    SaveFileInfo();

    //输出当前文件信息
    // PrintFileInfo();

    return ret;
}

void FileManager::SaveFileInfo()
{
    // 如果没有文件信息,则删除文件
    if(file_info.empty())
    {
        LOG_I("file_manager have no file, delete file %s\n", FILEMANAGER_FILENAME);
        std::remove(FILEMANAGER_FILE(FILEMANAGER_FILEPATH,FILEMANAGER_FILENAME));
        return;
    }

    // 创建JSON对象
    cJSON *root = cJSON_CreateObject();
    cJSON *files = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "files", files);

    // 将每个文件信息添加到JSON数组中
    for (const auto& info : file_info) 
    {
        cJSON *file_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(file_obj, "file_name", info.m_file_name.c_str());
        cJSON_AddStringToObject(file_obj, "est_time", info.m_est_time.c_str());
        cJSON_AddNumberToObject(file_obj, "create_time", info.m_create_time);
        cJSON_AddNumberToObject(file_obj, "file_size", info.m_file_size);
        cJSON_AddNumberToObject(file_obj, "total_layers", info.m_total_layers);
        cJSON_AddNumberToObject(file_obj, "thumbnail_width", info.m_thumbnail_width);
        cJSON_AddNumberToObject(file_obj, "thumbnail_height", info.m_thumbnail_height);
        cJSON_AddNumberToObject(file_obj, "est_filament_length", info.m_est_filament_length);
        
        cJSON_AddItemToArray(files, file_obj);
    }

    // 将JSON转换为字符串并保存到文件
    char *json_str = cJSON_Print(root);
    std::ofstream fp(FILEMANAGER_FILE(FILEMANAGER_FILEPATH,FILEMANAGER_FILENAME));
    if (fp.is_open()) 
    {
        fp << json_str;
        fp.close();
        LOG_D("file_manager sync to file %s\n", FILEMANAGER_FILE(FILEMANAGER_FILEPATH,FILEMANAGER_FILENAME));
    } 
    else 
    {
        LOG_E("can't open file %s to sync file_manager\n", FILEMANAGER_FILE(FILEMANAGER_FILEPATH,FILEMANAGER_FILENAME));
    }

    // 清理内存
    free(json_str);
    cJSON_Delete(root);
}


void FileManager::PrintFileInfo()
{
    std::vector<FileInfo>& file_info = FileManager::GetInstance()->GetFileInfo();
    for (const auto& info : file_info) 
    {
        std::cout << info.m_file_name << "," << info.m_est_time << "," << info.m_create_time << "," <<\
         info.m_file_size << "," << info.m_total_layers << "," << info.m_thumbnail_width << "," <<\
         info.m_thumbnail_height << "," << info.m_est_filament_length << std::endl;
    }
}

void FileManager::ClearFileInfo()
{
    LOG_I("ClearFileInfo：delete dir %s\n",FILEMANAGER_FILEPATH);
    hl_system("rm %s -rf", FILEMANAGER_FILEPATH);
    system("sync");
}