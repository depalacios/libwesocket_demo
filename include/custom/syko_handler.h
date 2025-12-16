#include <cjson.h>
#include <string.h>
#include <libwebsockets.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>     // Para close()
#include <sys/socket.h> // Para socket(), bind(), sendto(), recvfrom()
#include <sys/ioctl.h>
#include <net/if.h>     // Para struct ifreq
#include <linux/can.h>  // Para struct can_frame
#include <linux/can/raw.h> // Para CAN_RAW

enum commands{
    unknown_command = 0,
    get_basic_config,
    get_full_config,
    get_available_features,
    remotegui_device_info,
    remotegui_vehicle_info,
    remotegui_read_dtc,
    remotegui_clear_dtc,
    remotegui_program_vehicle,
    remotegui_datalog,
    remotegui_user_input,
};

int initCanBus();
void receiveCanMjs();
cJSON * unknown_command_fnc();
cJSON * remotegui_device_info_fnc();
cJSON * remotegui_program_vehicle_fnc();
enum commands sykoCommandsHandler(cJSON *root);
enum commands sykoCommandsTranslate(char * command);
void sendCanMjs(const char *mjs, size_t len);
