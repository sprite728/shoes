#include <stdint.h>
#include <string.h>

#include "nordic_common.h"
#include "nrf.h"
#include "nrf_soc.h"
#include "app_error.h"
#include "softdevice_handler.h"
#include "app_timer.h"
#include "app_util.h"
#include "nrf_drv_spi.h"
#include "app_gpiote.h"
#include "nrf_delay.h"

#include "radio.h"

#include "led.h"
#include "adxl362.h"
#include "board.h"



#define LED0 8
#define LED1 26
#define LED2 29
#define LED3 9
#define LED4 10

#define DEVICE_NAME "Shoes!"

#define TIMESLOT_LENGTH_US   100000
// #define TIMESLOT_LENGTH_US   50000


#define TIMESLOT_DISTANCE_US 100000
#define TIMESLOT_TIMEOUT_US 200000


#define ACCELEROMETER_INTERRUPT_PIN 11
#define BUTTON_INTERRUPT_PIN 16

static nrf_drv_spi_t _spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);

app_gpiote_user_id_t gpiote_user_acc;


static nrf_radio_signal_callback_return_param_t m_signal_callback_return_param;
static nrf_radio_request_t m_timeslot_req_earliest = {
    NRF_RADIO_REQ_TYPE_EARLIEST,
    .params.earliest = {
        NRF_RADIO_HFCLK_CFG_DEFAULT,
        NRF_RADIO_PRIORITY_NORMAL,
        TIMESLOT_LENGTH_US,
        TIMESLOT_TIMEOUT_US
    }
};


void timeslot_sys_event_handler(uint32_t evt);



void ble_error(uint32_t error_code) {
    led_on(LED0);
}

/**@brief Callback function for asserts in the SoftDevice.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyze
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num   Line number of the failing ASSERT call.
 * @param[in] file_name  File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name) {
    app_error_handler(0x5599, line_num, p_file_name);
}



/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timers_init(void) {

}




/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
// static void sleep_mode_enter(void)
// {
//     uint32_t err_code = bsp_indication_set(BSP_INDICATE_IDLE);
//     APP_ERROR_CHECK(err_code);

//     // Prepare wakeup buttons.
//     err_code = bsp_btn_ble_sleep_mode_prepare();
//     APP_ERROR_CHECK(err_code);

//     // Go to system-off mode (this function will not return; wakeup will cause a reset).
//     err_code = sd_power_system_off();
//     APP_ERROR_CHECK(err_code);
// }





/**@brief Function for handling the Application's BLE Stack events.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void on_ble_evt(ble_evt_t * p_ble_evt) {

    switch (p_ble_evt->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            break;

        case BLE_GAP_EVT_TIMEOUT:
            break;

        case BLE_GATTC_EVT_TIMEOUT:
        case BLE_GATTS_EVT_TIMEOUT:
            break;

        default:
            // No implementation needed.
            break;
    }
}

static void ble_evt_dispatch(ble_evt_t * p_ble_evt) {
    on_ble_evt(p_ble_evt);
}

static void sys_evt_dispatch (uint32_t sys_evt) {
    timeslot_sys_event_handler(sys_evt);
}


static void ble_stack_init(void) {
    uint32_t err_code;

    // Initialize the SoftDevice handler module.
    SOFTDEVICE_HANDLER_INIT(NRF_CLOCK_LFCLKSRC_RC_250_PPM_TEMP_4000MS_CALIBRATION, NULL);

    ble_enable_params_t ble_enable_params;
    // Need these #defines. C is the worst.
    #define CENTRAL_LINK_COUNT    2
    #define PERIPHERAL_LINK_COUNT 1
    err_code = softdevice_enable_get_default_config(2, // central link count
                                                    1, // peripheral link count
                                                    &ble_enable_params);
    APP_ERROR_CHECK(err_code);

    //Check the ram settings against the used number of links
    CHECK_RAM_START_ADDR(CENTRAL_LINK_COUNT,PERIPHERAL_LINK_COUNT);

    // Enable BLE stack.
    err_code = softdevice_enable(&ble_enable_params);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_ble_evt_handler_set(ble_evt_dispatch);
    APP_ERROR_CHECK(err_code);

    // Register with the SoftDevice handler module for BLE events.
    err_code = softdevice_sys_evt_handler_set(sys_evt_dispatch);
    APP_ERROR_CHECK(err_code);
}


// Function for the Power manager.
static void power_manage (void) {
    uint32_t err_code = sd_app_evt_wait();
    APP_ERROR_CHECK(err_code);
}


void timeslot_sys_event_handler(uint32_t evt) {
    uint32_t err_code;

    switch (evt) {
      case NRF_EVT_RADIO_SESSION_IDLE:
      case NRF_EVT_RADIO_BLOCKED:
        // Request a new timeslot
        err_code = sd_radio_request(&m_timeslot_req_earliest);
        APP_ERROR_CHECK(err_code);
        break;

      case NRF_EVT_RADIO_SESSION_CLOSED:
        break;

      case NRF_EVT_RADIO_SIGNAL_CALLBACK_INVALID_RETURN:
        // ASSERT(false);
        break;

      case NRF_EVT_RADIO_CANCELED:
        err_code = sd_radio_request(&m_timeslot_req_earliest);
        APP_ERROR_CHECK(err_code);
        break;

      default:
        break;
    }
}







#define RX_BUF_SIZE 128
static uint8_t m_rx_buf[RX_BUF_SIZE];

typedef struct {
    uint8_t type_and_options;
    uint8_t length;
    uint8_t s1;
    uint8_t src_addr[6];
    uint8_t flags[3];
    uint8_t manuf[7];
    uint8_t name[8];
} __attribute__((packed)) advertisement_t;


advertisement_t advertisement = {
    .type_and_options = 0x02,  // ADV_NONCONN_IND
    // .type_and_options = 0x00,  // ADV_NONCONN_IND
    .length = 24,
    .s1 = 0,
    .src_addr = {0x08, 0x07, 0x0F, 0xe5, 0x98, 0xc0},
    .flags = {0x02, 0x01, 0x06},

    //           13                      17
    .manuf = {6, 0xff, 0xe0, 0x02, 0x14, 0x02, 0xb6},
    //          20
    .name = {7, 0x09, 0x53, 0x48, 0x4f, 0x45, 0x53, 0x21}
};


// static uint8_t m_tx_buf[] =
// {
//   0x01,                               // BLE Header (PDU_TYPE: SCAN_REQ, TXadd: 1 (random address), RXadd: 1 (random address)
//   0x00,                               // Length of payload: 12
//   0x00,                               // Padding bits for S1 (REF: the  nRF51 reference manual 16.1.2)
//   0xDE, 0xDE, 0xDE, 0xDE, 0xDE, 0xDE, // InitAddr LSByte first
//   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // AdvAddr LSByte first
// };



void continue_scan () {
    radio_disable();

    memset((void*) m_rx_buf, '\0', RX_BUF_SIZE);
    radio_buffer_configure(&m_rx_buf[0]);
    radio_rx_prepare(true);
    radio_rssi_enable();
}


void start_scan () {
    NVIC_EnableIRQ(TIMER0_IRQn);


    radio_init(39); // set channel to only use 39
    radio_rx_timeout_init();


    continue_scan();
}


void send_advertisement () {
    // memcpy(&m_tx_buf[9], &m_rx_buf[3], 6);
    radio_disable();
    radio_init(39);
    radio_buffer_configure((uint8_t*) &advertisement);
    radio_tx_prepare();
}



// The ID of the node that started the last flood.
uint64_t last_initiator = 0;
// The ID of the flood from that initiator. We need both because
// the same node could start two consecutive floods.
uint8_t  last_flood_id = 0xff;


void start_flood () {
    // Update flood id
    advertisement.manuf[5]++;

    send_advertisement();
}

void relay_flood_packet (int8_t rssi) {

}


void rx_callback (bool crc_valid) {
    if (crc_valid) {
        led_toggle(LED0);

        int8_t rssi = -1 * (int8_t) radio_rssi_get();

        // Check to see if this packet is us
        // Do this in a super hardcoded way. So nobody better change this
        // packet...
        if (m_rx_buf[13] == 0xff &&  // Manufacturer data and service id
            m_rx_buf[14] == 0xe0 &&
            m_rx_buf[15] == 0x02 &&
            m_rx_buf[16] == 0x14 &&
            m_rx_buf[20] == 0x09 &&  // Device name
            m_rx_buf[21] == 0x53 &&
            m_rx_buf[22] == 0x48 &&
            m_rx_buf[23] == 0x4f &&
            m_rx_buf[24] == 0x45 &&
            m_rx_buf[25] == 0x53 &&
            m_rx_buf[26] == 0x21) {
            // Yes! This is us!


            uint32_t id = (((uint64_t) m_rx_buf[3]) << 0) |
                          (((uint64_t) m_rx_buf[4]) << 8) |
                          (((uint64_t) m_rx_buf[5]) << 16) |
                          (((uint64_t) m_rx_buf[6]) << 24) |
                          (((uint64_t) m_rx_buf[7]) << 32) |
                          (((uint64_t) m_rx_buf[8]) << 40);

            uint8_t flood_id = m_rx_buf[17];
            // uint8_t extra = m_rx_buf[18];

            // Check to see if this is a new flood
            if (last_initiator != id || last_flood_id != flood_id) {
                // New flood!
                last_initiator = id;
                last_flood_id = flood_id;

                // led_on(25);

                // TODO: setup timer to turn off led after flood is done


                // Now spread the flood!
                relay_flood_packet(rssi);

                // Don't need to keep scanning at this point.
                return;
            }
        }

    }
    continue_scan();
}


void tx_callback () {
    start_scan();
}



nrf_radio_signal_callback_return_param_t* radio_cb (uint8_t sig) {
    switch (sig) {
        case NRF_RADIO_CALLBACK_SIGNAL_TYPE_START:
            // Setup a timer so we know when our timeslot is up
            NRF_TIMER0->TASKS_CLEAR = 1;
            NRF_TIMER0->EVENTS_COMPARE[0] = 0;
            NRF_TIMER0->INTENSET = TIMER_INTENSET_COMPARE0_Msk;
            NRF_TIMER0->CC[0] = TIMESLOT_LENGTH_US - 500;

            // Scan baby scan!
            start_scan();
            // send_advertisement();

            m_signal_callback_return_param.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_NONE;
            break;

        case NRF_RADIO_CALLBACK_SIGNAL_TYPE_RADIO:
            radio_event_cb();
            break;

        case NRF_RADIO_CALLBACK_SIGNAL_TYPE_TIMER0:
            // Our timeslot is about to expire. Ask for a new one.
            // I tried using the extension mechanism, I just got a bunch of
            // repeated callback weirdness.

            m_signal_callback_return_param.params.request.p_next = &m_timeslot_req_earliest;
            m_signal_callback_return_param.callback_action = NRF_RADIO_SIGNAL_CALLBACK_ACTION_REQUEST_AND_END;
            break;

        default:
            break;
    }
    return &m_signal_callback_return_param;
}


static void led_iterate () {
    static uint8_t state = 0;

    state += 1;
    if (state > 0x0F) {
        state = 0;
    }

    if (state & 0x1) led_off(LED1); else led_on(LED1);
    if (state & 0x2) led_off(LED2); else led_on(LED2);
    if (state & 0x4) led_off(LED4); else led_on(LED4);
    if (state & 0x8) led_off(LED3); else led_on(LED3);
}




static void interrupt_handler (uint32_t pins_l2h, uint32_t pins_h2l) {
    if (pins_h2l & (1 << ACCELEROMETER_INTERRUPT_PIN)) {
        // High to low transition
        // led_toggle(LED0);
        led_iterate();

        // me.seq++;
        // adv_init(&me);
        // advertising_start();
        // app_timer_start(app_timer, APP_TIMER_TICKS(4000, 0), NULL);
        // send_advertisement();

        start_flood();
    }

    if (pins_l2h & (1 << BUTTON_INTERRUPT_PIN)) {
        // Button press
        // led_toggle(LED0);
        led_iterate();
    }
}


void accelerometer_init () {
    uint32_t err;

    // Configure the accel hardware
    adxl362_accelerometer_init(&_spi, adxl362_NOISE_NORMAL, true, false, false);

    uint16_t act_thresh = 0x0222;
    adxl362_set_activity_threshold(act_thresh);
    uint16_t inact_thresh = 0x0096;
    adxl362_set_inactivity_threshold(inact_thresh);

    uint8_t a_time = 4;
    adxl362_set_activity_time(a_time);
    uint8_t ia_time = 30;
    adxl362_set_inactivity_time(ia_time);

    adxl362_interrupt_map_t intmap_2;

    intmap_2.DATA_READY = 0;
    intmap_2.FIFO_READY = 0;
    intmap_2.FIFO_WATERMARK = 0;
    intmap_2.FIFO_OVERRUN = 0;
    intmap_2.ACT = 0;
    intmap_2.INACT = 0;
    intmap_2.AWAKE = 1;
    intmap_2.INT_LOW = 1;
    adxl362_config_INTMAP(&intmap_2, true);

    adxl362_config_interrupt_mode(adxl362_INTERRUPT_LOOP, true , true);
    adxl362_activity_inactivity_interrupt_enable();

    adxl362_read_status_reg();

    // led_on(LED0);


    // Configure the accel interrupt

    // Need one user: accelerometer
    APP_GPIOTE_INIT(2);

    // Register the accelerometer
    err = app_gpiote_user_register(&gpiote_user_acc,
                             // (1<<BUTTON_INTERRUPT_PIN),   // Which pins we want the interrupt for low to high
                             (1<<ACCELEROMETER_INTERRUPT_PIN) | (1<<BUTTON_INTERRUPT_PIN),   // Which pins we want the interrupt for low to high
                             // (1<<ACCELEROMETER_INTERRUPT_PIN),   // Which pins we want the interrupt for low to high
                             // 1<<ACCELEROMETER_INTERRUPT_PIN,   // Which pins we want the interrupt for high to low
                             // 1<<BUTTON_INTERRUPT_PIN,   // Which pins we want the interrupt for high to low
                             (1<<ACCELEROMETER_INTERRUPT_PIN) | (1<<BUTTON_INTERRUPT_PIN),   // Which pins we want the interrupt for high to low
                             interrupt_handler);

    if (err != NRF_SUCCESS) {
        led_on(LED0);
    }

    // Enable the interrupt!
    err = app_gpiote_user_enable(gpiote_user_acc);
    if (err != NRF_SUCCESS) {
        led_on(LED0);
    }
}






int main () {
    uint32_t err_code;

    // Initialize.
    led_init(LED0);
    led_off(LED0);

    led_init(LED1);
    led_on(LED1);
    led_init(LED2);
    led_on(LED2);
    led_init(LED3);
    led_on(LED3);
    led_init(LED4);
    led_on(LED4);

    // led_off(LED1);
    // led_off(LED4);

    // led_off(LED2);
    // led_off(LED3);

    timers_init();

    ble_stack_init();

    sd_power_dcdc_mode_set(NRF_POWER_DCDC_ENABLE);

    accelerometer_init();

    // Create a session for doing timeslots
    err_code = sd_radio_session_open(radio_cb);
    APP_ERROR_CHECK(err_code);

    // Request a timeslot
    err_code = sd_radio_request(&m_timeslot_req_earliest);
    APP_ERROR_CHECK(err_code);

    // Enter main loop.
    while (1) {
        power_manage();
    }
}
