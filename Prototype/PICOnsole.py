from machine import Pin, SPI, PWM
import framebuf
import gc
import sdcard
import sys
import time
import uos
    
def clamp(v, a, b):
    if v < a:
        return a
    if v > b:
        return b
    return v

class path:
    @staticmethod
    def split(full_path):
        slash = full_path.rfind("/")
        return [full_path[:slash], full_path[slash+1:]]
    
    @staticmethod
    def basename(full_path):
        return path.split(full_path)[1]
    
    @staticmethod
    def dirname(full_path):
        return path.split(full_path)[0]
    
    @staticmethod
    def join(a, b):
        return (a + "/" + b).replace("//", "/").rstrip("/")
    
    @staticmethod
    def exists(full_path):
        directory, name = path.split(full_path)
        contents = uos.listdir(directory)
        return name in contents
    
    @staticmethod
    def isfile(full_path):
        name = path.basename(full_path)
        for x in uos.ilistdir(path.dirname(full_path)):
            if name == x[0]:
                return (x[1] & 0x4000) == 0 and (x[1] & 0x8000) != 0
        return False
        
    @staticmethod
    def isdir(full_path):
        name = path.basename(full_path)
        for x in uos.ilistdir(path.dirname(full_path)):
            if name == x[0]:
                return (x[1] & 0x4000) != 0 and (x[1] & 0x8000) == 0
        return False

class Sprite(framebuf.FrameBuffer):
    def __init__(self, buffer = None, width = 0, height = 0, sprite_path = None):
        self.width = width
        self.height = height
        self.transparent_color = -1
        if buffer != None:
            self.buffer = buffer
        else:
            self.buffer = bytearray(width * height * 2)
        if sprite_path != None:
            self._load(sprite_path)
        super().__init__(self.buffer, self.width, self.height, framebuf.RGB565)
    
    def _load(self, sprite_path):
        with open(sprite_path, "rb") as sprite_file:
            header = sprite_file.read(len("PICOSPRITE"))
            if header != "PICOSPRITE".encode("ascii"):
                print("Failed to load sprite (bad header):", sprite_path)
                return
            transparency_byte = sprite_file.read(1)
            if transparency_byte == "O".encode("ascii"):
                sprite_file.read(2) # Skip transparency color bytes
            elif transparency_byte == "T".encode("ascii"):
                self.transparent_color = int.from_bytes(sprite_file.read(2), "big")
            else:
                print("Failed to load sprite (bad transparency byte):", sprite_path)
                return
            self.height = int.from_bytes(sprite_file.read(4), "big")
            self.width = int.from_bytes(sprite_file.read(4), "big")
            self.buffer = bytearray(sprite_file.read(self.height * self.width * 2))

class Program:
    def __init__(self):
        self.debugging = False

    def start(self, root_directory, LCD, console_os):
        self.directory = root_directory
        self.LCD = LCD
        self.console_os = console_os
    
    def update(self, inputs):
        return False
    
    def stop(self):
        pass
    
    @classmethod
    def bootstrap(my_class, directory = "/"):
        picos = None
        try:
            picos = OS()
            picos.set_backlight_strength(0.5)
            picos.start_standalone_program(my_class(), directory)
            while picos.is_active():
                picos.update()
        except Exception as e:
            if picos != None:
                picos.set_backlight_strength(0)
            raise e

class LCD_1inch8(framebuf.FrameBuffer):
    @staticmethod
    def color(r, g, b):
        r = int(r * 0b11111 / 255)
        if r < 0:
            r = 0
        else:
            r = r << 3
        g = int(g * 0b111111 / 255)
        if g < 0:
            g = 0
        else:
            g = (g & 0b000111) << 13 | (g & 0b111000) >> 3
        b = int(b * 0b11111 / 255)
        if b < 0:
            b = 0
        else:
            b = b << 8
        return r + b + g
    
    def __init__(self):
        self.width = 160
        self.height = 128
        
        self.cs = Pin(9,Pin.OUT)
        self.rst = Pin(12,Pin.OUT)
        
        self.cs(1)
        self.spi = SPI(1, 80_000_000, polarity=0, phase=0, sck=Pin(10), mosi=Pin(11), miso=None)
        self.dc = Pin(8,Pin.OUT)
        self.dc(1)
        self.buffer = bytearray(self.height * self.width * 2)
        super().__init__(self.buffer, self.width, self.height, framebuf.RGB565)
        self.init_display()
        

        self.WHITE      = 0xFFFF
        self.LIGHT_GREY = self.color(192, 192, 192)
        self.GREY       = self.color(128, 128, 128)
        self.DARK_GREY  = self.color(64, 64, 64)
        self.BLACK      = 0x0000
        self.GREEN      = 0x001F
        self.BLUE       = 0xF800
        self.RED        = 0x07E0
        self.YELLOW     = self.RED  | self.GREEN
        self.CYAN       = self.BLUE | self.GREEN
        self.MAGENTA    = self.RED  | self.BLUE

    def write_cmd(self, cmd):
        self.cs(1)
        self.dc(0)
        self.cs(0)
        self.spi.write(bytearray([cmd]))
        self.cs(1)

    def write_data(self, buf):
        self.cs(1)
        self.dc(1)
        self.cs(0)
        self.spi.write(bytearray([buf]))
        self.cs(1)

    def init_display(self):
        """Initialize dispaly"""  
        self.rst(1)
        self.rst(0)
        self.rst(1)
        
        self.write_cmd(0x36);
        self.write_data(0x70);
        
        self.write_cmd(0x3A);
        self.write_data(0x05);

         #ST7735R Frame Rate
        self.write_cmd(0xB1);
        self.write_data(0x01);
        self.write_data(0x2C);
        self.write_data(0x2D);

        self.write_cmd(0xB2);
        self.write_data(0x01);
        self.write_data(0x2C);
        self.write_data(0x2D);

        self.write_cmd(0xB3);
        self.write_data(0x01);
        self.write_data(0x2C);
        self.write_data(0x2D);
        self.write_data(0x01);
        self.write_data(0x2C);
        self.write_data(0x2D);

        self.write_cmd(0xB4); #Column inversion
        self.write_data(0x07);

        #ST7735R Power Sequence
        self.write_cmd(0xC0);
        self.write_data(0xA2);
        self.write_data(0x02);
        self.write_data(0x84);
        self.write_cmd(0xC1);
        self.write_data(0xC5);

        self.write_cmd(0xC2);
        self.write_data(0x0A);
        self.write_data(0x00);

        self.write_cmd(0xC3);
        self.write_data(0x8A);
        self.write_data(0x2A);
        self.write_cmd(0xC4);
        self.write_data(0x8A);
        self.write_data(0xEE);

        self.write_cmd(0xC5); #VCOM
        self.write_data(0x0E);

        #ST7735R Gamma Sequence
        self.write_cmd(0xe0);
        self.write_data(0x0f);
        self.write_data(0x1a);
        self.write_data(0x0f);
        self.write_data(0x18);
        self.write_data(0x2f);
        self.write_data(0x28);
        self.write_data(0x20);
        self.write_data(0x22);
        self.write_data(0x1f);
        self.write_data(0x1b);
        self.write_data(0x23);
        self.write_data(0x37);
        self.write_data(0x00);
        self.write_data(0x07);
        self.write_data(0x02);
        self.write_data(0x10);

        self.write_cmd(0xe1);
        self.write_data(0x0f);
        self.write_data(0x1b);
        self.write_data(0x0f);
        self.write_data(0x17);
        self.write_data(0x33);
        self.write_data(0x2c);
        self.write_data(0x29);
        self.write_data(0x2e);
        self.write_data(0x30);
        self.write_data(0x30);
        self.write_data(0x39);
        self.write_data(0x3f);
        self.write_data(0x00);
        self.write_data(0x07);
        self.write_data(0x03);
        self.write_data(0x10);

        self.write_cmd(0xF0); #Enable test command
        self.write_data(0x01);

        self.write_cmd(0xF6); #Disable ram power save mode
        self.write_data(0x00);

            #sleep out
        self.write_cmd(0x11);
        #DEV_Delay_ms(120);

        #Turn on the LCD display
        self.write_cmd(0x29);

    def show(self):
        self.write_cmd(0x2A)
        self.write_data(0x00)
        self.write_data(0x01)
        self.write_data(0x00)
        self.write_data(0xA0)
        
        self.write_cmd(0x2B)
        self.write_data(0x00)
        self.write_data(0x02)
        self.write_data(0x00)
        self.write_data(0x81)
        
        self.write_cmd(0x2C)
        
        self.cs(1)
        self.dc(1)
        self.cs(0)
        self.spi.write(self.buffer)
        self.cs(1)
    
    def centered_text(self, text, x, y, c = None):
        if c == None:
            c = self.WHITE
        text_width = len(text) * 8
        self.text(text, x - int(text_width / 2), y, c)
    
    def blit_advanced(self, sprite, dest_x, dest_y, src_x=0, src_y=0, src_w=-1, src_h=-1, key=-1, palette=None):
        if src_x < 0 or src_y < 0:
            return
        if dest_x > self.width or dest_y > self.height:
            return
        if src_w < 0:
            src_w = sprite.width - src_x
        if src_h < 0:
            src_h = sprite.height - src_x
        if dest_x < 0:
            src_x -= dest_x
            src_w += dest_x
            dest_x = 0
        if dest_y < 0:
            src_y -= dest_y
            src_h += dest_y
            dest_y = 0
        if src_w <= 0 or src_h <= 0:
            return
        if dest_x + src_w > self.width:
            src_w = self.width - dest_x
        if dest_y + src_h > self.height:
            src_h = self.height - dest_y
        color_mapper = lambda c: c
        if palette != None:
            color_mapper = lambda c: palette[c]
        key_checker = lambda c: c != key
        if key == None or key < 0:
            key_checker = lambda c: True
        for y in range(src_h):
            dest_line = dest_y + y
            src_line = src_y + y
            for x in range(src_w):
                c = sprite.pixel(x + src_x, src_line)
                if key_checker(c):
                    self.pixel(x + dest_x, dest_line, color_mapper(c))

class OS:
    SETTINGS_FILEPATH = "/sd/settings"
    @staticmethod
    def load_settings():
        # TODO
        pass
    
    @staticmethod
    def save_settings():
        # TODO
        pass
    
    def __init__(self):
        print("Setting up input pins")
        self.inputs = {
            "pins": {
                "cathode": [
                    Pin(16, Pin.OUT),
                    Pin(17, Pin.OUT),
                    Pin(18, Pin.OUT)
                    ],
                "anode": [
                    Pin(19, Pin.IN, Pin.PULL_DOWN),
                    Pin(20, Pin.IN, Pin.PULL_DOWN),
                    Pin(21, Pin.IN, Pin.PULL_DOWN)
                    ]
                },
            "pin_mapping": {
                'a': (0, 0),
                'b': (0, 1),
                'down': (0, 2),

                'x': (1, 0),
                'y': (1, 1),
                'up': (1, 2),
                
                'start': (2, 0),
                'right': (2, 1),
                'left': (2, 2),
                
                }
            }
        self.inputs["values"] = {name: 0 for name in self.inputs["pin_mapping"]}
        
        print("Creating Backlight PWM")
        try:
            self.backlight_pwm = PWM(Pin(13))
            self.backlight_pwm.freq(1000)
            self.set_backlight_strength(0.5)
        except Exception as e:
            self._fatal_os_error("Failed to create backlight PWM: " + str(e))
        
        print("Creating LCD interface")
        self.LCD = None
        try:
            self.LCD = LCD_1inch8()
        except Exception as e:
            self._fatal_os_error("Failed to Initialize LCD: " + str(e))
        try:
            self.LCD.fill(self.LCD.WHITE)
        except Exception as e:
            self._fatal_os_error("Failed to fill LCD with white: " + str(e))
        try:
            pi_logo = Sprite(sprite_path="/PiLogo.sprite")
        except Exception as e:
            self._fatal_os_error("Failed to load Raspberry Pi Logo sprite: " + str(e))
        try:
            self.LCD.blit(pi_logo, int(self.LCD.width / 2 - pi_logo.width / 2), int(self.LCD.height / 2 - pi_logo.height / 2))
        except Exception as e:
            self._fatal_os_error("Failed to blit Raspberry Pi Logo sprite: " + str(e))
        try:
            self.LCD.show()
        except Exception as e:
            self._fatal_os_error("Failed to show framebuffer to LCD: " + str(e))
            
        print("Creating SD interface")
        sd_spi = None
        try:
            sd_spi = SPI(0, 80_000_000, polarity=0, phase=0, bits=8, firstbit=SPI.MSB, sck=Pin(2, Pin.OUT), mosi=Pin(3, Pin.OUT), miso=Pin(4, Pin.IN))
        except Exception as e:
            self._fatal_os_error("Failed to create SPI interface for SD card: " + str(e))
        sd_cs = Pin(5, Pin.OUT)
        try:
            self.sd = sdcard.SDCard(sd_spi, sd_cs)
        except Exception as e:
            self._fatal_os_error("Failed to create SD card interface: " + str(e))
        
        print("Mounting SD card")
        try:
            self.vfs = uos.VfsFat(self.sd)
            uos.mount(self.vfs, "/sd")
        except Exception as e:
            self._fatal_os_error("Failed to mount SD card: " + str(e))
        if not path.exists("/sd/programs"):
            try:
                uos.mkdir("/sd/programs")
            except Exception as e:
                self._fatal_os_error("Failed to create programs folder on SD card: " + str(e))
        elif path.isfile("/sd/programs"):
            self._fatal_os_error("Failed to create programs folder on SD card: Path already exists as a file")
        try:
            sys.path.append("/sd/programs")
        except Exception as e:
            self._fatal_os_error("Failed to add programs folder to path:" + str(e))
        
        self.program_stack = []
        self.start_hold_time = None
    
    def error(self, message):
        print("Error:", message)
        
    def fatal_program_error(self, message):
        print("Fatal program error:", message)
        self._display_error_message("Fatal Prog Error", self.LCD.RED, message, self.LCD.WHITE)
        self.stop_active_program()
        
    def _fatal_os_error(self, message):
        print("Fatal OS error:", message)
        self._display_error_message("!FATAL OS ERROR!", self.LCD.RED, message, self.LCD.RED, False)
        raise Exception(message)
    
    def _display_error_message(self, title, title_color, message, message_color, wait_for_start = True):
        if self.LCD == None:
            print("Can't display error on LCD: LCD isn't initialized!")
            return
        self.LCD.fill_rect(4, 8, self.LCD.width - 8, self.LCD.height - 16, self.LCD.BLACK)
        self.LCD.rect(6, 10, self.LCD.width - 12, self.LCD.height - 20, self.LCD.WHITE)
        self.LCD.rect(8, 12, self.LCD.width - 16, self.LCD.height - 24, self.LCD.WHITE)
        center_x = int(self.LCD.width / 2)
        self.LCD.centered_text(title, center_x, 30, title_color)
        words = message.split()
        max_line_length = int((self.LCD.width - 20) / 8)
        max_lines = 5
        for line_index in range(max_lines):
            line = words[0]
            if len(line) > max_line_length:
                words[0] = line[max_line_length:]
                line = line[:max_line_length]
            else:
                word_index = 1
                while word_index < len(words):
                    word = words[word_index]
                    if len(line) + len(word) + 1 > max_line_length:
                        break
                    line += " " + word
                    word_index += 1
                words = words[word_index:]
            self.LCD.centered_text(line, center_x, 46 + line_index * 12, message_color)
            if len(words) == 0:
                break
        if len(words) > 0:
            self.LCD.centered_text(".....", center_x, 46 + max_lines * 12, message_color)
            
        self.LCD.show()
        
        if not wait_for_start:
            return
        print("Waiting for start to continue past error...")
        pin_pair = self.inputs["pin_mapping"]["start"]
        self.inputs["pins"]["cathode"][pin_pair[0]].on()
        while self.inputs["pins"]["anode"][pin_pair[1]].value() != 1:
            time.sleep_ms(16)
        self.inputs["pins"]["cathode"][pin_pair[0]].off()
    
    def start_builtin_program(self, program_name):
        print("Importing builtin module:", program_name)
        try:
            module = __import__(program_name, globals(), locals(), ["Program"], 0)
        except Exception as e:
            self._fatal_os_error("Failed to import builtin module: " + str(e))
            return
        try:
            program = module.Program()
        except Exception as e:
            self._fatal_os_error("Failed to initialize builtin Program: " + str(e))
            del module
            del sys.modules[name]
            return
        self.program_stack.append({"directory": "/", "name": program_name, "module": module, "program": program})
        del module
        if program.start("/", self.LCD, self) == False:
            self._fatal_os_error("Failed to start builtin program")
            self.stop_active_program()

    def start_standalone_program(self, program, directory="/"):
        print("Starting standalone program...")
        self.program_stack.append({"directory": directory, "name": None, "module": None, "program": program})
        if program.start(directory, self.LCD, self) == False:
            self._fatal_os_error("Failed to start builtin program")
            self.stop_active_program()
        
    def start_program(self, program_path):
        directory = program_path
        name = ""
        if directory.startswith("/sd/programs/"):
            name = directory[len("/sd/programs/"):]
        else:
            name = directory
            directory = path.join("/sd/programs/", directory)
        if name.endswith(".py"):
            name = name[:-3]
            directory = path.dirname(directory)
        name = name.strip("/").replace("/", ".")
        print("Importing module:", name, " in directory:", directory, "from path:", program_path)
        try:
            module = __import__(name, globals(), locals(), ["Program"], 0)
        except Exception as e:
            self.fatal_program_error("Failed to import module: " + str(e))
            return
        try:
            program = module.Program()
        except Exception as e:
            self.fatal_program_error("Failed to initialize Program: " + str(e))
            del module
            del sys.modules[name]
            return
        self.program_stack.append({"directory": directory, "name": name, "module": module, "program": program})
        del module
        started_successfully = True
        try:
            started_successfully = program.start(directory, self.LCD, self)
        except Exception as e:
            self._fatal_os_error("Error starting program: " + str(e))
        if started_successfully == False:
            self.fatal_program_error("Failed to start program")
            self.stop_active_program()
    
    def stop_active_program(self):
        if not self.is_active():
            return
        active_program = self.program_stack.pop()
        try:
            active_program["program"].stop()
        except Exception as e:
            self._fatal_os_error("Error stopping program: " + str(e))
        del sys.modules[active_program["name"]]
        del active_program
        gc.collect()
    
    def set_backlight_strength(self, normalized_value):
        self.backlight_strength = clamp(normalized_value, 0, 1)
        self.backlight_pwm.duty_u16(int(65535 * self.backlight_strength))
    
    def _update_input(self):
        for name, pin_pair in self.inputs["pin_mapping"].items():
            self.inputs["pins"]["cathode"][pin_pair[0]].on()
            if self.inputs["pins"]["anode"][pin_pair[1]].value() == 1:
                self.inputs["values"][name] += 1
            else:
                self.inputs["values"][name] = 0
            self.inputs["pins"]["cathode"][pin_pair[0]].off()
        if self.start_hold_time == None:
            if self.inputs["values"]["start"] > 0:
                self.start_hold_time = time.ticks_ms()
        elif not self.inputs["values"]["start"] > 0:
            if time.ticks_ms() > (self.start_hold_time + 1000):
                active_program = self.program_stack[-1]
                if active_program["directory"] == "/" and (active_program["name"] == "settings" or active_program["name"] == "debugger"):
                    self.stop_active_program()
                else:
                    if active_program["program"].debugging:
                        self.start_builtin_program("debugger")
                    else:
                        self.start_builtin_program("settings")
            self.start_hold_time = None
    
    def _update_program(self):
        if self.is_active():
            active_program = self.program_stack[-1]
            keep_alive = False
            try:
                keep_alive = active_program["program"].update(self.inputs["values"])
            except Exception as e:
                self.fatal_program_error(str(e))
                keep_alive = False
                if len(self.program_stack) == 1:
                    raise
            if self.start_hold_time != None and time.ticks_ms() > (self.start_hold_time + 5000):
                print("Start hold time exceeded. Closing program")
                keep_alive = False
            if not keep_alive:
                if self.start_hold_time != None:
                    self.start_hold_time = time.ticks_ms()
                self.stop_active_program()
        else:
            self.LCD.fill(self.LCD.BLACK)
            self.LCD.centered_text("No program active...", int(self.LCD.width / 2), 16, self.LCD.WHITE)
    
    def _update_display(self):
        self.LCD.show()
    
    def update(self):
        try:
            self._update_input()
        except Exception as e:
            self._fatal_os_error("Error updating input: " + str(e))
        try:
            gc.collect()
        except Exception as e:
            self._fatal_os_error("Garbage collection failed: " + str(e))
        try:
            self._update_program()
        except Exception as e:
            self._fatal_os_error("Error updating program: " + str(e))
        try:
            self._update_display()
        except Exception as e:
            self._fatal_os_error("Error updating display: " + str(e))
    
    def get_program_count(self):
        return len(self.program_stack)
    
    def is_active(self):
        return self.get_program_count() > 0
