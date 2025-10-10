#ifndef PALETTE2_H
#define PALETTE2_H
#include <string>
#include <vector>
#include "reactor.h"
#include <queue>
#include "serialhdl.h"
#include <utility>
#include "gcode.h"
#include <map>

typedef struct palette2_status_tag{
    bool is_splicing;
    int remaining_load_length;
    std::pair<int, double> ping;
}palette2_status_t;

class Palette2{
    private:

    public:
        Palette2(std::string section_name);
        ~Palette2();

        int m_baud;
        Serialhdl *m_serial = nullptr;

        std::vector<std::string> m_COMMAND_CLEAR;
        std::string m_COMMAND_HEARTBEAT;
        std::string m_COMMAND_CUT;
        std::string m_COMMAND_FILENAME;
        std::string m_COMMAND_FILENAMES_DONE;
        std::string m_COMMAND_FIRMWARE;
        std::string m_COMMAND_PING;
        std::string m_COMMAND_SMART_LOAD_STOP;
        std::string m_INFO_NOT_CONNECTED;

        std::string m_cmd_Connect_Help;
        std::string m_cmd_Disconnect_Help;
        std::string m_cmd_Clear_Help;
        std::string m_cmd_Cut_Help;
        std::string m_cmd_Smart_Load_Help;

        std::string m_cmd_O1_help;
        std::string m_cmd_O9_help;

        double m_feedrate_splice;
        double m_feedrate_normal;
        int m_auto_load_speed;
        double m_auto_cancel_variation;

        ReactorTimerPtr m_read_timer = nullptr;
        std::string m_read_buffer = "";
        double m_heartbeat;
        std::vector<std::string> m_read_queue;
        ReactorTimerPtr m_write_timer = nullptr;
        std::vector<std::string> m_write_queue;
        ReactorTimerPtr m_heartbeat_timer = nullptr;
        // m_heartbeat = None
        bool m_signal_disconnect = false;
        bool m_is_printing = false;
        ReactorTimerPtr m_smart_load_timer = nullptr;

        std::vector<std::string> m_omega_drives;
        std::vector<std::string> m_files;
        bool m_is_setup_complete;
        bool m_is_splicing;
        bool m_is_loading;
        int m_remaining_load_length;
        std::vector<std::string> m_omega_algorithms;
        int m_omega_algorithms_counter;
        std::vector<std::pair<int, std::string>> m_omega_splices;
        int m_omega_splices_counter;
        std::vector<std::pair<int, double>> m_omega_pings;
        std::vector<std::pair<int, double>> m_omega_pongs;
        std::string m_omega_current_ping;
        std::vector<std::string> m_omega_header; //9
        int m_omega_header_counter;
        std::string m_omega_last_command;
        std::vector<std::string> m_omega_drivers;

        void _reset();
        bool _check_P2(GCodeCommand* gcmd = nullptr);
        void cmd_Connect(GCodeCommand& gcmd);
        void cmd_Disconnect(GCodeCommand& gmcd);
        void cmd_Clear(GCodeCommand& gcmd);
        void cmd_Cut(GCodeCommand& gcmd);
        void cmd_Smart_Load(GCodeCommand& gcmd);
        void cmd_OmegaDefault(GCodeCommand& gcmd);
        void _wait_for_heartbeat();
        void cmd_O1(GCodeCommand& gcmd);
        void cmd_O9(GCodeCommand& gcmd);
        void cmd_O21(GCodeCommand& gcmd);
        void cmd_O22(GCodeCommand& gcmd);
        void cmd_O23(GCodeCommand& gcmd);
        void cmd_O24(GCodeCommand& gcmd);
        void cmd_O25(GCodeCommand& gcmd);
        void cmd_O26(GCodeCommand& gcmd);
        void cmd_O27(GCodeCommand& gcmd);
        void cmd_O28(GCodeCommand& gcmd);
        void cmd_O29(GCodeCommand& gcmd);
        void cmd_O30(GCodeCommand& gcmd);
        void cmd_O31(GCodeCommand& gcmd);
        void cmd_O32(GCodeCommand& gcmd);

        void p2cmd_O20(std::vector<std::string> params);
        void check_ping_variation(double last_ping);
        void p2cmd_O34(std::vector<std::string> params);

        void p2cmd_O40(std::vector<std::string> params);
        void p2cmd_O50(std::vector<std::string> params);
        void p2cmd_O53(std::vector<std::string> params);
        void p2cmd_O88(std::vector<std::string> params);


        void printCancelling(std::vector<std::string> params);
        void printCancelled(std::vector<std::string> params);
        void loadingOffsetStart(std::vector<std::string> params);
        void loadingOffset(std::vector<std::string> params);
        void feedrateStart(std::vector<std::string> params);
        void feedrateEnd(std::vector<std::string> params);
        void p2cmd_O97(std::vector<std::string> params);
        void p2cmd_O100(std::vector<std::string> params);
        void p2cmd_O102(std::vector<std::string> params);
        void p2cmd(std::string line);

        bool _param_Matcher(std::vector<std::vector<std::string>> matchers, std::vector<std::string> params);
        double _run_Read(double eventtime);
        double _run_Heartbeat(double eventtime);
        double _run_Write(double eventtime);
        double _run_Smart_Load(double eventtime);
        palette2_status_t get_status(double eventtime);
};  
#endif