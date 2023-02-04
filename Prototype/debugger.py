import gc
import uos

import PICOnsole

class Program(PICOnsole.Program):
    def update(self, inputs):
        self.LCD.fill(self.LCD.BLACK)
        title_w = len("Debugger") * 8
        center_x = int(self.LCD.width / 2)
        self.LCD.rect(center_x - int(title_w / 2), 6, title_w + 4, 12, self.LCD.WHITE)
        self.LCD.centered_text("Debugger", center_x, 8, self.LCD.WHITE)
        
        
        ram_free = gc.mem_free()
        ram_used = gc.mem_alloc()
        self.LCD.centered_text("RAM", center_x, 20, self.LCD.WHITE)
        self.LCD.text("Used:{:9.2f}kb".format(ram_used / 1024), 12, 30, self.LCD.WHITE)
        self.LCD.text("Free:{:9.2f}kb".format(ram_free / 1024), 12, 40, self.LCD.WHITE)
        
        self.LCD.centered_text("SD Card", center_x, 50, self.LCD.WHITE)
        try:
            sd_stat = uos.statvfs("/sd")
            sd_total = sd_stat[1] * sd_stat[2]
            sd_free = sd_stat[1] * sd_stat[3]
            sd_used = sd_total - sd_free
            sd_used_unit = "kb"
            sd_used /= 1024
            if sd_used > 4000:
                sd_used_unit = "mb"
                sd_used /= 1024
                if sd_used > 4000:
                    sd_used_unit = "gb"
                    sd_used /= 1024
            sd_free_unit = "kb"
            sd_free /= 1024
            if sd_free > 4000:
                sd_free_unit = "mb"
                sd_free /= 1024
                if sd_free > 4000:
                    sd_free_unit = "gb"
                    sd_free /= 1024
            self.LCD.text("Used:{:9.2f}{}".format(sd_used, sd_used_unit), 12, 60, self.LCD.WHITE)
            self.LCD.text("Free:{:9.2f}{}".format(sd_free, sd_free_unit), 12, 70, self.LCD.WHITE)
        except:
            self.LCD.centered_text("No SD Card...", 12, 60, self.LCD.WHITE)
        self.LCD.centered_text("Internal Storage", center_x, 80, self.LCD.WHITE)
        try:
            internal_stat = uos.statvfs("/")
            internal_total = sd_stat[1] * sd_stat[2]
            internal_free = sd_stat[1] * sd_stat[3]
            internal_used = sd_total - sd_free
            internal_used_unit = "kb"
            internal_used /= 1024
            if internal_used > 4000:
                internal_used_unit = "mb"
                internal_used /= 1024
                if internal_used > 4000:
                    internal_used_unit = "gb"
                    internal_used /= 1024
            internal_free_unit = "kb"
            internal_free /= 1024
            if internal_free > 4000:
                internal_free_unit = "mb"
                internal_free /= 1024
                if internal_free > 4000:
                    internal_free_unit = "gb"
                    internal_free /= 1024
            self.LCD.text("Used:{:9.2f}{}".format(internal_used, internal_used_unit), 12, 90, self.LCD.WHITE)
            self.LCD.text("Free:{:9.2f}{}".format(internal_free, internal_free_unit), 12, 100, self.LCD.WHITE)
        except:
            self.LCD.centered_text("Unknown", 12, 90, self.LCD.WHITE)
        return True

if __name__ == "__main__":
    Program.bootstrap()
