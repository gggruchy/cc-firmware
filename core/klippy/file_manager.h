#ifndef __FILE_MANAGER_H
#define __FILE_MANAGER_H

#include <iostream>
#include <vector>
#include <cstring>

#define FILEMANAGER_FILEPATH "/user-resource/file_info/"
#define FILEMANAGER_FILENAME "file_info.txt"

// 拼接宏字符串
#define FILEMANAGER_FILE(a, b) a b

typedef struct _FileInfo
{
    std::string m_file_name;
    std::string m_est_time;
    time_t m_create_time;
    long m_file_size;
    uint32_t m_total_layers;
    int m_thumbnail_width;
    int m_thumbnail_height;
    double m_est_filament_length;

     // 默认构造函数
    _FileInfo() : m_file_name("unknow"), m_est_time("unknow"), m_create_time(0), m_file_size(0),\
     m_total_layers(0), m_thumbnail_width(0), m_thumbnail_height(0), m_est_filament_length(0.0) {}  // 默认值
}FileInfo;

class FileManager
{
public:
    /**
     * @brief: Default constructor
     */
    FileManager();
    
    /**
     * @brief: Destructor
     */
    ~FileManager();

    // 禁用拷贝构造和赋值操作符，防止意外复制单例
    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;

    /**
     * @brief: 获取实例
     */
    static FileManager* GetInstance();

    static void DestroyInstance();  // 添加销毁实例的方法

    /*

    /*
    @brief: 获取维护的文件信息数组
    */
    std::vector<FileInfo> &GetFileInfo();

    /*
    @brief: 加载文件信息到内存中
    */
    void LoadFileInfo();

    /*
        @brief: 内存中添加指定的文件信息，如果维护有同名文件信息则先删除后添加，并同步到文件
        @param:
            @path: 被添加文件的路径
    */  
    int AddFile(std::string path);

    /*
        @brief: 内存中删除指定的文件信息并同步到文件
        @param:
            @path: 被删除文件的路径
    */
    int DeleteFile(std::string path);

    /*
    @brief: 将内存中维护的文件信息写到文件中
    */
    void SaveFileInfo();

    /*
        @brief : 打印输出维护的所有文件信息
    */
    void PrintFileInfo();

    /*
        @brief : 清空维护了文件信息的文件和缩略图
    */
    void ClearFileInfo();
private:
    static FileManager* instance;
    std::vector<FileInfo> file_info;   // 改为直接存储对象而不是指针
};

#endif