# DS3231-Driver-for-Raspberry-Pi
DS3231 Driver for Raspberry Pi 

# Yêu cầu:
+ RaspberryPi
+ Module DS3231
+ Header tương ứng

# Chức năng các file
+ DS3231_Driver.c : file source driver
+ Makefile: file chứa lệnh make và make clean để biên dịch file driver sang .ko
+ Example.c : file code mẫu sử dụng driver ở user space.

# Cách biên dịch và sử dụng
Bước 1: Tải các file trên về và đặt chung một thư mục
Bước 2: Chuyển terminal đến thư mục đó và gõ
```bash
make
```
Bước 3: Đợi hệ thống biên dịch xong, ta sẽ có được file "DS3231_Driver.ko". Hãy gõ lệnh bên dưới để cài driver vào hệ thống
```bash
sudo insmod DS3231_Driver.ko
```
Bước 4: Chuyển terminal đến thư mục chứa file device tree của hệ thống
```bash
cd /boot/firmware
```
Bước 5: Chỉnh sửa file device tree. Trước tiên cần chuyển đổi file từ đuôi .dtb sang .dts bằng lệnh
```bash
dtc -I dtb -O dts -o "ten_file_dts".dts "ten_file_dtb".dtb
```
 Bước 6: Chỉnh sửa file .dts vừa chuyển đổi bằng nano
 ```bash
 sudo nano "ten_file_dts.dts"
 ```
 Bước 7: Thêm vào module i2c1 dòng lệnh
 ```bash
```
Bước 8: Lưu và chuyển đổi lại sang file .dtb bằng lệnh
```bash
dtc -I dts -O dtb -o "ten_file_dtb".dtb "ten_file_dts".dts
```
Bước 9: Biên dịch file Example.c để chạy thử
```bash
gcc Example.c -o example
```
Bước 10: Chạy file Example
```bash
sudo ./example
```
