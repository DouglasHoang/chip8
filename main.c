#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include "raylib.h"

#define CHIP8_WIDTH 64
#define CHIP8_HEIGHT 32

#define SCALE 5

#define BOUNDS_X CHIP8_WIDTH * SCALE
#define BOUNDS_Y CHIP8_HEIGHT * SCALE

#define BUFSIZE 1000000

uint8_t get_hex(uint16_t instruction, size_t digit);
void draw_pixel(int x, int y, Color color);
void draw_font(int x, int y, int letter);
void memory_set(uint8_t *memory, uint8_t *data, size_t m_length, size_t d_length, size_t offset);
void convert_to_big_ed(uint8_t *data, size_t d_length);
void draw_chip_display(uint64_t display[CHIP8_HEIGHT]);

uint8_t font[] = {
    0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
    0x20, 0x60, 0x20, 0x20, 0x70, // 1
    0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
    0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
    0x90, 0x90, 0xF0, 0x10, 0x10, // 4
    0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
    0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
    0xF0, 0x10, 0x20, 0x40, 0x40, // 7
    0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
    0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
    0xF0, 0x90, 0xF0, 0x90, 0x90, // A
    0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
    0xF0, 0x80, 0x80, 0x80, 0xF0, // C
    0xE0, 0x90, 0x90, 0x90, 0xE0, // D
    0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
    0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

uint8_t program[] = {
  0x61, 0x00, 0x71, 0x01, 0x12, 0x02,
};

int keys[] = {
  KEY_ZERO,
  KEY_ONE,
  KEY_TWO,
  KEY_THREE,
  KEY_Q,
  KEY_W,
  KEY_E,
  KEY_A,
  KEY_S,
  KEY_D,
  KEY_Z,
  KEY_C,
  KEY_FOUR,
  KEY_R,
  KEY_F,
  KEY_V,
};

struct Chip8
{
  uint8_t memory[4096];
  uint8_t registers[16];
  uint16_t i_register;
  uint16_t stack[16];
  uint16_t pc;
  size_t sp;

  uint64_t display[CHIP8_HEIGHT];

  uint8_t timer;
  uint8_t sound;
};

int main(void)
{
  srand(time(0));

  struct Chip8 cpu = {
    .pc = 0x200,
    .i_register = 0,
    .sp = 0,
  };

  // Load font
  memory_set(cpu.memory, font, 4096, 80, 80);

  // Load program
  FILE *fd = fopen("ibm.ch8", "r");
  if (fd == NULL) {
    printf("cannot find file\n");
    exit(1);
  }
  uint8_t buf[BUFSIZ];
  size_t buf_length = fread(buf, sizeof(uint8_t), BUFSIZE, fd);
  //convert_to_big_ed(buf, buf_length);
  memory_set(cpu.memory, buf, 4096, buf_length, 512);

  // Set program counter to start of the program
  cpu.pc = 512;

  //memory_set(cpu.memory, program, 4096, 6, 512);

  InitWindow(BOUNDS_X, BOUNDS_Y, "Chip 8");

  BeginDrawing();
  ClearBackground(BLACK);
  EndDrawing();

  size_t count = 0;

  bool should_draw = false;
  while (!WindowShouldClose())
  {
    uint16_t instruction = cpu.memory[cpu.pc] << 8 | cpu.memory[cpu.pc + 1];

    cpu.pc += 2;

    size_t x = get_hex(instruction, 2);
    size_t y = get_hex(instruction, 1);
    uint8_t n = get_hex(instruction, 0); 
    uint8_t nn = instruction & 0xFF;
    uint16_t nnn = instruction & 0xFFF;

    switch (get_hex(instruction, 3))
    {
      case 0x0:
      {
        switch (nn)
        {
        case 0xE0:
        {
          // 00E0 - CLS
          // Clear the display
          for (int i = 0; i < CHIP8_HEIGHT; i++) {
            cpu.display[i] = 0;
          }

          should_draw = true;

          break;
        }
        case 0xEE:
        {
          // 00EE - RET
          // Return from a subroutine.
          // The interpreter sets the program counter to the address at the top of the stack, then subtracts 1 from the stack pointer.
          if (cpu.sp == 0) {
            printf("nothing to pop off stack");
            exit(1);
          } 
          cpu.pc = cpu.stack[cpu.sp - 1];
          cpu.sp--;
          break;
        }
        default:
        {
          // 0nnn - SYS addr
          // Jump to a machine code routine at nnn.
          // This instruction is only used on the old computers on which Chip-8 was originally implemented. It is ignored by modern interpreters.
          
          // Apparently you shouldn't implement this instruction
          // cpu.pc = instruction;
        }
        }
        break;
      }
      case 0x1:
      {
        // 1nnn - JP addr
        // Jump to location nnn.
        // The interpreter sets the program counter to nnn.
        cpu.pc = nnn;
        //printf("Jumping to %04x\n", nnn);
        break;
      }
      case 0x2:
      {
        // 2nnn - CALL addr
        // Call subroutine at nnn.
        // The interpreter increments the stack pointer, then puts the current PC on the top of the stack. The PC is then set to nnn.
        cpu.stack[cpu.sp] = cpu.pc;
        cpu.sp++;
        cpu.pc = nnn;
        break;
      }
      case 0x3:
      {
        // 3xkk - SE Vx, byte
        // Skip next instruction if Vx = kk.
        // The interpreter compares register Vx to kk, and if they are equal, increments the program counter by 2.
        if (cpu.registers[x] == nn)
        {
          cpu.pc += 2;
        };
        break;
      }
      case 0x4:
      {
        // 4xkk - SNE Vx, byte
        // Skip next instruction if Vx != kk.
        // The interpreter compares register Vx to kk, and if they are not equal, increments the program counter by 2.
        if (cpu.registers[x] != nn)
        {
          cpu.pc += 2;
        };
        break;
      }
      case 0x5:
      {
        // 5xy0 - SE Vx, Vy
        // Skip next instruction if Vx = Vy.
        // The interpreter compares register Vx to register Vy, and if they are equal, increments the program counter by 2.
        if (cpu.registers[x] == cpu.registers[y])
        {
          cpu.pc += 2;
        };
        break;
      }
      case 0x6:
      {
        // 6xkk - LD Vx, byte
        // Set Vx = kk.
        // The interpreter puts the value kk into register Vx.
        cpu.registers[x] = nn;
        break;
      }
      case 0x7:
      {
        // 7xkk - ADD Vx, byte
        // Set Vx = Vx + kk.
        // Adds the value kk to the value of register Vx, then stores the result in Vx.
        cpu.registers[x] += nn;
        break;
      }
      case 0x8:
      {
        switch (n)
        {
        case 0x0:
        {
          // 8xy0 - LD Vx, Vy
          // Set Vx = Vy.
          // Stores the value of register Vy in register Vx.
          cpu.registers[x] = cpu.registers[y];
          break;
        }
        case 0x1:
        {
          // 8xy1 - OR Vx, Vy
          // Set Vx = Vx OR Vy.
          // Performs a bitwise OR on the values of Vx and Vy, then stores the result in Vx. A bitwise OR compares the corrseponding bits from two values, and if either bit is 1, then the same bit in the result is also 1. Otherwise, it is 0.
          cpu.registers[x] = cpu.registers[x] | cpu.registers[y];
          break;
        }
        case 0x2:
        {
          // 8xy2 - AND Vx, Vy
          // Set Vx = Vx AND Vy.
          // Performs a bitwise AND on the values of Vx and Vy, then stores the result in Vx. A bitwise AND compares the corrseponding bits from two values, and if both bits are 1, then the same bit in the result is also 1. Otherwise, it is 0.
          cpu.registers[x] = cpu.registers[x] & cpu.registers[y];
          break;
        }
        case 0x3:
        {
          // 8xy3 - XOR Vx, Vy
          // Set Vx = Vx XOR Vy.
          // Performs a bitwise exclusive OR on the values of Vx and Vy, then stores the result in Vx. An exclusive OR compares the corrseponding bits from two values, and if the bits are not both the same, then the corresponding bit in the result is set to 1. Otherwise, it is 0.
          cpu.registers[x] = cpu.registers[x] ^ cpu.registers[y];
          break;
        }
        case 0x4:
        {
          // 8xy4 - ADD Vx, Vy
          // Set Vx = Vx + Vy, set VF = carry.
          // The values of Vx and Vy are added together. If the result is greater than 8 bits (i.e., > 255,) VF is set to 1, otherwise 0. Only the lowest 8 bits of the result are kept, and stored in Vx.
          uint16_t val = cpu.registers[x] + cpu.registers[y];

          if (val > 0xFF)
          {
            cpu.registers[0xF] = 1;
          }
          else
          {
            cpu.registers[0xF] = 0;
          }

          cpu.registers[x] = val;
          break;
        }
        case 0x5:
        {
          // 8xy5 - SUB Vx, Vy
          // Set Vx = Vx - Vy, set VF = NOT borrow.
          // If Vx > Vy, then VF is set to 1, otherwise 0. Then Vy is subtracted from Vx, and the results stored in Vx.
          if (cpu.registers[x] > cpu.registers[y])
          {
            cpu.registers[0xF] = 0;
          }
          else
          {
            cpu.registers[0xF] = 1;
          }
          cpu.registers[x] = cpu.registers[x] - cpu.registers[y];
          break;
        }
        case 0x6:
        {
          // 8xy6 - SHR Vx {, Vy}
          // Set Vx = Vx SHR 1.
          // If the least-significant bit of Vx is 1, then VF is set to 1, otherwise 0. Then Vx is divided by 2.
          cpu.registers[0xF] = cpu.registers[x] & 0x1;
          cpu.registers[x] = cpu.registers[x] >> 1;
          break;
        }
        case 0x7:
        {
          // 8xy7 - SUBN Vx, Vy
          // Set Vx = Vy - Vx, set VF = NOT borrow.
          // If Vy > Vx, then VF is set to 1, otherwise 0. Then Vx is subtracted from Vy, and the results stored in Vx.
          if (cpu.registers[y] > cpu.registers[x])
          {
            cpu.registers[0xF] = 1;
          }
          else
          {
            cpu.registers[0xF] = 0;
          }
          cpu.registers[x] = cpu.registers[y] - cpu.registers[x];
          break;
        }
        case 0xE:
        {
          // 8xyE - SHL Vx {, Vy}
          // Set Vx = Vx SHL 1.
          // If the most-significant bit of Vx is 1, then VF is set to 1, otherwise to 0. Then Vx is multiplied by 2.
          cpu.registers[0xF] = (cpu.registers[x] >> 7) & 0x1;
          cpu.registers[x] = cpu.registers[x] << 1;
          break;
        }
        }
        break;
      }
      case 0x9:
      {
        // 9xy0 - SNE Vx, Vy
        // Skip next instruction if Vx != Vy.
        // The values of Vx and Vy are compared, and if they are not equal, the program counter is increased by 2.
        if (cpu.registers[x] != cpu.registers[y])
        {
          cpu.pc += 2;
        }

        break;
      }
      case 0xA:
      {
        // Annn - LD I, addr
        // Set I = nnn.
        // The value of register I is set to nnn.
        cpu.i_register = nnn;
        break;
      }
      case 0xB:
      {
        // Bnnn - JP V0, addr
        // Jump to location nnn + V0.
        // The program counter is set to nnn plus the value of V0.
        cpu.pc = nnn + cpu.registers[0];
        break;
      }
      case 0xC:
      {
        // Cxkk - RND Vx, byte
        // Set Vx = random byte AND kk.
        // The interpreter generates a random number from 0 to 255, which is then ANDed with the value kk. The results are stored in Vx. See instruction 8xy2 for more information on AND.
        uint8_t random = (rand() % 254);
        cpu.registers[x] = nn & random;
        break;
      }
      case 0xD:
      {
        // Dxyn - DRW Vx, Vy, nibble
        // Display n-byte sprite starting at memory location I at (Vx, Vy), set VF = collision.
        // The interpreter reads n bytes from memory, starting at the address stored in I. These bytes are then displayed as sprites on screen at coordinates (Vx, Vy). Sprites are XORed onto the existing screen. If this causes any pixels to be erased, VF is set to 1, otherwise it is set to 0. If the sprite is positioned so part of it is outside the coordinates of the display, it wraps around to the opposite side of the screen. See instruction 8xy3 for more information on XOR, and section 2.4, Display, for more information on the Chip-8 screen and sprites.
        cpu.registers[0xF] = 0;

        for (size_t i = 0; i < n; i++) {
          uint8_t y_idx = (cpu.registers[y] + i) % 32;
          uint8_t sprite_data = cpu.memory[cpu.i_register + i];

          for (size_t j = 0; j < 8; j++) {
            uint8_t x_idx = (cpu.registers[x] + j) % 64;
            uint8_t sprite_bit = (sprite_data >> (j)) & 0x1;

            //printf("x: %d, y: %d, on: %d\n", x_idx, y_idx, sprite_bit);

            if ((cpu.display[y_idx] >> x_idx & 0x1) == 1 && sprite_bit == 1) {
              cpu.registers[0xF] = 1;
              printf("I am ran in here\n");
            }

            cpu.display[y_idx] = cpu.display[y_idx] ^ (sprite_bit << x_idx);
          }
        }

        should_draw = true;
        
        break;
      }
      case 0xE:
      {
        switch (nn)
        {
          case 0x9E:
          {
            // Ex9E - SKP Vx
            // Skip next instruction if key with the value of Vx is pressed.
            // Checks the keyboard, and if the key corresponding to the value of Vx is currently in the down position, PC is increased by 2.
            if (cpu.registers[x] > 0xF) {
              printf("not a valid key!\n");
              exit(1);
            }

            if (IsKeyDown(keys[cpu.registers[x]])) {
              cpu.pc += 2;
            }

            break;
          }
          case 0xA1:
          {
            // ExA1 - SKNP Vx
            // Skip next instruction if key with the value of Vx is not pressed.
            // Checks the keyboard, and if the key corresponding to the value of Vx is currently in the up position, PC is increased by 2.
            if (cpu.registers[x] > 0xF) {
              printf("not a valid key!\n");
              exit(1);
            }

            if (!IsKeyDown(keys[cpu.registers[x]])) {
              cpu.pc += 2;
            }

            break;
          }
        }
        break;
      }
      case 0xF:
      {
        switch (nn)
        {
        case 0x07:
        {
          // Fx07 - LD Vx, DT
          // Set Vx = delay timer value.
          // The value of DT is placed into Vx.
          cpu.registers[x] = cpu.timer;
          break;
        }
        case 0x0A:
        {
          // Fx0A - LD Vx, K
          // Wait for a key press, store the value of the key in Vx.
          // All execution stops until a key is pressed, then the value of that key is stored in Vx.
          bool key_pressed = false;
          for (size_t i = 0; i < 16; i++) {
            if (IsKeyDown(keys[i])) {
              cpu.registers[x] = i;
              key_pressed = true;
            }
          }

          if (!key_pressed) {
            // Go back an instruction if a key is not pressed
            cpu.pc -= 2;
          }

          break;
        }
        case 0x15:
        {
          // Fx15 - LD DT, Vx
          // Set delay timer = Vx.
          // DT is set equal to the value of Vx.
          cpu.timer = cpu.registers[x];
          break;
        }
        case 0x18:
        {
          // Fx18 - LD ST, Vx
          // Set sound timer = Vx.
          // ST is set equal to the value of Vx.
          cpu.sound = cpu.registers[x];
          break;
        }
        case 0x1E:
        {
          // Fx1E - ADD I, Vx
          // Set I = I + Vx.
          // The values of I and Vx are added, and the results are stored in I.
          cpu.i_register += cpu.registers[x];
          break;
        }
        case 0x29:
        {
          // Fx29 - LD F, Vx
          // Set I = location of sprite for digit Vx.
          // The value of I is set to the location for the hexadecimal sprite corresponding to the value of Vx. See section 2.4, Display, for more information on the Chip-8 hexadecimal font.
          cpu.i_register = cpu.registers[x];
          break;
        }
        case 0x33:
        {
          // Fx33 - LD B, Vx
          // Store BCD representation of Vx in memory locations I, I+1, and I+2.
          // The interpreter takes the decimal value of Vx, and places the hundreds digit in memory at location in I, the tens digit at location I+1, and the ones digit at location I+2.
          cpu.memory[cpu.i_register + 2] = cpu.registers[x] % 10;
          cpu.memory[cpu.i_register + 1] = (cpu.registers[x] / 10) % 10;
          cpu.memory[cpu.i_register] = (cpu.registers[x] / 100) % 10;
          break;
        }
        case 0x55:
        {
          // Fx55 - LD [I], Vx
          // Store registers V0 through Vx in memory starting at location I.
          // The interpreter copies the values of registers V0 through Vx into memory, starting at the address in I.
          printf("not implemented yet");
          break;
        }
        case 0x65:
        {
          // Fx65 - LD Vx, [I]
          // Read registers V0 through Vx from memory starting at location I.
          // The interpreter reads values from memory starting at location I into registers V0 through Vx.
          printf("not implemented yet");
          break;
        }
          printf("F is not implemented");
          break;
        }
      }
    }

    if (cpu.timer == 0) {
      cpu.timer = 0xFF;
    } else {
      cpu.timer--;
    }

    if (cpu.sound == 0) {
      cpu.sound = 0xFF;
    } else {
      cpu.sound--;
    } 

    if (should_draw) {
      printf("drawing!!\n");
      draw_chip_display(cpu.display);
      should_draw = false;
    }

    //draw_chip_display(cpu.display);

    sleep(0.01667);
  }

  CloseWindow();

  return 0;
}

uint8_t get_hex(uint16_t instruction, size_t digit)
{
  return (instruction >> (digit * 4)) & 0xF;
}

void draw_pixel(int x, int y, Color color)
{
  int start_x = x * SCALE;
  int start_y = y * SCALE;

  for (int i = 0; i < SCALE; i++)
  {
    for (int j = 0; j < SCALE; j++)
    {
      int x = start_x + i;
      int y = start_y + j;

      if (x < BOUNDS_X && y < BOUNDS_Y)
      {
        DrawPixel(start_x + i, start_y + j, color);
      }
    }
  }
}

void draw_font(int x, int y, int letter)
{
  if (letter < 0 || letter > 15)
  {
    return;
  }

  size_t start_idx = letter * 5;

  for (int i = 0; i < 5; i++)
  {
    uint8_t byte = font[start_idx + i];
    uint8_t nibble = byte >> 4;
    for (int j = 0; j < 4; j++)
    {
      uint8_t on = (nibble >> (3 - j)) & 0x1;
      if (on)
      {
        draw_pixel(j + x, i + y, RAYWHITE);
      }
    }
  }
}

void memory_set(uint8_t *memory, uint8_t *data, size_t m_length, size_t d_length, size_t offset) {
  for (size_t i = 0; i < d_length; i++) {
    size_t idx = offset + i;

    if (idx >= m_length) {
      return;
    }

    memory[idx] = data[i];
  }
}

void convert_to_big_ed(uint8_t *data, size_t d_length) {
  for (size_t i = 0; i < d_length; i += 2) {
    // swap
    uint8_t s = data[i];
    data[i] = data[i + 1];
    data[i + 1] = s;
  }
}

void draw_chip_display(uint64_t display[CHIP8_HEIGHT]) {
  BeginDrawing();

  ClearBackground(BLACK);

  for (size_t y = 0; y < CHIP8_HEIGHT; y++) {
    for (size_t x = 0; x < CHIP8_WIDTH; x++) {
      uint64_t on = (display[y] >> x) & 0x1;
      if (on) {
        draw_pixel(x, y, RAYWHITE); 
      } else {
        draw_pixel(x, y, BLACK); 
      }
    }
  }

  EndDrawing();
}
