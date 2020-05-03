g++ client.cpp -o client --std=c++11
g++ server.cpp -o server --std=c++11 -pthread
rmmod vga_driver.ko
insmod vga_driver.ko
