// Including the necessary libraries
// Standard input/output header file: Reading and writing data from standard input and standard output
#include <stdio.h>
// Functions and macros for creating tasks, retrieving information about tasks, managing threads, and handling synchronization primitives
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
// Registering and unregistering event handlers, dispatching events, and managing event queues
#include "esp_event.h"
// Reading, writing, and erasing data to the non-volatile storage (NVS) partition on the ESP32
#include "nvs_flash.h"
// Logging messages to the ESP console and external loggers
#include "esp_log.h"
// Initializing and managing the ESP Nimble Bluetooth stack
#include "esp_nimble_hci.h"
// Platform-specific functions for interacting with the Nimble Bluetooth stack and integrating the Nimble Bluetooth stack with the FreeRTOS kernel
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
// Host (BLE Host) header file: Managing the BLE host stack
#include "host/ble_hs.h"
// GAP (Generic Access Profile) service header file: Advertising the device, managing connections, etc.
#include "services/gap/ble_svc_gap.h"
// GATT (Generic Attribute Profile) service header file: Defining and managing GATT services, characteristics, and descriptors
#include "services/gatt/ble_svc_gatt.h"   
// SDK configuration header file: Information about the configuration of the ESP32             
#include "sdkconfig.h"  
// Controlling GPIO pins                 
#include "driver/gpio.h"                   

// Defining the variables and functions required
char *TAG = "Melika-Server";
uint8_t ble_addr_type;
struct ble_gap_adv_params adv_params;
bool status = false;
void ble_app_advertise(void);

// Writing data to ESP32 defined as server
static int device_write(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{   
    // Retrieving the data written to the characteristic 
    char *data = (char *)ctxt->om->om_data;

    // Deciding about the data
    if (strcmp(data, (char *)"LIGHT ON\0") == 0)
    {
        // Turning on the LED
        gpio_set_level(GPIO_NUM_2, 1);
    }
    else if (strcmp(data, (char *)"LIGHT OFF\0") == 0)
    {   
        // Turning off the LED
        gpio_set_level(GPIO_NUM_2, 0);
    }

    // Clearing the data buffer to ensure that it's ready for the next write operation
    memset(data, 0, strlen(data));

    return 0;
}

// Array of GATT service: structures that represent BLE services. 
// Each service definition includes a UUID (Universal Unique Identifier), a list of characteristics, and access callback functions.
static const struct ble_gatt_svc_def gatt_svcs[] = {
     // A primary service type, which means it can have child characteristics
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     // Defining UUID for device type, which is used to identify it during discovery and communication
     .uuid = BLE_UUID16_DECLARE(0x180), 
     // A structure to define the characteristics which is a unit of data that can be read, written, or subscribed to
     .characteristics = (struct ble_gatt_chr_def[]){
          // Defining UUID for writing
         {.uuid = BLE_UUID16_DECLARE(0xDEAD), 
          // The flags field determines the read/write/notify capabilities of the characteristic
          // BLE_GATT_CHR_F_WRITE flag indicates that the characteristic is writable
          .flags = BLE_GATT_CHR_F_WRITE,
          // The access callback function is responsible for handling read and write operations on the characteristic
          .access_cb = device_write},
         // Closing the structure for the first characteristic and indicates that there are no additional characteristics for the primary service
         {0}
    }
    },
    // Closing the array of service definitions and indicates that there are no more services to define
    {0}   
};

// Handling BLE events 
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    // Deciding about the event
    switch (event->type)
    {
    // Advertise if connected
    // If the connection failed, Re-initiate the advertising process to make the device discoverable again
    case BLE_GAP_EVENT_CONNECT:
        // Displaying the status of the connection
        ESP_LOGI("GAP", "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        if (event->connect.status != 0)
        {
            ble_app_advertise();
        }
        break;

    // If the ESP32 loses the connection with a BLE client, Re-start advertising
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT DISCONNECTED");
        if (event->connect.status != 0)
        {
            ble_app_advertise();
        }
        break;
    // If the advertising process finishes successfully, Re-initiates the advertising process to maintain the device's visibility
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI("GAP", "BLE GAP EVENT");
        ble_app_advertise();
        break;

    default:
        break;
    }

    return 0;
}

// Defining the BLE connection
void ble_app_advertise(void)
{

    // Defining the necessary variables
    struct ble_hs_adv_fields fields;
    const char *device_name;

    // Initializing all the fields of the "fields" struct to zeros
    memset(&fields, 0, sizeof(fields));

    // Retrieving the current BLE device name from flash storage
    device_name = ble_svc_gap_device_name(); 

    // Assigning the "fields"' fields
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;

    // Setting the advertising fields defined in the "fields" struct
    ble_gap_adv_set_fields(&fields);

    // Initializing all the fields of the "adv_params" struct to zeros
    memset(&adv_params, 0, sizeof(adv_params));

}

void ble_app_on_sync(void)
{
    // Automatically determining the best advertising address type for the ESP32
    // Analyzing the current network conditions and selects the appropriate address type, which affects the range and visibility of the BLE advertisement
    ble_hs_id_infer_auto(0, &ble_addr_type); 
    ble_app_advertise();                    
}

// Defining the infinite task
// This function is intended to run as a separate background task that continuously monitors and manages the BLE connection
void host_task(void *param)
{

    // Handling incoming BLE events and data: Processing BLE messages, responding to client requests, and maintaining the communication channel
    // The function will continue to run until it is explicitly stopped by calling the nimble_port_stop() function
    nimble_port_run(); 

}

void connect_ble(void)
{

    // Initializing the NVS flash memory
    // This memory is used to store persistent data, such as the device name and key information
    nvs_flash_init(); 
    // Initializing the NimBLE host stack
    // It is responsible for managing BLE communication and interactions with the underlying hardware
    nimble_port_init();                        
    // Initializing NimBLE configuration - server name
    ble_svc_gap_device_name_set("Melika-Server");
    // Initializing the BLE GAP and GATT services
    // These services provide the basic functionality for BLE communication, such as advertising, connecting, and exchanging data
    ble_svc_gap_init();                        
    ble_svc_gatt_init();            
    ble_gatts_count_cfg(gatt_svcs); 
    // Queuing the BLE services defined in the gatt_svcs array         
    ble_gatts_add_svcs(gatt_svcs);             
    // This callback function is invoked when the BLE system becomes synchronized, indicating that it is ready to start advertising and connecting
    ble_hs_cfg.sync_cb = ble_app_on_sync;      
    // Starting the BLE thread
    nimble_port_freertos_init(host_task);      

}

void boot_creds_clear(void *param)
{
    // Storing the current time obtained from the ESP32's timer system
    int64_t m = esp_timer_get_time();
    while (true)
    {
        // If the boot button is not pressed ...
        if (!gpio_get_level(GPIO_NUM_0))
        {
            int64_t n = esp_timer_get_time();

            // If the button has been held for at least 3 seconds ...
            if ((n - m) / 1000 >= 2000)
            {
                // Displaying the detection of pressing the boot button
                ESP_LOGI("BOOT BUTTON:", "Button Pressed FOR 3 SECOND\n");
                // BLE device will be connectable and discoverable by other BLE devices
                adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; 
                adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; 
                // Restarting the BLE advertising process using the specified advertising parameters 
                ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
                // Indicating that the boot button has been pressed for 3 seconds and the boot credentials should be reset
                status = true;
                // Pausing for 100 ms before checking the button status again
                // This delay ensures that the function does not consume excessive CPU resources
                vTaskDelay(100);
                // Resetting the timer
                m = esp_timer_get_time();
            }
        }
        else
        {
            m = esp_timer_get_time();
        }

        vTaskDelay(10);

        if (status)
        {
            adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; 
            adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; 
            ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
        }
    }
}

// The main function
void app_main()
{
    // Setting the boot button as input
    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT); 
    // Setting the LED pin as output
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT);

    connect_ble();

    // The task is given a priority of 5, which indicates that it should have a higher execution priority than most other tasks
    xTaskCreate(boot_creds_clear, "boot_creds_clear", 2048, NULL, 5, NULL);
}
