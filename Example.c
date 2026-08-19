#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>



//readlink -f /sys/bus/i2c/devices/1-0068/driver // kiểm tra device nào đang chiếm
//echo 1-0068 | sudo tee /sys/bus/i2c/drivers/rtc-ds1307/unbind // unbind device đó

#define DEVICE_PATH "/dev/DS3231"
#define DS3231_IOCTL_MAGIC 'd'
#define DS3231_IOCTL_READ_SECOND     _IOR(DS3231_IOCTL_MAGIC, 1, int)
#define DS3231_IOCTL_READ_MINUTE     _IOR(DS3231_IOCTL_MAGIC, 2, int)
#define DS3231_IOCTL_READ_HOUR       _IOR(DS3231_IOCTL_MAGIC, 3, int)
#define DS3231_IOCTL_WRITE_CLOCK     _IOW(DS3231_IOCTL_MAGIC, 4, struct ds3231_clock)
#define DS3231_IOCTL_SET_ALARM       _IOW(DS3231_IOCTL_MAGIC, 5, struct ds3231_clock)
#define DS3231_IOCTL_READ_STATUS     _IOR(DS3231_IOCTL_MAGIC, 6, int)
#define DS3231_IOCTL_WRITE_STATUS    _IO(DS3231_IOCTL_MAGIC, 7)

struct ds3231_clock{
    unsigned char hour;
    unsigned char min;
    unsigned char sec;
};

int main()
{
    int fd;
    int data[3];
    int status;
    struct ds3231_clock time_data;
    int dg;

    //open the device
    fd = open(DEVICE_PATH, O_RDWR);

    if(fd < 0)
    {
        perror("Fail to open the device\n");
        return errno;
    }

    time_data.hour = 9;
    time_data.min = 59;
    time_data.sec = 30;
    if(ioctl(fd, DS3231_IOCTL_WRITE_CLOCK, &time_data) < 0)
    {
        perror("Fail to write data\n");
        close(fd);
        return errno;
    }

    time_data.hour = 10;
    time_data.min = 0;
    time_data.sec = 00;
    if(ioctl(fd, DS3231_IOCTL_SET_ALARM, &time_data) < 0)
    {
        perror("Fail to set alarm\n");
        close(fd);
        return errno;
    }
    else printf("Set Alarm Succecful\n");

    
    ioctl(fd, DS3231_IOCTL_WRITE_STATUS);

    ioctl(fd, DS3231_IOCTL_READ_STATUS, &dg);
    if(dg < 0)
    {
        printf("Fail to read status reg\n");
        close(fd);
    }
    int cnt =0;

    while(1)
    {
        if(ioctl(fd, DS3231_IOCTL_READ_SECOND, &data[0]) < 0)
        {
            perror("Fail to read second data\n");
            close(fd);
            return errno;
        }
        
        if(ioctl(fd, DS3231_IOCTL_READ_MINUTE, &data[1]) < 0)
        {
            perror("Fail to read second data\n");
            close(fd);
            return errno;
        }

        if(ioctl(fd, DS3231_IOCTL_READ_HOUR, &data[2]) < 0)
        {
            perror("Fail to read second data\n");
            close(fd);
            return errno;
        }

        if(ioctl(fd, DS3231_IOCTL_READ_STATUS, &status) < 0)
        {
            perror("Fail to read status\n");
            close(fd);
            return errno;
        }

        if(status & (1<<0) == (1<<0)) 
        {
            printf("Rengggg \n");
            ioctl(fd, DS3231_IOCTL_WRITE_STATUS);
        }

        // có \n để in mỗi giây, nếu ko có thì sẽ phải đợi đủ 50 lần mới in
        printf("%d:%d:%d\n", data[2],data[1],data[0]); 
        sleep(1);

        if(cnt ++ > 50) break;
    }

    close(fd);
    return 0;
}