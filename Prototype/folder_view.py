import gc
import time
import uos

import PICOnsole

class Program(PICOnsole.Program):
    def start(self, root_directory, LCD, console_os):
        super().start(root_directory, LCD, console_os)
        self.program_root = "/sd/programs"
        if not PICOnsole.path.exists(self.program_root):
            print("Root path doesn't exist:", self.program_root)
            uos.mkdir(self.program_root)
        elif not PICOnsole.path.isdir(self.program_root):
            print("Can't make root dir! It already exists as a file:", self.program_root)
            return False
        self.select_dir("/")
        return True
    
    def select_dir(self, new_dir):
        if new_dir == "":
            new_dir = "/"
        self.current_dir = new_dir
        self.selected_item = 0
        full_path = PICOnsole.path.join(self.program_root, self.current_dir)
        if len(new_dir) <= 1:
            self.files = []
        else:
            self.files = [("../", "..")]
        for item in uos.ilistdir(full_path):
            if (item[1] & 0x4000) != 0:
                if PICOnsole.path.isfile(PICOnsole.path.join(PICOnsole.path.join(full_path, item[0]), "__init__.py")):
                    self.files.append(("{" + item[0] + "}", item[0]))
                else:
                    self.files.append((item[0] + "/", item[0]))
            else:
                self.files.append((item[0], item[0]))
            
    
    def update(self, inputs):
        self.LCD.fill(self.LCD.BLACK)
        # Top bar
        self.LCD.fill_rect(0, 0, self.LCD.width, 12, self.LCD.DARK_GREY)
        self.LCD.fill_rect(int(self.LCD.width / 2) - 1, 0, 2, 12, self.LCD.GREY)
        if len(self.current_dir) <= 10:
            self.LCD.text(self.current_dir, 2, 2, self.LCD.WHITE)
        else:
            self.LCD.text("..." + self.current_dir[-7:], 2, 2, self.LCD.WHITE)
        ms_per_top_bar = 4000
        top_bar_mode_count = 3
        top_bar_mode = int((time.ticks_ms() % (ms_per_top_bar * top_bar_mode_count)) / ms_per_top_bar)
        if top_bar_mode == 0:
            sd_stat = uos.statvfs("/sd")
            total_count = sd_stat[1] * sd_stat[2]
            free_count = sd_stat[1] * sd_stat[3]
            used_count = total_count - free_count
            self.LCD.text(" SD:{:4.0%}".format(used_count / total_count), self.LCD.width - (8 * 8 + 2), 2, self.LCD.WHITE)
        elif top_bar_mode == 1:
            internal_stat = uos.statvfs("/")
            total_count = internal_stat[1] * internal_stat[2]
            free_count = internal_stat[1] * internal_stat[3]
            used_count = total_count - free_count
            self.LCD.text("FLS:{:4.0%}".format(used_count / total_count), self.LCD.width - (8 * 8 + 2), 2, self.LCD.WHITE)
        elif top_bar_mode == 2:
            free_count = gc.mem_free()
            used_count = gc.mem_alloc()
            total_count = used_count + free_count
            self.LCD.text("RAM:{:4.0%}".format(used_count / total_count), self.LCD.width - (8 * 8 + 2), 2, self.LCD.WHITE)
        
        # Inputs
        if inputs["up"] == 1:
            self.selected_item -= 1
            if self.selected_item < 0:
                self.selected_item += len(self.files)
        elif inputs["down"] == 1:
            self.selected_item += 1
            if self.selected_item >= len(self.files):
                self.selected_item -= len(self.files)
        elif inputs["a"] == 1:
            selected_path = PICOnsole.path.join(PICOnsole.path.join(self.program_root, self.current_dir), self.files[self.selected_item][1])
            if self.files[self.selected_item][1] == "..":
                self.select_dir(PICOnsole.path.dirname(self.current_dir))
            elif selected_path.endswith(".py") and PICOnsole.path.isfile(selected_path):
                self.console_os.start_program(selected_path)
            elif PICOnsole.path.isdir(selected_path):
                potential_init = PICOnsole.path.join(selected_path, "__init__.py")
                if PICOnsole.path.isfile(potential_init):
                    self.console_os.start_program(selected_path)
                else:
                    self.select_dir(PICOnsole.path.join(self.current_dir, self.files[self.selected_item][1]))
        # File list
        max_item_count = 10
        item_count = min(max_item_count, len(self.files))
        top_index = PICOnsole.clamp(self.selected_item - int(max_item_count / 2), 0, item_count - (max_item_count - 1))
        bottom_index = top_index + item_count
        for i in range(item_count):
            y = 12 * (i + 1)
            item_index = (i + top_index)
            if item_index % 2 == 0:
                self.LCD.fill_rect(0, y, self.LCD.width, 12, self.LCD.DARK_GREY)
            if item_index == self.selected_item:
                self.LCD.rect(0, y, self.LCD.width, 12, self.LCD.GREY)
            if len(self.files[i + top_index][0]) <= 20:
                self.LCD.text(self.files[i + top_index][0], 2, 2 + y, self.LCD.WHITE)
            else:
                self.LCD.text(self.files[i + top_index][0][:17] + "...", 2, 2 + y, self.LCD.WHITE)
        return True

if __name__ == "__main__":
    Program.bootstrap()
