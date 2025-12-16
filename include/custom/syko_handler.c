#include "syko_handler.h"

int s;
struct sockaddr_can addr;
struct ifreq ifr;

int initCanBus(){
    s = socket(PF_CAN, SOCK_RAW, CAN_RAW);

    if (s < 0){
        lwsl_user("Socket open error\n");
        return 1;
    }

    strcpy(ifr.ifr_name, "can0");
    ioctl(s, SIOCGIFINDEX, &ifr);

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    // 3. Bindear (enlazar) el socket a la interfaz
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        lwsl_user("Binding CAN error\n");
        close(s);
        return 1;
    }
    
    lwsl_user("Succesfuly started CAN\n");
    return 0;
}

void sendCanMjs(const char *mjs, size_t len) {
    size_t bytes_sent = 0, bytes_missing;   
    struct can_frame frame;
    int nbytes;
    
    frame.can_id = 0x123; 
    frame.can_dlc = 8;
    
    while (bytes_sent < len) {        
        bytes_missing = len - bytes_sent; 
        frame.can_dlc = (uint8_t)(bytes_missing > 8 ? 8 : bytes_missing);
        
        // Copy data to frame
        memcpy(frame.data, mjs + bytes_sent, frame.can_dlc);
        
        // Fill missing can bus with 0
        if (frame.can_dlc < 8) {
             memset(frame.data + frame.can_dlc, 0, 8 - frame.can_dlc);
        }

        // Send frame
        nbytes = write(s, &frame, sizeof(frame));

        if (nbytes != sizeof(frame)) {
            lwsl_user("CAN send frame error: only %d bytes were sent\n", nbytes);           
            return; 
        }
        
        // Update byte counter
        bytes_sent += frame.can_dlc;
    }
    
    lwsl_user("Message with %zu bytes succesfuly sent by CAN\n", len);
}

void receiveCanMjs(){
    struct can_frame rx_frame;
    int nbytes;

    // Get frame
    nbytes = read(s, &rx_frame, sizeof(rx_frame));

    if (nbytes < 0) {
        lwsl_user("Read Can Bus Error");
    } else if (nbytes == sizeof(rx_frame)) {
        lwsl_user("Recibido ID: 0x%X, DLC: %d, Data: ", rx_frame.can_id, rx_frame.can_dlc);
        
        for (int i = 0; i < rx_frame.can_dlc; i++) {
            lwsl_user("%02X ", rx_frame.data[i]);
        }
        
        lwsl_user("\n");
    }
}

enum commands sykoCommandsTranslate(char * command_request){
    // Get commands
     if (strcmp("get/basic-config", command_request) == 0){
        return get_basic_config;
     }
     else if (strcmp("get/full-config", command_request) == 0){
        return get_full_config;
     }
     else if (strcmp("get/available-features", command_request) == 0){
        return get_available_features;
     }
     // Remotegui commands
     else if (strcmp("remotegui/device-info", command_request) == 0){
        return remotegui_device_info;
     }
     else if (strcmp("remotegui/vehicle-info", command_request) == 0){
        return remotegui_vehicle_info;
     }
     else if (strcmp("remotegui/read-dtc", command_request) == 0){
        return remotegui_read_dtc;
     }
     else if (strcmp("remotegui/clear-dtc", command_request) == 0){
        return remotegui_clear_dtc;
     }
     else if (strcmp("remotegui/program-vehicle", command_request) == 0){
        return remotegui_program_vehicle;
     }
     else if (strcmp("remotegui/datalog", command_request) == 0){
        return remotegui_datalog;
     }
     else if (strcmp("remotegui/user-input", command_request) == 0){
        return remotegui_user_input;
     }
     else{
        return unknown_command;
     }
}

enum commands sykoCommandsHandler(cJSON *root){
    cJSON *comando = cJSON_GetObjectItemCaseSensitive(root, "sequence");
    cJSON *request = cJSON_GetObjectItemCaseSensitive(root, "request");

    lwsl_user("Sequence: %d, Request: %s\n", comando->valueint, request->valuestring);

    return sykoCommandsTranslate(request->valuestring);

    // switch ()
    // {
    // case get_basic_config:
    //     /* code */
    //     break;
    
    // default:
    //     break;
    // }

    // if (strcmp("remotegui/device-info", request->valuestring) == 0){
    //     lwsl_user("El buen comando\n");
    //     sendCanMjs("Hello world!", 12);
    // }
}

cJSON * unknown_command_fnc(){
    cJSON *root = NULL;

    root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "version", "1.2.3");
    cJSON_AddNumberToObject(root, "sequence", 100);
    cJSON_AddStringToObject(root, "response", "unknown-command");
    cJSON_AddStringToObject(root, "status", "not_found");

    return root;
}

cJSON * remotegui_device_info_fnc(){ 
    cJSON * root = NULL;
    cJSON *device_info_obj = NULL;
    cJSON *button_array = NULL;

    root = cJSON_CreateObject();
    device_info_obj = cJSON_CreateObject();
    button_array = cJSON_CreateArray();

    cJSON_AddItemToArray(button_array, cJSON_CreateString("EXIT"));
    cJSON_AddItemToArray(button_array, cJSON_CreateString("DONE"));

    cJSON_AddItemToObject(device_info_obj, "button", button_array);
    cJSON_AddStringToObject(device_info_obj, "title", "DEVICE INFO");
    cJSON_AddStringToObject(device_info_obj, "type", "message");
    cJSON_AddStringToObject(device_info_obj, "message", "This message contains formatted text of device info.");  
    
    cJSON_AddItemToObject(root, "remotegui/device-info", device_info_obj);
    cJSON_AddStringToObject(root, "version", "1.2.3");
    cJSON_AddNumberToObject(root, "sequence", 100);
    cJSON_AddStringToObject(root, "response", "remotegui/device-info");
    cJSON_AddStringToObject(root, "status", "ok");

    return root;
}

cJSON * remotegui_program_vehicle_fnc(){
    cJSON * root = NULL;

    root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", "1.2.3");
    cJSON_AddNumberToObject(root, "sequence", 120);
    cJSON_AddStringToObject(root, "response", "remotegui/program-vehicle");
    cJSON_AddStringToObject(root, "status", "ok");

    sendCanMjs("program_ecu", 11);

    return root;
}


