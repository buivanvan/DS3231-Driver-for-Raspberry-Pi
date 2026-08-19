#include <linux/init.h> // thư viện chứa macro init, exit
#include <linux/module.h> // thư viện chứa API cơ bản của kernel
#include <linux/i2c.h> // thư viện API i2c
#include <linux/device.h> // API quản lí ,tạo device, class
#include <linux/fs.h> // API liên quan đến file system, character device
#include <linux/uaccess.h> // thư viện giao tiếp giữa kernel và user

#define DRIVER_NAME     "DS3231_Driver"
#define CLASS_NAME      "DS3231"
#define DEVICE_NAME     "DS3231"

#define DS3231_REG_Second   0x00
#define DS3231_REG_Minute   0x01
#define DS3231_REG_Hour     0x02
#define DS3231_REG_Day      0x03
#define DS3231_REG_A1M1     0x07
#define DS3231_REG_A1M2     0x08
#define DS3231_REG_A1M3     0x09
#define DS3231_REG_A1M4     0x0A
#define DS3231_REG_CONTROL  0x0E
#define DS3231_REG_STATUS   0x0F

struct ds3231_clock {
    u8 hour;
    u8 min;
    u8 sec;
};

// IOCTL Commands
#define DS3231_IOCTL_MAGIC 'd'
#define DS3231_IOCTL_READ_SECOND     _IOR(DS3231_IOCTL_MAGIC, 1, int)
#define DS3231_IOCTL_READ_MINUTE     _IOR(DS3231_IOCTL_MAGIC, 2, int)
#define DS3231_IOCTL_READ_HOUR       _IOR(DS3231_IOCTL_MAGIC, 3, int)
#define DS3231_IOCTL_WRITE_CLOCK     _IOW(DS3231_IOCTL_MAGIC, 4, struct ds3231_clock)
#define DS3231_IOCTL_SET_ALARM       _IOW(DS3231_IOCTL_MAGIC, 5, struct ds3231_clock)
#define DS3231_IOCTL_READ_STATUS     _IOR(DS3231_IOCTL_MAGIC, 6, int)
#define DS3231_IOCTL_WRITE_STATUS    _IO(DS3231_IOCTL_MAGIC, 7)


static struct i2c_client *ds3231_client;
static struct class* ds3231_class = NULL;
static struct device* ds3231_device = NULL;
static int major_number;

static int ds3231_read_clock(struct i2c_client *client, int index)
{
    u8 clk[3];
    u8 tmp[3];

    if(i2c_smbus_read_i2c_block_data(client, DS3231_REG_Second, sizeof(tmp), tmp) < 0)
    {
        printk(KERN_ERR "Fail to read clock data\n");
        return -EIO;
    }

    clk[index] = tmp[index] & 0x0F;
    clk[index] = clk[index] + ((tmp[index] >> 4)& 0x07)*10;

    return clk[index];
}

static u8 dec_to_bcd(u8 val)
{
    return ((val / 10) << 4) | (val % 10);
}

static int ds3231_write_clock(struct i2c_client *client, u8 hour, u8 min, u8 sec)
{
    //cvt to bcd
    sec = dec_to_bcd(sec);
    min = dec_to_bcd(min);
    hour = dec_to_bcd(hour);

    if(i2c_smbus_write_byte_data(client, DS3231_REG_Second, sec) < 0)
    {
        printk(KERN_ERR "Fail to write clock data");
        return -EIO;
    }

    if(i2c_smbus_write_byte_data(client, DS3231_REG_Minute, min) < 0)
    {
        printk(KERN_ERR "Fail to write clock data");
        return -EIO;
    }

    if(i2c_smbus_write_byte_data(client, DS3231_REG_Hour, hour) < 0)
    {
        printk(KERN_ERR "Fail to write clock data");
        return -EIO;
    }

    return 0;
}

static int clear_flag_IT(struct i2c_client* client)
{
    static int st_reg;
    st_reg = i2c_smbus_read_byte_data(client, DS3231_REG_STATUS);
    if(st_reg < 0)
    {
        printk(KERN_ERR "Fail to read the status register\n");
        return -EIO;
    }

    if((st_reg & (1<<0) ) == (1<<0))
    {
        st_reg &= ~(1<<0);
        if(i2c_smbus_write_byte_data(client, DS3231_REG_STATUS, st_reg) < 0)
        {
            printk(KERN_ERR "Fail to write to status register\n");
            return -EIO;
        }
        st_reg = i2c_smbus_read_byte_data(client, DS3231_REG_STATUS);
        if(st_reg < 0)
        {
            printk(KERN_ERR "Fail to read the status register\n");
            return -EIO;
        }
    } 

    return 1;
}

static int ds3231_set_alarm(struct i2c_client *client, u8 hour, u8 min, u8 sec)
{
    //cvt to bcd
    sec = dec_to_bcd(sec);
    min = dec_to_bcd(min);
    hour = dec_to_bcd(hour);
    static u8 date = 0x80;
    static u8 ctrl = 0x01;

    if(i2c_smbus_write_byte_data(client, DS3231_REG_A1M1, sec) < 0)
    {
        printk(KERN_ERR "Fail to write clock data\n");
        return -EIO;
    }

    if(i2c_smbus_write_byte_data(client, DS3231_REG_A1M2, min) < 0)
    {
        printk(KERN_ERR "Fail to write clock data\n");
        return -EIO;
    }

    if(i2c_smbus_write_byte_data(client, DS3231_REG_A1M3, hour) < 0)
    {
        printk(KERN_ERR "Fail to write clock data\n");
        return -EIO;
    }
    // set Alarm when hours, minutes, and seconds match
    if(i2c_smbus_write_byte_data(client, DS3231_REG_A1M4, date) < 0)
    {
        printk(KERN_ERR "Fail to write clock data\n");
        return -EIO;
    }

    // En  Alarm 1 Interrupt
    if(i2c_smbus_write_byte_data(client, DS3231_REG_CONTROL, ctrl) < 0)
    {
        printk(KERN_ERR "Fail to enable interrupt alarm\n");
        return -EIO;
    }

    clear_flag_IT(client);

    return 0;
}

static long ds3231_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int data;
    struct ds3231_clock clock;

    switch(cmd)
    {
        case DS3231_IOCTL_READ_SECOND:
            data = ds3231_read_clock(ds3231_client, 0);
            break;
        case DS3231_IOCTL_READ_MINUTE:
            data = ds3231_read_clock(ds3231_client, 1);
            break;
        case DS3231_IOCTL_READ_HOUR:
            data = ds3231_read_clock(ds3231_client, 2);
            break;
        case DS3231_IOCTL_WRITE_CLOCK:
            if(copy_from_user(&clock, (int __user *)arg, sizeof(clock)))
            {
                return -EFAULT;
            }

            if(clock.hour > 23 || clock.min > 59 || clock.sec > 59) return -EINVAL;

            return ds3231_write_clock(ds3231_client, clock.hour, clock.min, clock.sec);
        case DS3231_IOCTL_SET_ALARM:
            if(copy_from_user(&clock, (int __user *)arg, sizeof(clock)))
            {
                return -EFAULT;
            }
            if(clock.hour > 23 || clock.min > 59 || clock.sec > 59) return -EINVAL;

            return ds3231_set_alarm(ds3231_client, clock.hour, clock.min, clock.sec);
        case DS3231_IOCTL_READ_STATUS:
            data = i2c_smbus_read_byte_data(ds3231_client, DS3231_REG_STATUS);
            if(data < 0)
            {
                printk(KERN_ERR "Fail to read status\n");
                return -EIO;
            }
            break;
        case DS3231_IOCTL_WRITE_STATUS:
            if( clear_flag_IT(ds3231_client) < 0)
            {
                printk(KERN_ERR "Fail to write status\n");
                return -EIO;
            }
            break;
            
        default:
            return -EINVAL;
    }

    if(copy_to_user( (int __user*)arg, &data, sizeof(data)))
    {
        return -EFAULT;
    }

    return 0;
}

static int ds3231_open(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "DS3231 device opened\n");
    return 0;
}

static int ds3231_release(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "DS3231 device closed\n");
    return 0;
}

static struct file_operations fops = 
{
    .open = ds3231_open,
    .unlocked_ioctl = ds3231_ioctl,
    .release = ds3231_release,
};

static int ds3231_probe(struct i2c_client *client)
{
    ds3231_client = client;

    //create a char device
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if(major_number < 0)
    {
        printk(KERN_ERR "Fail to register a major number\n");
        return major_number;
    }
    
    ds3231_class = class_create(CLASS_NAME);
    if( IS_ERR(ds3231_class))
    {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "Fail to register device class\n");
        return PTR_ERR(ds3231_class);
    }

    ds3231_device = device_create(ds3231_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);  
    if( IS_ERR(ds3231_device))
    {
        class_destroy(ds3231_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "Fail to create the device \n");
        return PTR_ERR(ds3231_device);
    }

    printk(KERN_INFO "DS3231 Driver Installed\n");
    return 0;
}

static void ds3231_remove(struct i2c_client *client)
{
    device_destroy(ds3231_class, MKDEV(major_number,0));
    class_unregister(ds3231_class);
    class_destroy(ds3231_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    printk(KERN_INFO "DS3231 Driver Removed\n");
}

static const struct of_device_id ds3231_of_match[] =
{
    { .compatible = "dallas,ds3231", },
    {},
};
MODULE_DEVICE_TABLE(of, ds3231_of_match);

static struct i2c_driver DS3231_Driver = {
    .driver = {
        .name = DRIVER_NAME,
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(ds3231_of_match),
    },
    .probe = ds3231_probe,
    .remove = ds3231_remove,
};

static int __init ds3231_init(void)
{
    printk(KERN_INFO "Initializing DS3231 Driver\n");
    return i2c_add_driver(&DS3231_Driver);
}

static void __exit ds3231_exit(void)
{
    printk(KERN_INFO "Exiting DS3231 driver\n");
    i2c_del_driver(&DS3231_Driver);
}

module_init(ds3231_init);
module_exit(ds3231_exit);

MODULE_AUTHOR("VVan");
MODULE_DESCRIPTION("DS3231 I2C Client Driver with IOCTL Interface");
MODULE_LICENSE("GPL");
