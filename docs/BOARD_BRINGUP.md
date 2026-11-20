# Nạp và xác định thông số board trước khi chạy ứng dụng

Bản sửa hiện nằm trên branch local `fix/board-bringup-runtime`. Không có thông số đo phần cứng nào được tự coi là đã xác minh. Chế độ mặc định là **chẩn đoán**, không phải chu kỳ Auto. Đây là bước cần làm với board đang có nhưng chưa đo.

## 1. Chọn đúng chế độ

Mở `Core/Inc/board_config.h`:

- `APP_DIAGNOSTIC_MODE = 1`: nạp lần đầu để đo. Chạy scheduler/TIM2, đọc trực tiếp bốn nút, quét I2C, thử DHT20/LCD sau scan; không chạy FSM giao thông. Các chân đèn khởi động ở input, không tự quét mã màu.
- `APP_DIAGNOSTIC_MODE = 0`: chạy Auto/Manual/Tuning sau khi điền bảng mã, mức nút và bật hai cờ xác minh. Nếu thiếu thông số, compiler báo lỗi thay vì tạo firmware âm thầm vô hiệu hóa nút/đèn.

Không bật BUTTON_HW_VERIFIED/TRAFFIC_HW_VERIFIED chỉ để hết lỗi build. Chúng ghi nhận kết quả đo, không phải phép đo tự động. Giữ mức nguồn và SWD theo nhãn thật trên bộ nạp.

## 2. Build và nạp bản chẩn đoán bằng CubeIDE

1. Lưu code, chọn project rồi F5. Core/Src phải hiện board_diagnostics.c và Core/Inc có board_config.h, board_diagnostics.h.
2. Kiểm tra APP_DIAGNOSTIC_MODE=1. Project > Clean rồi Build Project.
3. Debug Configurations: chọn đúng project và file ELF trong Debug của project, ST-LINK, giao tiếp SWD. Nạp bằng Debug.
4. Khi dừng ở main, nhấn Resume (F8). Code không tự tiếp tục nếu debugger còn đang Suspend.
5. Sau một giây nhấn Suspend. Mở Window > Show View > Expressions; nếu không có mục Expressions trực tiếp thì chọn Other > Debug > Expressions.
6. Thêm các biểu thức: g_board_diag, g_i2c_scan. Chúng là biến toàn cục, không phụ thuộc stack frame hiện tại.

`g_board_diag.alive` tăng sau các lượt callback. `tick_ms` là HAL_GetTick. LD2/PA5 được toggle mỗi khoảng 500 ms. Nếu alive không tăng khi Resume, kiểm tra timer/clock/SWD, không đổi bảng mã đèn để chữa lỗi scheduler.

## 3. Xác định mức nút

Bốn ô raw_buttons tương ứng:

| Chỉ số | Nút | Chân STM32 |
|---|---|---|
| 0 | BTN1 | PA8 |
| 1 | BTN2 | PA9 |
| 2 | BTN3 | PC7 |
| 3 | BTN4 | PB6 |

Đọc một lần khi không nhấn: Resume khoảng nửa giây rồi Suspend. Ghi kết quả 0/1. Giữ nút cần thử, Resume rồi Suspend trong khi vẫn giữ nút và đọc lại. Lặp đủ bốn nút. Các nút phải có mức idle ổn định; nếu trôi/ngẫu nhiên, kiểm tra nguồn, GND, dây và điện trở kéo. GPIO hiện giữ NOPULL theo thiết kế ban đầu, không tự thêm pull-up nếu chưa biết mạch.

Điền BTN1_ACTIVE_LEVEL..BTN4_ACTIVE_LEVEL trong board_config.h bằng **mức khi nhấn**. Ví dụ đo thấy nhấn LOW thì ghi 0; nhấn HIGH thì ghi 1. Có thể khác nhau theo từng nút. Khi đã đo đủ và ổn định, đặt BUTTON_HW_VERIFIED=1.

## 4. Quét I2C và xem cảm biến

Phiên scan tự bắt đầu ở chế độ diagnostic. Sau ít nhất khoảng hai giây Resume rồi Suspend, kiểm tra:

- g_i2c_scan.complete phải true; detected[0x38] dự kiến ACK cho DHT20.
- Xem bitmap detected để tìm địa chỉ LCD thật, thường gặp 0x27/0x3F nhưng phải dùng giá trị đo được. device_count là số địa chỉ ACK, không xác định danh tính IC.
- bus_error_count đếm lỗi ngoài NACK đơn thuần; nếu tăng, kiểm tra dây/pull-up/nguồn và đúng I2C1 PB8=SCL, PB9=SDA.

Sau scan, DHT20 được service mỗi khoảng 100 ms và LCD 200 ms. Chỉ hai driver này dùng bus, không chạy lại scan liên tục. Sensor ERROR sẽ chờ hai giây rồi thử lại. Đọc struct data trong dht20.c bằng breakpoint hoặc Expressions có scope phù hợp; getter là API cho code, không cần gọi hàm thủ công trong debugger.

Nếu muốn quét lại: Suspend, đặt g_board_diag.command=1, Resume, chờ rồi Suspend kiểm tra complete. Driver cảm biến tự khởi động lại sau phiên scan. Sửa TV3_LCD_ADDRESS trong board_config.h theo địa chỉ đo, rebuild và nạp lại. LCD mẫu là HD44780 16x2 + PCF8574: RS=P0, RW=P1, E=P2, BL=P3 active HIGH, D4..D7=P4..P7. Nếu controller/mapping khác, chỉ đổi địa chỉ là chưa đủ; phải gửi ảnh hoặc sơ đồ module để sửa driver.

## 5. Đo bảng mã đèn theo lệnh thủ công

Trong lúc đo, chỉ nối **một cụm đèn cần thử**; ngắt cụm còn lại để tránh hai đường cùng xanh khi chưa biết mã. Input/high-impedance không có nghĩa module chắc chắn tắt. Đây là phép thử bàn học, không phải điều khiển giao thông thật.

Mỗi lần đổi mã:

1. Suspend debugger.
2. Đặt g_board_diag.road=1 (cụm 1) hoặc 2 (cụm 2).
3. Đặt g_board_diag.code=0, 1, 2 hoặc 3 theo bảng dưới.
4. **Cuối cùng** đặt g_board_diag.command=2.
5. Resume để áp mã. Quan sát đèn rồi Suspend, ghi màu. command sẽ về 0, last_command=2, result=0 nếu yêu cầu hợp lệ. Nếu chưa Resume thì lệnh chưa chạy.

| code | A | B | Màu cụm 1 đo được | Màu cụm 2 đo được |
|---|---|---|---|---|
| 0 | 0 | 0 | ... | ... |
| 1 | 0 | 1 | ... | ... |
| 2 | 1 | 0 | ... | ... |
| 3 | 1 | 1 | ... | ... |

Cụm 1: A=PB3, B=PB5. Cụm 2: A=PB4, B=PB10. Đo từng cụm riêng, không suy ra cụm 2 giống cụm 1.

Muốn trả các chân đèn về input: đặt command=3 rồi Resume. Muốn áp mã khác thì nhập road/code trước, command sau. Command hoặc đối số không hợp lệ cho result=1; không phải lỗi phần cứng.

Sau khi xác định đủ bốn màu, thêm tám define vào board_config.h với các **giá trị đo thực tế**:

```c
/* Điền số 0..3 đã đo cho mỗi tên, không chép các số giả từ test. */
/* TL1_RED_CODE, TL1_AMBER_CODE, TL1_GREEN_CODE, TL1_OFF_CODE */
/* TL2_RED_CODE, TL2_AMBER_CODE, TL2_GREEN_CODE, TL2_OFF_CODE */
```

Mỗi cụm cần bốn mã phân biệt trong 0..3. Nếu không có bốn trạng thái Red/Amber/Green/Off đúng như phần cứng yêu cầu, dừng để kiểm tra wiring/module, không gán tên màu tùy ý. Sau khi xác minh đặt TRAFFIC_HW_VERIFIED=1.

## 6. Chuyển sang chạy Auto/Manual/Tuning

Điền đủ thông số đo như trên, rồi APP_DIAGNOSTIC_MODE=0. Clean, build và nạp lại đúng ELF mới. Mặc định dự án bắt đầu Auto sau clearance đỏ một giây; BTN1 SHORT đổi mode; Manual BTN2 SHORT NEXT, BTN3 DOWN STOP, BTN4 DOWN RESTART; Tuning BTN2 SHORT chọn trường, BTN3/4 DOWN tăng/giảm, BTN2 LONG Save, BTN1 rời mode hủy thay đổi chưa lưu. Giữ BTN3/4 không tự tăng lặp.

Quan sát đủ sáu pha và hai clearance, thử STOP/RESTART, sáu trường chỉnh và biên, Save/Cancel, kiểm tra không GREEN/GREEN, rút DHT20/LCD và theo dõi control vẫn chạy. Đo jitter/time I2C và chạy ổn định tối thiểu 30 phút trước ghi HARDWARE PASS.

Runtime preload mã RED đã xác minh vào latch GPIO trước khi bật các chân output để giảm glitch lúc startup. Chẩn đoán giữ các chân input cho đến lệnh thử thủ công. Hai cách khởi tạo có chủ ý khác nhau.

## 7. Kiểm thử trên máy

PowerShell ở thư mục repo:

```powershell
./tests/tv3/run.ps1
./tests/tv2/run.ps1
./tests/bringup/run.ps1
```

ARM builder có ba profile:

- Diagnostic: ảnh chẩn đoán với APP_DIAGNOSTIC_MODE=1.
- ConfiguredRuntime: dùng bảng mã thật đã điền, compiler từ chối nếu chưa xác minh.
- TestRuntime: chỉ kiểm tra compile/link với các giá trị GPIO giả do bộ test cung cấp. **Không được nạp ELF TestRuntime lên board.**

Host mocks chứng minh logic, không chứng minh mức nút/mã màu/địa chỉ/điện áp của thiết bị thật. HAL I2C blocking vẫn có các chặng chờ nội bộ; tham số timeout 2 ms không bảo đảm toàn callback dưới hai ms. Chưa chạy phần cứng thì chỉ ghi SOFTWARE PASS.

## Kết quả kiểm tra bản sửa ngày 20/11/2026

- PASS 11 nhóm scheduler, 10 nhóm software timer, 11 nhóm config, 11 nhóm mode manager.
- PASS TV2: 16 button, 10 traffic driver, 12 Auto, 11 Manual và bộ integration.
- PASS bộ test TV3 hiện có: LCD/DHT20/scanner/Tuning, timeout/CRC, staged và giới hạn.
- PASS test diagnostic mới: khởi động input-only, nhường bus scanner, mức raw nút, tám yêu cầu road/code, lệnh sai, bỏ output và quét lại.
- PASS kiểm tra guard cấu hình: diagnostic mặc định được build; runtime thiếu xác minh hoặc thiếu mã màu bị từ chối; bảng mã trùng và mức nút ngoài 0/1 bị từ chối; bộ thông số giả hợp lệ được compiler chấp nhận.
- PASS ARM GCC 13.3.1 compile/link profile Diagnostic và TestRuntime. TestRuntime chỉ là bài kiểm tra compile với dữ liệu giả, không phải ảnh nạp board. Profile ConfiguredRuntime chưa thể được coi là đạt phần cứng khi chưa điền kết quả đo thật.
- Còn warning unused-parameter ở syscalls.c và RWX segment từ linker script hiện có; không coi đây là zero-warning build. Warning so sánh enum trong traffic đã được sửa.
- Chưa flash hoặc quan sát board, chưa đo điện/timing/jitter và chưa có bằng chứng chạy ổn định phần cứng.

## Bổ sung chẩn đoán khi đổi code mà màu không đổi

Bản cập nhật thêm applied_road, applied_code, applied_count và các thanh ghi GPIOB đọc ngược vào g_board_diag. result=2 nghĩa latch ODR không khớp lệnh yêu cầu; result=0 chỉ xác nhận latch, không xác nhận màu đèn. pin_code là hai bit A/B đọc từ IDR sau khi cấu hình output, không phải mã màu tự nhận diện.

Sau lệnh command=2 hợp lệ đầu tiên, chỉ cần Suspend, đổi code, Resume; firmware tự áp lại khi road/code thay đổi. Command=3 trả input và dừng chế độ theo dõi này. Mặc định sau reset vẫn không tự xuất mã.

Với road=1, gpio_b_odr & 0x28 dự kiến là 0, 32, 8, 40 ứng với code 0,1,2,3. applied_count phải tăng mỗi lần áp mã. Nếu pin_code đổi đúng mà LED không đổi, đối chiếu chân cắm/wiring với PB3/PB5; nếu pin_code không khớp, kiểm tra GPIO mode, chân bị ngoại vi chiếm hoặc tín hiệu bên ngoài. Không tự suy ra bảng màu trước khi quan sát đúng cụm đèn.

Sau I2C remap, code tái khẳng định NOJTAG để bảo đảm PB3/PB4 dùng GPIO, vẫn giữ SWD PA13/PA14. Đây là biện pháp bảo đảm cấu hình, chưa khẳng định JTAG là nguyên nhân của hiện tượng trên board.

## Đếm giây và giữ nút trong Tuning

AUTO: dòng trên `1 R:14s 2 G:10s` (đường 1 đỏ còn 14 giây; đường 2 xanh còn 10 giây). R=đỏ, A=vàng, G=xanh. Đếm đỏ bao gồm các pha còn lại đến khi đường đó chuyển xanh. Dòng dưới vẫn là nhiệt độ/độ ẩm.

MANUAL: hiện màu từng đường, không đếm ngược vì chuyển pha bằng nút NEXT.

TUNING: BTN3 tăng, BTN4 giảm. Nhấn đổi 1 giây; giữ 1 giây bắt đầu lặp mỗi 200 ms. Thả nút dừng lặp. Vẫn giữ giới hạn cấu hình và BTN2 giữ để lưu. Lặp chỉ hoạt động trong Tuning.

Nạp lại: dừng Debug, Refresh project, Clean/Build, Debug rồi F8. Kiểm tra cả đếm giây và giữ/thả nút trên board.
