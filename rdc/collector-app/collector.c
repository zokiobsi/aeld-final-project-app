#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/i2c-dev.h>   
#include <mosquitto.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define I2C_DEV "/dev/i2c-1"
#define I2C_ADDR 0x44
#define MQTT_BROKER_IP "127.0.0.1"
#define MQTT_PORT 1883
#define MQTT_TOPIC "Sensor1/temp"
#define SAMPLE_RATE_HZ 1
#define MEASURE_SLEEP_MS 20


volatile sig_atomic_t running = 1;
void stop_signal(int sig){
    switch (sig){
        case SIGINT:
        case SIGTERM:
            running = 0;
            break;
    }
}


// CRC-8, poly 0x31, init 0xFF (per the SHT3x datasheet)
static uint8_t crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0xFF; // crc init value
    for (int i = 0; i < len; i++) { //for each data byte...
        crc ^= data[i]; //XOR crc with data
        for (int b = 0; b < 8; b++) { //then for each bit in the byte...
            if (crc & 0b10000000) { //check if the leftmost bit is 1
                crc = (uint8_t)((crc << 1) ^ 0x31); //if it is, shift left and apply poly 0x31
            } else {
                crc = (uint8_t)(crc << 1); //otherwise, shift left
            }
        }
    }
    return crc;
}


int read_temp(int fd, double *temp_c)
{
    const uint8_t read_cmd[2] = {0x24, 0x00};
    uint8_t ret_buf[6];
    uint16_t raw_temp;


    if (write(fd, read_cmd, 2) != 2)
        return -1;
    
    usleep(MEASURE_SLEEP_MS * 1000); //measurement time

    if (read(fd, ret_buf, 6) != 6)
        return -2;

    if (crc8(ret_buf, 2) != ret_buf[2]){
        errno = EBADMSG;
        return -3;
    }
    
    raw_temp = (ret_buf[0] << 8) | ret_buf[1];
    *temp_c = -45.0 + 175.0 * raw_temp / 65535;
    return 0;
}

int main(void){
    signal(SIGINT, stop_signal);
    signal(SIGTERM, stop_signal);

    //i2c setup
    int i2c_fd = open(I2C_DEV, O_RDWR);
    if (i2c_fd < 0){
        fprintf(stderr, "%s: %s\n", I2C_DEV, strerror(errno));
        return 1;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, I2C_ADDR) < 0){
        fprintf(stderr, "%s: %s\n", I2C_DEV, strerror(errno));
        close(i2c_fd);
        return 2;
    }

    //MQTT setup
    mosquitto_lib_init();
    struct mosquitto *m_client = mosquitto_new(NULL, true, NULL);
    if(!m_client){
        fprintf(stderr, "failed to create new mosquitto client\n");
        close(i2c_fd);
        return 3;
    }

    mosquitto_reconnect_delay_set(m_client, 1, 15, false);
    mosquitto_connect_async(m_client, MQTT_BROKER_IP, MQTT_PORT, 60);
    mosquitto_loop_start(m_client);

    while (running) {
        double temp_c;

        if (read_temp(i2c_fd, &temp_c) == 0){
            char payload[16];

            int length = snprintf(payload, sizeof(payload), "%.1f", temp_c);
            int rc = mosquitto_publish(m_client, NULL, MQTT_TOPIC, length, payload, 0, false);
            if (rc != MOSQ_ERR_SUCCESS)
                fprintf(stderr, "publish failed: %s\n", mosquitto_strerror(rc));
            else
                printf("logged %s to %s\n", payload, MQTT_TOPIC);
        } else {
            fprintf(stderr, "sensor read failed: %s\n", strerror(errno));
        }
        fflush(stdout);
        usleep(1000000 / SAMPLE_RATE_HZ - MEASURE_SLEEP_MS * 1000);
    }

    mosquitto_disconnect(m_client);
    mosquitto_loop_stop(m_client, false);
    mosquitto_destroy(m_client);
    mosquitto_lib_cleanup();
    close(i2c_fd);
    return 0;
}