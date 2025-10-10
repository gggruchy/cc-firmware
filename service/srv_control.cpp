#include "srv_control.h"
#include "srv_state.h"
#include "simplebus.h"
#include "klippy.h"

typedef struct
{

} srv_control_t;

static srv_control_t srv_control;
static void srv_control_service_callback(const char *name, void *context, int request_id, void *args, void *response);

void srv_control_init(void)
{
    simple_bus_register_service("srv_control", &srv_control, srv_control_service_callback);
}

static void srv_control_service_callback(const char *name, void *context, int request_id, void *args, void *response)
{
    srv_control_t *s = (srv_control_t *)context;
    char linebuf[1024];

    if (!s)
        return;

    switch (request_id)
    {
    case SRV_CONTROL_STEPPER_MOVE:
    {
        int n = 0;
        srv_control_req_move_t *req = (srv_control_req_move_t *)args;
        srv_control_res_move_t *res = (srv_control_res_move_t *)response;
        srv_state_res_t state_res;
        uint8_t axis_flags = 0;

        if (fabs(req->x) > 1e-6)
            axis_flags |= 0x01;
        if (fabs(req->y) > 1e-6)
            axis_flags |= 0x02;
        if (fabs(req->z) > 1e-6)
            axis_flags |= 0x04;
        if (fabs(req->e) > 1e-6)
            axis_flags |= 0x08;

        simple_bus_request("srv_state", SRV_STATE_SRV_ID_STATE, NULL, &state_res);
        // 如果对应的轴未归零则报错
        // if ((state_res.state.home_state[0] != SRV_STATE_HOME_END_SUCCESS && axis_flags & 0x01) ||
        //     (state_res.state.home_state[1] != SRV_STATE_HOME_END_SUCCESS && axis_flags & 0x02) ||
        //     (state_res.state.home_state[2] != SRV_STATE_HOME_END_SUCCESS && axis_flags & 0x04))
        if ((Printer::GetInstance()->m_tool_head->m_kin->get_status(get_monotonic())["homed_axes"].find("x") == std::string::npos && axis_flags & 0x01) ||
            (Printer::GetInstance()->m_tool_head->m_kin->get_status(get_monotonic())["homed_axes"].find("y") == std::string::npos && axis_flags & 0x02) ||
            (Printer::GetInstance()->m_tool_head->m_kin->get_status(get_monotonic())["homed_axes"].find("z") == std::string::npos && axis_flags & 0x04))
        {
            res->ret = SRV_CONTROL_RET_ERROR;
            break;
        }

        // 检查喷头温度
        // if (axis_flags & 0x08)
        // {
        //     if (Printer::GetInstance()->m_printer_extruder->m_heater->m_can_extrude == false)
        //     {
        //         res->ret = SRV_CONTROL_RET_ERROR;
        //         break;
        //     }
        // }

        n += snprintf(linebuf + n, sizeof(linebuf) - n, "MANUAL_MOVE ");
        if (axis_flags & 0x01)
            n += snprintf(linebuf + n, sizeof(linebuf) - n, "X=%.2lf ", req->x);
        if (axis_flags & 0x02)
            n += snprintf(linebuf + n, sizeof(linebuf) - n, "Y=%.2lf ", req->y);
        if (axis_flags & 0x04)
            n += snprintf(linebuf + n, sizeof(linebuf) - n, "Z=%.2lf ", req->z);
        if (axis_flags & 0x08)
            n += snprintf(linebuf + n, sizeof(linebuf) - n, "E=%.2lf ", req->e);

        n += snprintf(linebuf + n, sizeof(linebuf) - n, "F=%.2lf", req->f);
        manual_control_sq.push("BED_MESH_APPLICATIONS ENABLE=0");
        manual_control_sq.push("G91");
        //m_min_extrude_temp设置为0，表示不限制喷头温度
        manual_control_sq.push("SET_MIN_EXTRUDE_TEMP S0");
        manual_control_sq.push(string(linebuf));
        //m_min_extrude_temp设置为默认值，表示限制喷头温度
        manual_control_sq.push("SET_MIN_EXTRUDE_TEMP RESET");
        manual_control_sq.push("BED_MESH_APPLICATIONS ENABLE=1");
        Printer::GetInstance()->manual_control_signal();
        res->ret = SRV_CONTROL_RET_OK;
    }
    break;

    case SRV_CONTROL_STEPPER_HOME:
    {
        int n = 0;
        srv_control_req_home_t *req = (srv_control_req_home_t *)args;
        srv_control_res_home_t *res = (srv_control_res_home_t *)response;

        srv_state_home_msg_t home_msg;

        n += snprintf(linebuf + n, sizeof(linebuf) - n, "G28 ");

        if (req->x)
        {
            n += snprintf(linebuf + n, sizeof(linebuf) - n, "X ", req->x);

            home_msg.axis = 0;
            home_msg.st = SRV_STATE_HOME_HOMING;
            simple_bus_publish_sync("home", SRV_HOME_MSG_ID_STATE, &home_msg, sizeof(home_msg));
        }

        if (req->y)
        {
            n += snprintf(linebuf + n, sizeof(linebuf) - n, "Y ", req->y);

            home_msg.axis = 1;
            home_msg.st = SRV_STATE_HOME_HOMING;
            simple_bus_publish_sync("home", SRV_HOME_MSG_ID_STATE, &home_msg, sizeof(home_msg));
        }

        if (req->z)
        {
            n += snprintf(linebuf + n, sizeof(linebuf) - n, "Z ", req->z);

            home_msg.axis = 2;
            home_msg.st = SRV_STATE_HOME_HOMING;
            simple_bus_publish_sync("home", SRV_HOME_MSG_ID_STATE, &home_msg, sizeof(home_msg));
        }

        manual_control_sq.push(string(linebuf));
        Printer::GetInstance()->manual_control_signal();
        res->ret = SRV_CONTROL_RET_OK;
    }
    break;

    case SRV_CONTROL_STEPPER_DISABLE:
    {
        manual_control_sq.push("M18");
        Printer::GetInstance()->manual_control_signal();
    }
    break;

    case SRV_CONTROL_HEATER_TEMPERATURE:
    {
        srv_control_req_heater_t *req = (srv_control_req_heater_t *)args;
        srv_state_heater_msg_t heater_msg;
        heater_msg.heater_id = req->heater_id;

        if (req->heater_id == HEATER_ID_EXTRUDER)
        {
            Printer::GetInstance()->m_pheaters->set_temperature(Printer::GetInstance()->m_printer_extruder->get_heater(), req->temperature, false);
            Printer::GetInstance()->m_printer_extruder->m_heater->m_lock.lock();
            heater_msg.target_temperature = Printer::GetInstance()->m_printer_extruder->m_heater->m_target_temp;
            heater_msg.current_temperature = Printer::GetInstance()->m_printer_extruder->m_heater->m_smoothed_temp;
            Printer::GetInstance()->m_printer_extruder->m_heater->m_lock.unlock();

            simple_bus_publish_sync("heater", SRV_HEATER_MSG_ID_STATE, &heater_msg, sizeof(heater_msg));
        }
        else if (req->heater_id == HEATER_ID_BED)
        {
            Printer::GetInstance()->m_pheaters->set_temperature(Printer::GetInstance()->m_bed_heater->m_heater, req->temperature, false);
            Printer::GetInstance()->m_bed_heater->m_heater->m_lock.lock();
            heater_msg.target_temperature = Printer::GetInstance()->m_bed_heater->m_heater->m_target_temp;
            heater_msg.current_temperature = Printer::GetInstance()->m_bed_heater->m_heater->m_smoothed_temp;
            Printer::GetInstance()->m_bed_heater->m_heater->m_lock.unlock();
            simple_bus_publish_sync("heater", SRV_HEATER_MSG_ID_STATE, &heater_msg, sizeof(heater_msg));
        }
    }
    break;

    case SRV_CONTROL_FAN_SPEED:
    {
        srv_control_req_fan_t *req = (srv_control_req_fan_t *)args;
        int n;
        if (req->fan_id > FAN_NUMBERS)
            return;

        std::vector<PrinterFan *> fans = Printer::GetInstance()->m_printer_fans;
        char fan_cmd[100];
        sprintf(fan_cmd, "SET_FAN_SPEED I=%d S=%f", req->fan_id, req->value);
        ui_cb[set_fan_speed_cb](fan_cmd);
        srv_state_fan_msg_t fan_msg;
        fan_msg.fan_id = req->fan_id;
        fan_msg.value = req->value;
        simple_bus_publish_sync("fan", SRV_FAN_MSG_ID_STATE, &fan_msg, sizeof(fan_msg));
        // for (auto it = fans.begin(); it != fans.end(); ++it)
        // {
        //     Fan *fan = (*it)->m_fan;
        //     if (fan->m_id == req->fan_id)
        //     {
        //         // fan->set_speed_from_command(req->value);
        //         fan_cmd_t cmd;
        //         cmd.fan_speed = req->value;
        //         cmd.fan = fan;
        //         ui_cb[set_fan_speed_cb](&cmd);

        //     }
        // }
    }
    break;

    default:
        break;
    }
}
