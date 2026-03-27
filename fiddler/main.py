import machine
import time

# --- LED Indicators ---
LED_GOOD = machine.Pin(14, machine.Pin.OUT)      # "Good to Go" (Green)
LED_ERROR = machine.Pin(15, machine.Pin.OUT)     # "I2C Error" (Red)
LED_HEARTBEAT = machine.Pin(16, machine.Pin.OUT) # Heartbeat (Blue)

# Variables for non-blocking heartbeat
last_heartbeat = time.ticks_ms()
heartbeat_interval = 500 # 0.5 seconds

# --- PCA9685 Driver ---
class PCA9685:
    def __init__(self, i2c, address=0x40):
        self.i2c = i2c
        self.address = address
        self.active = False
        self.init_pca()

    def init_pca(self):
        try:
            self.i2c.writeto_mem(self.address, 0x00, b'\x00') 
            time.sleep_ms(10)
            self.i2c.writeto_mem(self.address, 0x00, b'\x10') 
            self.i2c.writeto_mem(self.address, 0xFE, b'\x79') # 50Hz
            self.i2c.writeto_mem(self.address, 0x00, b'\xA1') 
            self.active = True
            print("I2C Good to Go")
            LED_GOOD.value(1)
            LED_ERROR.value(0)
        except Exception as e:
            print(f"I2C Error: PCA9685 not responding - {e}")
            self.active = False
            LED_GOOD.value(0)
            LED_ERROR.value(1)

    def set_pwm(self, channel, duty_us):
        if not self.active: return
        duty = int(duty_us * 4096 / 20000)
        try:
            self.i2c.writeto_mem(self.address, 0x06 + 4 * channel, 
                                 bytes([0, 0, duty & 0xFF, duty >> 8]))
        except:
            self.active = False
            LED_GOOD.value(0)
            LED_ERROR.value(1)

# --- Full 16-Channel SBUS Reader ---
class SBUSReader:
    def __init__(self):
        self.uart = machine.UART(1, baudrate=100000, bits=8, parity=0, stop=2, rx=machine.Pin(9), invert=machine.UART.INV_RX)
        self.mapped_channels = [1500] * 16
        self.failsafe = True

    def update(self):
        if self.uart.any() >= 25:
            start_byte = self.uart.read(1)
            if start_byte == b'\x0f':
                frame = self.uart.read(24)
                if len(frame) == 24 and frame[23] == 0x00:
                    self._decode(frame)
                    return True
                else:
                    self.uart.read(self.uart.any())
        return False

    def _decode(self, f):
        c = [0] * 16
        c[0]  = (f[0]  | f[1] << 8) & 0x07FF
        c[1]  = (f[1] >> 3 | f[2] << 5) & 0x07FF
        c[2]  = (f[2] >> 6 | f[3] << 2 | f[4] << 10) & 0x07FF
        c[3]  = (f[4] >> 1 | f[5] << 7) & 0x07FF
        c[4]  = (f[5] >> 4 | f[6] << 4) & 0x07FF
        c[5]  = (f[6] >> 7 | f[7] << 1 | f[8] << 9) & 0x07FF
        c[6]  = (f[8] >> 2 | f[9] << 6) & 0x07FF
        c[7]  = (f[9] >> 5 | f[10] << 3) & 0x07FF
        c[8]  = (f[11] | f[12] << 8) & 0x07FF
        c[9]  = (f[12] >> 3 | f[13] << 5) & 0x07FF
        c[10] = (f[13] >> 6 | f[14] << 2 | f[15] << 10) & 0x07FF
        c[11] = (f[15] >> 1 | f[16] << 7) & 0x07FF
        c[12] = (f[16] >> 4 | f[17] << 4) & 0x07FF
        c[13] = (f[17] >> 7 | f[18] << 1 | f[19] << 9) & 0x07FF
        c[14] = (f[19] >> 2 | f[20] << 6) & 0x07FF
        c[15] = (f[20] >> 5 | f[21] << 3) & 0x07FF
        
        self.failsafe = bool(f[22] & 0x08)
        
        for i in range(16):
            val = int((c[i] - 172) * 1000 / 1639 + 1000)
            self.mapped_channels[i] = val if abs(val - 1500) > 20 else 1500

# --- Execution ---
i2c = machine.I2C(0, sda=machine.Pin(0), scl=machine.Pin(1), freq=400000)
pca = PCA9685(i2c)
sbus = SBUSReader()

print("Full 16-Ch SBUS Parser Online...")

while True:
    # 1. Heartbeat Logic (Non-blocking)
    current_time = time.ticks_ms()
    if time.ticks_diff(current_time, last_heartbeat) >= heartbeat_interval:
        LED_HEARTBEAT.toggle()
        last_heartbeat = current_time

    # 2. SBUS and PCA9685 Logic
    if sbus.update():
        if sbus.failsafe:
            for i in range(16): pca.set_pwm(i, 1500)
        else:
            ch = sbus.mapped_channels
            
            # Mecanum Mixing
            sway, surge, yaw = ch[0]-1500, ch[1]-1500, ch[3]-1500
            p0 = 1500 + surge + sway + yaw  # Front Left
            p1 = 1500 + surge - sway - yaw  # Front Right
            p2 = 1500 + surge - sway + yaw  # Rear Left
            p3 = 1500 + surge + sway - yaw  # Rear Right
            
            pca.set_pwm(0, max(1000, min(2000, p0)))
            pca.set_pwm(1, max(1000, min(2000, p1)))
            pca.set_pwm(2, max(1000, min(2000, p2)))
            pca.set_pwm(3, max(1000, min(2000, p3)))

            # Heave
            pca.set_pwm(4, ch[2])
            pca.set_pwm(5, ch[2])

            # Direct Mapping (Channels 5-14 to PWM 6-15)
            for i in range(10): 
                pca.set_pwm(i + 6, ch[i + 4])
    
    # Keep the loop tight for SBUS timing
    time.sleep_ms(1)
