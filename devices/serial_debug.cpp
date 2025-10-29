 #include "serial_debug.h"
 #include "../klippy.h"

 static int debug_serial_fd;
 SafeQueue<std::string> serial_control_sq;

 static void serial_command_control(void *arg)
 {
     Printer::GetInstance()->serial_control_signal();
     std::string control_line((char *)arg);
     // std::cout << "control_line = " << control_line << std::endl;
     serial_control_sq.push(control_line);
 }


 int serial_log(std::string logs)
 {
     write(debug_serial_fd, logs.c_str(), logs.size());
     write(debug_serial_fd, "\r\n", 3);
 }

 int serial_info(std::string info)
 {
     info = "[Info] " + info;
     write(debug_serial_fd, info.c_str(), info.size());
     write(debug_serial_fd, "\r\n", 3);
 }

 int serial_error(std::string error_msg)
 {
     error_msg = "[Error] " + error_msg;
     write(debug_serial_fd, error_msg.c_str(), error_msg.size());
     write(debug_serial_fd, "\r\n", 3);
 }

 int serial_debug(std::string debug_msg)
 {
     debug_msg = "[Debug] " + debug_msg;
     write(debug_serial_fd, debug_msg.c_str(), debug_msg.size());
     write(debug_serial_fd, "\r\n", 3);
 }

 int open_dev()
 {
     int fd = open(DEBUG_SERIAL, O_RDWR);
     if (fd == -1)
     {
         printf("can't open (%s)\n", DEBUG_SERIAL);
     }
     return fd;
 }

 void set_speed(int fd, int speed)
 {
     int i;
     int status;
     struct termios Opt = {0};
     int speed_arr[] = {
         B115200,
         B38400,
         B19200,
         B9600,
         B4800,
         B2400,
         B1200,
         B300,
     };
     int name_arr[] = {
         115200,
         38400,
         19200,
         9600,
         4800,
         2400,
         1200,
         300,
     };
     tcgetattr(fd, &Opt);
     for (i = 0; i < sizeof(name_arr) / sizeof(int); i++)
     {
         if (speed == name_arr[i])
             break;
     }

     tcflush(fd, TCIOFLUSH);
     cfsetispeed(&Opt, name_arr[i]);
     cfsetospeed(&Opt, name_arr[i]);

     Opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); /*Input*/
     Opt.c_oflag &= ~OPOST;                          /*Output*/

     status = tcsetattr(fd, TCSANOW, &Opt);
     if (status != 0)
     {
         printf("tcsetattrfd\n");
         return;
     }
     tcflush(fd, TCIOFLUSH);
 }

 int set_parity(int fd, int databits, int stopbits, int parity)
 {
     struct termios options;

     if (tcgetattr(fd, &options) != 0)
     {
         return -1;
     }
     options.c_cflag &= ~CSIZE;
     switch (databits) /*设置数据位数*/
     {
     case 7:
         options.c_cflag |= CS7;
         break;
     case 8:
         options.c_cflag |= CS8;
         break;
     default:
         fprintf(stderr, "Unsupporteddatasize\n");
         return -1;
     }

     switch (parity)
     {
     case 'n':
     case 'N':
         options.c_cflag &= ~PARENB; /*Clearparityenable*/
         options.c_iflag &= ~INPCK;  /*Enableparitychecking*/
         break;
     case 'o':
     case 'O':
         options.c_cflag |= (PARODD | PARENB); /*设置为奇效验*/
         options.c_iflag |= INPCK;             /*Disnableparitychecking*/
         break;
     case 'e':
     case 'E':
         options.c_cflag |= PARENB;  /*Enableparity*/
         options.c_cflag &= ~PARODD; /*转换为偶效验*/
         options.c_iflag |= INPCK;   /*Disnableparitychecking*/
         break;
     case 'S':
     case 's': /*asnoparity*/
         options.c_cflag &= ~PARENB;
         options.c_cflag &= ~CSTOPB;
         break;
     default:
         fprintf(stderr, "Unsupportedparity\n");
         return -1;
     }

     /*设置停止位*/
     switch (stopbits)
     {
     case 1:
         options.c_cflag &= ~CSTOPB;
         break;
     case 2:
         options.c_cflag |= CSTOPB;
         break;
     default:
         fprintf(stderr, "Unsupportedstopbits\n");
         return -1;
     }
     /*Setinputparityoption*/
     if (parity != 'n')
         options.c_iflag |= INPCK;
     tcflush(fd, TCIFLUSH);
     options.c_cc[VTIME] = 150; /*设置超时15seconds*/
     options.c_cc[VMIN] = 0;    /*UpdatetheoptionsanddoitNOW*/
     if (tcsetattr(fd, TCSANOW, &options) != 0)
     {
         return -1;
     }
     return 0;
 }

 int print_serial(char *buf, int len)
 {
     for (int i = 0; i < len; i++)
     {
         if (i % 10 == 0)
             printf("\n");
         printf("!0x%02x", buf[i]);
     }
     printf("\n");
 }


 char command_buf[256];
 int command_buf_pos = 0;
 int serial_back_show(int fd, char *buf, int len)
 {
     int i;
     for (i = 0; i < len; i++)
     {
         command_buf[command_buf_pos] = buf[i];
         command_buf_pos++;
         if (buf[i] == '\n')
         {
             write(fd, "\r\nok!\r\n", 9);
             serial_command_control(command_buf);
             memset(command_buf, 0, strlen(command_buf));
             command_buf_pos = 0;
             return 0;
         }
     }
     write(fd, buf, i+1);
 }

 void debug_serial_init()
 {
     int fd = open_dev();
     set_speed(fd, 115200);
     if (set_parity(fd, 8, 1, 'N') == -1)
     {
         printf("SetParityError\n");
         exit(0);
     }
     debug_serial_fd = fd;
     return;
 }


 void* debug_read_serial(void *arg)
 {
     int fd = debug_serial_fd;
     int ret;
     fd_set rd_fdset;
     struct timeval dly_tm;
     char buf[256];
     printf("\r\nplease enter commend, enter \"M\" or \"G\" show command.\r\n");
     printf("==========================================================================\r\n");
     printf("==========================================================================\r\n");
     while (1)
     {
         FD_ZERO(&rd_fdset);
         FD_SET(fd, &rd_fdset);
         dly_tm.tv_sec = 5;
         dly_tm.tv_usec = 0; 
         memset(buf, 0, 256);
         ret = select(fd + 1, &rd_fdset, NULL, NULL, &dly_tm);
         if (ret == 0)
             continue;
         if (ret < 0)
         {
             printf("select(%s)return%d.[%d]:%s\n", DEBUG_SERIAL, ret, errno, strerror(errno));
             continue;
         }
         ret = read(fd, buf, 256);
          printf("Cnt%d:read(%s)return%d.\r\n", i, DEBUG_SERIAL, ret);
         serial_back_show(fd, buf, ret);
          print_serial(buf, ret);
        
     }
     close(fd);
     return NULL;
 }