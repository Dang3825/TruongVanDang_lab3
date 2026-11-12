# Lab 3 - Đề 1: Mạch giám sát và cảnh báo người chạy (Runner)

**Họ và tên:** Trương Văn Đăng

**Lớp:** Kỹ thuật Điện tử - Tin học công nghiệp 2 K64

**Link Wokwi:** https://wokwi.com/projects/480115214305552385

**Link báo cáo:** [Xem báo cáo Lab 3](Tr%C6%B0%C6%A1ng%20V%C4%83n%20%C4%90%C4%83ng%20_%20lab3.docx)

## 1. Giới thiệu

Dự án xây dựng mạch giám sát và cảnh báo người chạy (Runner) sử dụng ESP32 kết hợp với cảm biến BMP180. Hệ thống có chức năng đo nhiệt độ, áp suất khí quyển và tính toán độ cao tương đối so với mực nước biển.

Dữ liệu được xử lý trên ESP32, hiển thị qua Serial Monitor và truyền lên MQTT Broker thông qua Wi-Fi để theo dõi trên giao diện Web.

## 2. Chức năng chính

- Đo nhiệt độ và áp suất khí quyển bằng cảm biến BMP180.
- Tính toán độ cao dựa trên áp suất khí quyển.
- Hiển thị dữ liệu trên Thiết kế Mạch Giám sát và cảnh báo người chạy (runner). Mạch này sử dụng ESP32 đọc dữ liệu nhiệt độ và áp lực khí quyển => độ cao (so với mực nước biển) từ cảm biến BMP180, hiển thị dữ liệu qua giao tiếp serial. Đồng thời ESP32 gửi dữ liệu lên MQTT Broker qua Wifi và hiển thị dữ liệu trên Web/App. Yêu cầu thực hiện:
- • Vẽ mạch kết nối trong Wokwi.
- • Thiết kế giao diện trên Web/App với các dữ liệu đọc được từ Broker hiển thị trên thẻ html "class" và nút “button” để điều khiển cơ cấu chấp hành:
-   + Các thẻ html hiển thị dữ liệu nhiệt độ, áp lực khí quyển và độ cao (so với mực nước biển) dạng số.
    + 2 biểu đồ cập nhật dữ liệu nhiệt độ và độ cao, trực tiếp theo thời gian thực (tùy chọn).
    + 2 nút button để điều khiển trạng thái Led và Loa cảnh báo (chế độ manual).
- • Xây dựng hệ RTOS cho hoạt động của ESP32, với các yêu cầu:
-   + “Task truyền data”: kết nối Wifi chạy Core 0, nếu thành công, đọc dữ liệu từ hàng đợi “data Queue”, và gửi lên MQTT broker.
    + “Task đọc data”, chạy Core 1: đọc dữ liệu cảm biến BMP180, hiển thị dữ liệu qua giao tiếp serial, gửi dữ liệu đầy đủ vào hàng đợi “data Queue” và dữ liệu cảnh báo gồm nhiệt độ + độ cao vào hàng đợi “data Alert”
    + “Tast cảnh báo”, chạy Core 1: đọc dữ liệu tử hàng đợi “data Alert” , và thực hiện Cảnh báo bằng Còi/Đèn khi nhiệt độ người chạy (runner) quá ngưỡng hoặc dưới ngưỡng & Cảnh báo khi có sự thay đổi độ cao đột ngột.Serial Monitor.
- Truyền dữ liệu cảm biến lên MQTT Broker.
- Hiển thị nhiệt độ, áp suất và độ cao trên giao diện Web.
- Tự động cảnh báo bằng LED và Buzzer khi phát hiện thông số bất thường.
- Điều khiển LED và Buzzer từ giao diện Web ở chế độ MANUAL.
- Sử dụng FreeRTOS để thực hiện nhiều Task đồng thời.

## 3. Công nghệ và linh kiện sử dụng

- **Vi điều khiển:** ESP32 DevKit V1.
- **Cảm biến:** BMP180.
- **Thiết bị cảnh báo:** LED, điện trở, Buzzer.
- **Lập trình:** Arduino IDE, C++, FreeRTOS.
- **Giao tiếp:** I2C.
- **Truyền dữ liệu:** Wi-Fi, MQTT.
- **MQTT Broker:** broker.hivemq.com.
- **Giao diện:** HTML, CSS, JavaScript, MQTT.js.

## 4. Nguyên lý hoạt động

1. Cảm biến BMP180 đo nhiệt độ và áp suất khí quyển.
2. ESP32 tiếp nhận dữ liệu và tính toán độ cao.
3. Task đọc dữ liệu đưa thông tin vào Queue.
4. Task truyền dữ liệu lấy thông tin từ Queue và gửi lên MQTT Broker.
5. Giao diện Web nhận dữ liệu và hiển thị các thông số theo thời gian thực.
6. Task cảnh báo kiểm tra nhiệt độ và sự thay đổi độ cao để kích hoạt LED, Buzzer.
7. Người dùng có thể điều khiển LED và Buzzer thông qua giao diện Web khi hệ thống ở chế độ MANUAL.

## 5. Ngưỡng cảnh báo

| Thông số | Điều kiện                              | Trạng thái  |
| -------- | -------------------------------------- | ----------- |
| Nhiệt độ | Dưới 10°C                              | Cảnh báo    |
| Nhiệt độ | Từ 10°C đến 35°C                       | Bình thường |
| Nhiệt độ | Trên 35°C                              | Cảnh báo    |
| Độ cao   | Chênh lệch giữa hai lần đo lớn hơn 5 m | Cảnh báo    |

Khi phát hiện điều kiện bất thường, hệ thống chuyển sang chế độ AUTO và kích hoạt LED, Buzzer. Khi các thông số trở về bình thường, hệ thống chuyển về MANUAL.

## 6. Cơ chế FreeRTOS

Hệ thống được chia thành ba Task:

| Task             | Core   | Chức năng                                                      |
| ---------------- | ------ | -------------------------------------------------------------- |
| TaskReadData     | Core 1 | Đọc cảm biến BMP180, hiển thị Serial và gửi dữ liệu vào Queue. |
| TaskTransmitData | Core 0 | Kết nối Wi-Fi, MQTT và truyền dữ liệu lên Broker.              |
| TaskAlert        | Core 1 | Kiểm tra điều kiện cảnh báo và điều khiển LED, Buzzer.         |

Hệ thống sử dụng hai Queue:

- `dataQueue`: truyền dữ liệu nhiệt độ, áp suất và độ cao.
- `dataAlert`: truyền dữ liệu nhiệt độ và độ cao phục vụ cảnh báo.

## 7. Hệ thống MQTT Topic

| Topic                    | Chức năng                                |
| ------------------------ | ---------------------------------------- |
| `runner/data`            | Gửi dữ liệu nhiệt độ, áp suất và độ cao. |
| `runner/control/led`     | Nhận lệnh điều khiển LED.                |
| `runner/control/buzzer`  | Nhận lệnh điều khiển Buzzer.             |
| `runner/status/led`      | Cập nhật trạng thái LED.                 |
| `runner/status/buzzer`   | Cập nhật trạng thái Buzzer.              |
| `runner/status/led_mode` | Cập nhật chế độ AUTO/MANUAL.             |

## 8. Cấu trúc dự án

```text
Lab_3/
├── README.md
├── lab3.ino
├── lab_3.html
├── diagram.json
├── libraries.txt
├── Truong_Van_Dang_lab3.docx
└── Truong_Van_Dang_N02.zip
```

## 9. Mục tiêu

Xây dựng mô hình giám sát và cảnh báo người chạy ứng dụng công nghệ IoT, giúp theo dõi các thông số môi trường, phát hiện những thay đổi bất thường và hỗ trợ điều khiển thiết bị cảnh báo từ xa thông qua giao diện Web.
