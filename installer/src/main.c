/*
 * BO2 Mod Menu Installer - PS5 Native App
 *
 * Copies _clientids.gsc from the app bundle to /data/DeadOps/t6_zm/
 * so the DeadOps GSC loader picks it up when Black Ops II starts.
 *
 * Platform: ps5-payload-sdk (prospero-clang)
 * Video: CPU framebuffer via sceVideoOut (1920x1080 tiled RGBA8)
 * Input: DualSense via scePad
 */

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

/* ---- PS5 system function declarations ----------------------------------- */

size_t sceKernelGetDirectMemorySize(void);
int    sceKernelAllocateDirectMemory(int64_t search_start, int64_t search_end,
                                     size_t length, size_t alignment,
                                     int memory_type, int64_t *physical_addr);
int    sceKernelMapDirectMemory(void **addr, size_t length, int protection,
                                int flags, int64_t physical_addr,
                                size_t alignment);
int    sceKernelSendNotificationRequest(int device, void *request,
                                        size_t size, int blocking);
int    sceKernelUsleep(unsigned microseconds);
int    sceKernelCreateEqueue(void **queue, const char *name);
int    sceKernelWaitEqueue(void *queue, void *events, int count,
                           int *out_count, unsigned *timeout);

int    sceSystemServiceHideSplashScreen(void);
int    sceSystemServiceLoadExec(const char *path, const char *argv[]);
int    sceUserServiceInitialize(void *params);
int    sceUserServiceGetInitialUser(int *user_id);

int    sceVideoOutOpen(int user_id, int bus_type, int index, const void *param);
int    sceVideoOutSetFlipRate(int handle, int rate);
int    sceVideoOutAddFlipEvent(void *queue, int handle, void *user_data);
int    sceVideoOutSubmitFlip(int handle, int buffer_index,
                             unsigned flip_mode, int64_t flip_arg);
void   sceVideoOutSetBufferAttribute2(void *attribute, uint64_t pixel_format,
                                      unsigned tiling_mode, unsigned width,
                                      unsigned height, uint64_t option,
                                      unsigned dcc_control,
                                      uint64_t dcc_clear_color);
int    sceVideoOutRegisterBuffers2(int handle, int set_index,
                                   int buffer_index_start, void *buffers,
                                   int buffer_count, void *attribute,
                                   int category, void *option);

int    scePadInit(void);
int    scePadOpen(int user_id, int type, int index, void *param);
int    scePadGetHandle(int user_id, int type, int index);
int    scePadReadState(int handle, void *data);

/* ---- constants ---------------------------------------------------------- */

#define FRAME_W          1920
#define FRAME_H          1080
#define FRAME_BYTES      0x1000000
#define MEM_ALIGN        0x200000
#define MEM_TYPE_GARLIC  3
#define MAP_PROT         0x33
#define PIX_FMT_RGBA8    UINT64_C(0x8000000022000000)
#define FLIP_VSYNC       1

#define PAD_UP           0x00000010
#define PAD_RIGHT        0x00000020
#define PAD_DOWN         0x00000040
#define PAD_LEFT         0x00000080
#define PAD_L1           0x00000400
#define PAD_R1           0x00000800
#define PAD_TRIANGLE     0x00001000
#define PAD_CIRCLE       0x00002000
#define PAD_CROSS        0x00004000
#define PAD_SQUARE       0x00008000
#define PAD_INTERCEPTED  0x80000000

#define GSC_SOURCE       "/app0/assets/_clientids.gsc"
#define DEADOPS_DIR      "/data/DeadOps"
#define T6_ZM_DIR        "/data/DeadOps/t6_zm"
#define GSC_DEST         "/data/DeadOps/t6_zm/_clientids.gsc"

/* DeadOps-PS5 payload: compiles the script and hooks the PS5 zombies build
   from outside the game, sent to the ELF loader like any other payload. */
#define PAYLOAD_SOURCE   "/app0/assets/deadops-ps5.elf"
#define ELFLDR_PORT      9021

/* Left behind by earlier versions that loaded the PS4 DeadOps.sprx through
   ploader; removed on install and uninstall. */
#define HOMEBREW_DIR     "/data/homebrew"
#define GAME_TITLE_ID    "PPSA34502"
#define OLD_SPRX_DEST    "/data/DeadOps/DeadOps.sprx"
#define PLOADER_TOML     "/data/plugins/ploader.toml"
#define TOML_BEGIN       "# BEGIN BO2-DeadOps (managed by BO2 Mod Installer)"
#define TOML_END         "# END BO2-DeadOps"

static const char toml_header[] =
    "[loader]\n"
    "timeout_s = 300\n"
    "scan_existing = true\n"
    "default_inject = true\n"
    "default_restore_sandbox = true\n";

/* ABGR colors for the tiled framebuffer */
#define COL_BG           0xFF1A1A2E
#define COL_PANEL        0xFF16213E
#define COL_HIGHLIGHT    0xFF0F3460
#define COL_ACCENT       0xFF5360E9
#define COL_WHITE        0xFFFFFFFF
#define COL_GRAY         0xFFA0A0A0
#define COL_GREEN        0xFF48E878
#define COL_RED          0xFF6060FF
#define COL_YELLOW       0xFF00D4FF
#define COL_BLACK        0xFF000000

/* ---- structures --------------------------------------------------------- */

struct VideoBuffer {
    void *data;
    void *metadata;
    void *reserved0;
    void *reserved1;
};

struct __attribute__((aligned(16))) VideoAttribute {
    uint8_t reserved[80];
};

struct NotificationRequest {
    char reserved[45];
    char message[3075];
};

union PadState {
    struct {
        uint32_t buttons;
        uint8_t  sticks[4];
        uint8_t  analog[2];
        uint16_t padding;
    } data;
    uint8_t storage[512];
};

/* ---- 8x8 bitmap font (public domain font8x8_basic) --------------------- */

static const uint8_t font8x8[128][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* NUL */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* SOH */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* STX */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ETX */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* EOT */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ENQ */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ACK */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* BEL */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* BS  */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* TAB */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* LF  */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* VT  */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* FF  */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* CR  */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* SO  */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* SI  */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* DLE */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* DC1 */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* DC2 */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* DC3 */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* DC4 */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* NAK */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* SYN */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ETB */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* CAN */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* EM  */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* SUB */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* ESC */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* FS  */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* GS  */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* RS  */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* US  */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* SPC */
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, /* !   */
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, /* "   */
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, /* #   */
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, /* $   */
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, /* %   */
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, /* &   */
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, /* '   */
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, /* (   */
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, /* )   */
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, /* *   */
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, /* +   */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, /* ,   */
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, /* -   */
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, /* .   */
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, /* /   */
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, /* 0   */
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, /* 1   */
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, /* 2   */
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, /* 3   */
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, /* 4   */
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, /* 5   */
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, /* 6   */
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, /* 7   */
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, /* 8   */
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, /* 9   */
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, /* :   */
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, /* ;   */
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, /* <   */
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, /* =   */
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, /* >   */
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, /* ?   */
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, /* @   */
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, /* A   */
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, /* B   */
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, /* C   */
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, /* D   */
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, /* E   */
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, /* F   */
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, /* G   */
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, /* H   */
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* I   */
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, /* J   */
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, /* K   */
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, /* L   */
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, /* M   */
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, /* N   */
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, /* O   */
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, /* P   */
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, /* Q   */
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, /* R   */
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, /* S   */
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* T   */
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, /* U   */
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, /* V   */
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, /* W   */
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, /* X   */
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, /* Y   */
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, /* Z   */
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0x00}, /* [   */
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, /* \   */
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0x00}, /* ]   */
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, /* ^   */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, /* _   */
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, /* `   */
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, /* a   */
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, /* b   */
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, /* c   */
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00}, /* d   */
    {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00}, /* e   */
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00}, /* f   */
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, /* g   */
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, /* h   */
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, /* i   */
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, /* j   */
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, /* k   */
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, /* l   */
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, /* m   */
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, /* n   */
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, /* o   */
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, /* p   */
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, /* q   */
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, /* r   */
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, /* s   */
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, /* t   */
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, /* u   */
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, /* v   */
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, /* w   */
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, /* x   */
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, /* y   */
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, /* z   */
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, /* {   */
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, /* |   */
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, /* }   */
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, /* ~   */
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, /* DEL */
};

/* ---- globals ------------------------------------------------------------ */

static int video_handle  = -1;
static int pad_handle    = -1;
static int user_id       = -1;
static void *flip_queue  = NULL;
static uint8_t *framebuf[2];
static struct NotificationRequest notif;

/* ---- tiled framebuffer -------------------------------------------------- */

static inline size_t tiled_offset(unsigned x, unsigned y)
{
    uint32_t off = ((y << 4) & 0x70U)    ^ ((y << 5) & 0xF00U)
                 ^ ((y << 9) & 0x1000U)  ^ ((y << 8) & 0x4000U)
                 ^ ((x << 2) & 0xCU)     ^ ((x << 5) & 0x380U)
                 ^ ((x << 4) & 0x400U)   ^ ((x << 6) & 0x800U)
                 ^ ((x << 9) & 0xA000U);
    uint32_t bpr = (FRAME_W + 127U) >> 7;
    uint32_t blk = (y >> 7) * bpr + (x >> 7);
    return ((size_t)blk << 16) + off;
}

static void put_pixel(uint8_t *buf, unsigned x, unsigned y, uint32_t col)
{
    if (x < FRAME_W && y < FRAME_H)
        *(uint32_t *)(buf + tiled_offset(x, y)) = col;
}

static void fill_screen(uint8_t *buf, uint32_t col)
{
    for (unsigned y = 0; y < FRAME_H; y++)
        for (unsigned x = 0; x < FRAME_W; x++)
            put_pixel(buf, x, y, col);
}

static void fill_rect(uint8_t *buf, unsigned rx, unsigned ry,
                       unsigned rw, unsigned rh, uint32_t col)
{
    unsigned ex = rx + rw, ey = ry + rh;
    if (ex > FRAME_W) ex = FRAME_W;
    if (ey > FRAME_H) ey = FRAME_H;
    for (unsigned y = ry; y < ey; y++)
        for (unsigned x = rx; x < ex; x++)
            put_pixel(buf, x, y, col);
}

static void draw_char(uint8_t *buf, unsigned cx, unsigned cy,
                       char c, uint32_t col, unsigned scale)
{
    unsigned idx = (unsigned char)c;
    if (idx > 127) idx = '?';
    const uint8_t *glyph = font8x8[idx];
    for (unsigned row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (unsigned col_bit = 0; col_bit < 8; col_bit++) {
            if (bits & (1 << col_bit)) {
                for (unsigned sy = 0; sy < scale; sy++)
                    for (unsigned sx = 0; sx < scale; sx++)
                        put_pixel(buf, cx + col_bit * scale + sx,
                                  cy + row * scale + sy, col);
            }
        }
    }
}

static void draw_text(uint8_t *buf, unsigned tx, unsigned ty,
                       const char *str, uint32_t col, unsigned scale)
{
    while (*str) {
        if (*str == '\n') {
            ty += 8 * scale + 4;
            (void)tx;
        } else {
            draw_char(buf, tx, ty, *str, col, scale);
            tx += 8 * scale;
        }
        str++;
    }
}

static unsigned text_width(const char *str, unsigned scale)
{
    unsigned w = 0;
    while (*str) { w += 8 * scale; str++; }
    return w;
}

static void draw_text_centered(uint8_t *buf, unsigned cy,
                                const char *str, uint32_t col, unsigned scale)
{
    unsigned w = text_width(str, scale);
    unsigned tx = (FRAME_W > w) ? (FRAME_W - w) / 2 : 0;
    draw_text(buf, tx, cy, str, col, scale);
}

/* ---- PS5 platform init -------------------------------------------------- */

static void notify_msg(const char *msg)
{
    memset(&notif, 0, sizeof(notif));
    size_t i = 0;
    for (; msg[i] && i + 1 < sizeof(notif.message); i++)
        notif.message[i] = msg[i];
    notif.message[i] = '\0';
    sceKernelSendNotificationRequest(0, &notif, sizeof(notif), 0);
}

static void __attribute__((noreturn)) halt(const char *msg)
{
    notify_msg(msg);
    for (;;) sceKernelUsleep(1000000);
}

static bool open_video(void)
{
    video_handle = sceVideoOutOpen(0xFF, 0, 0, NULL);
    if (video_handle < 0) return false;

    size_t pool = sceKernelGetDirectMemorySize();
    size_t bytes = (size_t)FRAME_BYTES * 2;
    int64_t phys = 0;
    if (pool < bytes) return false;
    if (sceKernelAllocateDirectMemory(0, (int64_t)pool, bytes,
                                      MEM_ALIGN, MEM_TYPE_GARLIC, &phys) < 0)
        return false;
    void *mapped = NULL;
    if (sceKernelMapDirectMemory(&mapped, bytes, MAP_PROT, 0,
                                 phys, MEM_ALIGN) < 0)
        return false;

    framebuf[0] = (uint8_t *)mapped;
    framebuf[1] = framebuf[0] + FRAME_BYTES;
    fill_screen(framebuf[0], COL_BG);
    fill_screen(framebuf[1], COL_BG);

    struct VideoBuffer vbufs[2] = {
        {framebuf[0], NULL, NULL, NULL},
        {framebuf[1], NULL, NULL, NULL}
    };
    struct VideoAttribute attr;
    memset(&attr, 0, sizeof(attr));
    sceVideoOutSetFlipRate(video_handle, 0);
    sceVideoOutSetBufferAttribute2(&attr, PIX_FMT_RGBA8, 0,
                                   FRAME_W, FRAME_H, 0, 0, 0);
    if (sceVideoOutRegisterBuffers2(video_handle, 0, 0, vbufs, 2,
                                    &attr, 0, NULL) < 0)
        return false;
    if (sceKernelCreateEqueue(&flip_queue, "inst_flip") < 0 ||
        sceVideoOutAddFlipEvent(flip_queue, video_handle, NULL) < 0)
        flip_queue = NULL;
    return true;
}

static void flip(int index, int64_t frame)
{
    sceVideoOutSubmitFlip(video_handle, index, FLIP_VSYNC, frame);
    if (flip_queue) {
        uint8_t __attribute__((aligned(16))) ev[128];
        int count = 0;
        sceKernelWaitEqueue(flip_queue, ev, 1, &count, NULL);
    } else {
        sceKernelUsleep(16000);
    }
}

static uint32_t read_pad(void)
{
    if (pad_handle < 0) return 0;
    union PadState st;
    memset(&st, 0, sizeof(st));
    if (scePadReadState(pad_handle, &st) < 0 || !st.data.sticks[0])
        /* sticks[0] being zero isn't a reliable "not connected" check,
           but we check the connected field which overlaps storage[48] */
        ;
    if (st.data.buttons & PAD_INTERCEPTED) return 0;
    return st.data.buttons;
}

static void __attribute__((noreturn)) exit_app(void)
{
    sceSystemServiceLoadExec("exit", NULL);
    for (int i = 0; i < 100; i++) sceKernelUsleep(100000);
    _exit(0);
}

/* ---- file operations ---------------------------------------------------- */

static int ensure_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) return 0;
    return mkdir(path, 0777);
}

static int game_stat_errno;
static int game_scan_errno;

static bool is_game_dir(const char *dir)
{
    char eboot[512];
    struct stat st;
    snprintf(eboot, sizeof(eboot), "%s/eboot.bin", dir);
    if (stat(eboot, &st) == 0) return true;
    game_stat_errno = errno;
    return false;
}

/* The BO2 folder in /data/homebrew. Known names are checked with stat first;
   anything else is found with the kernel's getdents rather than libc's
   readdir, which did not see the folder on hardware. */
static bool game_sprx_path(char *out, size_t outlen)
{
    static const char *const known[] = {
        HOMEBREW_DIR "/" GAME_TITLE_ID "-app0",
        HOMEBREW_DIR "/" GAME_TITLE_ID,
    };
    game_stat_errno = 0;
    game_scan_errno = 0;
    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        if (is_game_dir(known[i])) {
            snprintf(out, outlen, "%s/DeadOps.sprx", known[i]);
            return true;
        }
    }

    int fd = open(HOMEBREW_DIR, O_RDONLY | O_DIRECTORY);
    if (fd < 0) {
        game_scan_errno = errno;
        return false;
    }
    char __attribute__((aligned(8))) buf[4096];
    bool found = false;
    int n;
    while (!found && (n = getdents(fd, buf, sizeof(buf))) > 0) {
        for (int off = 0; off < n && !found;) {
            const struct dirent *entry = (const struct dirent *)(buf + off);
            if (entry->d_reclen == 0) break;
            off += entry->d_reclen;
            if (strncmp(entry->d_name, GAME_TITLE_ID, strlen(GAME_TITLE_ID)) != 0)
                continue;
            char dir[512];
            snprintf(dir, sizeof(dir), "%s/%s", HOMEBREW_DIR, entry->d_name);
            if (is_game_dir(dir)) {
                snprintf(out, outlen, "%s/DeadOps.sprx", dir);
                found = true;
            }
        }
    }
    if (n < 0) game_scan_errno = errno;
    close(fd);
    return found;
}

/* Whole file into a NUL-terminated heap buffer; caller frees. */
static char *read_file(const char *path, size_t *out_len)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return NULL;
    struct stat st;
    if (fstat(fd, &st) != 0 || st.st_size < 0) { close(fd); return NULL; }
    size_t cap = (size_t)st.st_size;
    char *buf = malloc(cap + 1);
    if (!buf) { close(fd); return NULL; }
    size_t total = 0;
    ssize_t n;
    while (total < cap && (n = read(fd, buf + total, cap - total)) > 0)
        total += (size_t)n;
    close(fd);
    buf[total] = '\0';
    *out_len = total;
    return buf;
}

/* Writes to path.tmp then renames, so a failed write never leaves a half file. */
static int write_file(const char *path, const char *data, size_t len)
{
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    size_t done = 0;
    while (done < len) {
        ssize_t n = write(fd, data + done, len - done);
        if (n <= 0) { close(fd); unlink(tmp); return -1; }
        done += (size_t)n;
    }
    close(fd);
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

static int copy_file(const char *src_path, const char *dst_path,
                     char *errbuf, size_t errlen)
{
    size_t len = 0;
    char *data = read_file(src_path, &len);
    if (!data || len == 0) {
        free(data);
        snprintf(errbuf, errlen, "Cannot read %s", src_path);
        return -1;
    }
    int rc = write_file(dst_path, data, len);
    free(data);
    if (rc != 0) {
        snprintf(errbuf, errlen, "Cannot write %s", dst_path);
        return -1;
    }
    return 0;
}

/* Cuts our marked block out of buf in place. */
static void toml_strip_block(char *buf, size_t *len)
{
    char *begin = strstr(buf, TOML_BEGIN);
    if (!begin) return;
    char *end = strstr(begin, TOML_END);
    if (!end) return;
    end += strlen(TOML_END);
    while (*end == '\n' || *end == '\r') end++;
    size_t tail = (size_t)(buf + *len - end);
    memmove(begin, end, tail + 1);
    *len = (size_t)(begin - buf) + tail;
}

static void toml_remove_entry(void)
{
    size_t len = 0;
    char *existing = read_file(PLOADER_TOML, &len);
    if (!existing) return;
    if (!strstr(existing, TOML_BEGIN)) { free(existing); return; }
    toml_strip_block(existing, &len);

    /* Only our generated header left: the file was ours, remove it. */
    while (len > 0 && (existing[len - 1] == '\n' || existing[len - 1] == '\r'))
        existing[--len] = '\0';
    const size_t header_len = sizeof(toml_header) - 2; /* minus NUL and final \n */
    if (len == 0 ||
        (len == header_len && memcmp(existing, toml_header, len) == 0))
        unlink(PLOADER_TOML);
    else {
        existing[len++] = '\n';
        write_file(PLOADER_TOML, existing, len);
    }
    free(existing);
}

static void remove_legacy_files(void)
{
    char game_sprx[512];
    if (game_sprx_path(game_sprx, sizeof(game_sprx)))
        unlink(game_sprx);
    unlink(OLD_SPRX_DEST);
    toml_remove_entry();
}

static int install_mod(char *errbuf, size_t errlen)
{
    if (ensure_dir(DEADOPS_DIR) != 0) {
        snprintf(errbuf, errlen, "Failed to create %s", DEADOPS_DIR);
        return -1;
    }
    if (ensure_dir(T6_ZM_DIR) != 0) {
        snprintf(errbuf, errlen, "Failed to create %s", T6_ZM_DIR);
        return -1;
    }
    if (copy_file(GSC_SOURCE, GSC_DEST, errbuf, errlen) != 0)
        return -1;
    remove_legacy_files();
    return 0;
}

static int uninstall_mod(void)
{
    unlink(GSC_DEST);
    remove_legacy_files();
    return 0;
}

/* Hands the DeadOps-PS5 payload to the ELF loader listening on the console. */
static int send_payload(char *errbuf, size_t errlen)
{
    size_t len = 0;
    char *elf = read_file(PAYLOAD_SOURCE, &len);
    if (!elf || len == 0) {
        free(elf);
        snprintf(errbuf, errlen, "deadops-ps5.elf is not bundled in the app");
        return -1;
    }

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        free(elf);
        snprintf(errbuf, errlen, "socket() failed (errno %d)", errno);
        return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ELFLDR_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        int e = errno;
        close(s);
        free(elf);
        snprintf(errbuf, errlen, "No ELF loader on port %d (errno %d)",
                 ELFLDR_PORT, e);
        return -1;
    }

    size_t done = 0;
    while (done < len) {
        ssize_t n = write(s, elf + done, len - done);
        if (n <= 0) break;
        done += (size_t)n;
    }
    close(s);
    free(elf);
    if (done != len) {
        snprintf(errbuf, errlen, "Send interrupted (%zu/%zu bytes)", done, len);
        return -1;
    }
    return 0;
}

static bool mod_installed(void)
{
    struct stat st;
    return stat(GSC_DEST, &st) == 0;
}

/* ---- UI rendering ------------------------------------------------------- */

enum {
    ITEM_INSTALL = 0,
    ITEM_LOAD,
    ITEM_UNINSTALL,
    ITEM_ABOUT,
    ITEM_EXIT,
    ITEM_COUNT
};

static const char *menu_labels[ITEM_COUNT] = {
    "Install Mod Menu",
    "Load Mods (send DeadOps)",
    "Uninstall Mod Menu",
    "About",
    "Exit"
};

enum AppState {
    STATE_MENU,
    STATE_RESULT,
    STATE_ABOUT
};

static void render_menu(uint8_t *buf, int selected, bool installed)
{
    fill_screen(buf, COL_BG);

    /* title */
    draw_text_centered(buf, 80, "BO2 ZOMBIES MOD MENU", COL_ACCENT, 5);
    draw_text_centered(buf, 145, "PS5 Installer", COL_GRAY, 3);

    /* status bar */
    const char *status = installed ? "Status: INSTALLED" : "Status: NOT INSTALLED";
    uint32_t status_col = installed ? COL_GREEN : COL_YELLOW;
    draw_text_centered(buf, 210, status, status_col, 3);

    /* menu items */
    unsigned y = 310;
    for (int i = 0; i < ITEM_COUNT; i++) {
        unsigned item_w = 700;
        unsigned item_h = 60;
        unsigned item_x = (FRAME_W - item_w) / 2;

        if (i == selected) {
            fill_rect(buf, item_x, y, item_w, item_h, COL_ACCENT);
            fill_rect(buf, item_x, y, 6, item_h, COL_ACCENT + 0x00404040);
        } else {
            fill_rect(buf, item_x, y, item_w, item_h, COL_PANEL);
        }

        uint32_t label_col = (i == selected) ? COL_WHITE : COL_GRAY;
        unsigned label_w = text_width(menu_labels[i], 3);
        draw_text(buf, (FRAME_W - label_w) / 2, y + 16, menu_labels[i],
                  label_col, 3);
        y += item_h + 16;
    }

    /* footer */
    draw_text_centered(buf, FRAME_H - 80,
                       "[X] Select   [O] Back   [UP/DOWN] Navigate",
                       COL_GRAY, 2);
    draw_text_centered(buf, FRAME_H - 50,
                       "Every session: Load Mods, then start Black Ops II",
                       COL_GRAY, 2);
}

static void render_result(uint8_t *buf, bool success, const char *msg)
{
    fill_screen(buf, COL_BG);

    if (success) {
        draw_text_centered(buf, 300, "SUCCESS", COL_GREEN, 6);
    } else {
        draw_text_centered(buf, 300, "ERROR", COL_RED, 6);
    }
    draw_text_centered(buf, 420, msg, COL_WHITE, 3);
    draw_text_centered(buf, FRAME_H - 80,
                       "Press [O] to return",
                       COL_GRAY, 2);
}

static void render_about(uint8_t *buf)
{
    fill_screen(buf, COL_BG);

    draw_text_centered(buf, 100, "BO2 ZOMBIES MOD MENU", COL_ACCENT, 4);
    draw_text_centered(buf, 160, "PS5 Installer v1.0", COL_WHITE, 3);

    unsigned y = 250;
    unsigned lx = 300;
    uint32_t hc = COL_YELLOW;
    uint32_t tc = COL_GRAY;

    draw_text(buf, lx, y, "Game:", hc, 3);
    draw_text(buf, lx + 200, y, "Call of Duty: Black Ops II", tc, 3);
    y += 50;
    draw_text(buf, lx, y, "Title:", hc, 3);
    draw_text(buf, lx + 200, y, "PPSA34502", tc, 3);
    y += 50;
    draw_text(buf, lx, y, "Mode:", hc, 3);
    draw_text(buf, lx + 200, y, "Zombies (local/offline)", tc, 3);
    y += 50;
    draw_text(buf, lx, y, "Loader:", hc, 3);
    draw_text(buf, lx + 200, y, "DeadOps-PS5 payload", tc, 3);
    y += 80;

    draw_text_centered(buf, y, "Features:", COL_WHITE, 3);
    y += 50;
    draw_text(buf, lx, y, "God Mode, Infinite Ammo, No Clip,", tc, 2);
    y += 30;
    draw_text(buf, lx, y, "Weapon Spawning, All Perks, Teleport,", tc, 2);
    y += 30;
    draw_text(buf, lx, y, "Zombie Control, Point Setting,", tc, 2);
    y += 30;
    draw_text(buf, lx, y, "Open All Doors, and more.", tc, 2);
    y += 60;

    draw_text_centered(buf, y, "In-game: Double-tap Melee to open menu", COL_GREEN, 3);

    draw_text_centered(buf, FRAME_H - 80,
                       "Press [O] to return",
                       COL_GRAY, 2);
}

/* ---- main --------------------------------------------------------------- */

int main(void)
{
    sceUserServiceInitialize(NULL);
    sceUserServiceGetInitialUser(&user_id);

    if (!open_video())
        halt("BO2 Installer: VideoOut setup failed");

    scePadInit();
    pad_handle = scePadOpen(user_id, 0, 0, NULL);
    if (pad_handle < 0)
        pad_handle = scePadGetHandle(user_id, 0, 0);

    sceSystemServiceHideSplashScreen();

    int selected = 0;
    enum AppState state = STATE_MENU;
    bool dirty = true;
    char result_msg[256] = "";
    bool result_ok = false;
    uint32_t prev_buttons = 0;

    int current = 0;
    int64_t frame_num = 0;

    for (;;) {
        uint32_t buttons = read_pad();
        uint32_t pressed = buttons & ~prev_buttons;
        prev_buttons = buttons;

        switch (state) {
        case STATE_MENU:
            if (pressed & PAD_UP) {
                selected = (selected + ITEM_COUNT - 1) % ITEM_COUNT;
                dirty = true;
            }
            if (pressed & PAD_DOWN) {
                selected = (selected + 1) % ITEM_COUNT;
                dirty = true;
            }
            if (pressed & PAD_CROSS) {
                switch (selected) {
                case ITEM_INSTALL: {
                    char err[256] = "";
                    if (install_mod(err, sizeof(err)) == 0) {
                        snprintf(result_msg, sizeof(result_msg),
                                 "Installed! Next: Load Mods, then start Zombies.");
                        result_ok = true;
                        notify_msg("BO2 Mod Menu installed!");
                    } else {
                        snprintf(result_msg, sizeof(result_msg),
                                 "Install failed: %s", err);
                        result_ok = false;
                    }
                    state = STATE_RESULT;
                    dirty = true;
                    break;
                }
                case ITEM_LOAD: {
                    char err[256] = "";
                    if (!mod_installed()) {
                        snprintf(result_msg, sizeof(result_msg),
                                 "Install the mod menu first.");
                        result_ok = false;
                    } else if (send_payload(err, sizeof(err)) == 0) {
                        snprintf(result_msg, sizeof(result_msg),
                                 "DeadOps is waiting. Start BO2 Zombies now.");
                        result_ok = true;
                    } else {
                        snprintf(result_msg, sizeof(result_msg), "%s", err);
                        result_ok = false;
                    }
                    state = STATE_RESULT;
                    dirty = true;
                    break;
                }
                case ITEM_UNINSTALL:
                    if (mod_installed()) {
                        if (uninstall_mod() == 0) {
                            snprintf(result_msg, sizeof(result_msg),
                                     "Mod menu uninstalled.");
                            result_ok = true;
                            notify_msg("BO2 Mod Menu removed.");
                        } else {
                            snprintf(result_msg, sizeof(result_msg),
                                     "Failed to remove file.");
                            result_ok = false;
                        }
                    } else {
                        snprintf(result_msg, sizeof(result_msg),
                                 "Mod menu is not installed.");
                        result_ok = false;
                    }
                    state = STATE_RESULT;
                    dirty = true;
                    break;
                case ITEM_ABOUT:
                    state = STATE_ABOUT;
                    dirty = true;
                    break;
                case ITEM_EXIT:
                    exit_app();
                    break;
                }
            }
            if (pressed & PAD_CIRCLE) {
                exit_app();
            }
            break;

        case STATE_RESULT:
        case STATE_ABOUT:
            if (pressed & (PAD_CIRCLE | PAD_CROSS)) {
                state = STATE_MENU;
                dirty = true;
            }
            break;
        }

        if (dirty) {
            uint8_t *buf = framebuf[current];
            switch (state) {
            case STATE_MENU:
                render_menu(buf, selected, mod_installed());
                break;
            case STATE_RESULT:
                render_result(buf, result_ok, result_msg);
                break;
            case STATE_ABOUT:
                render_about(buf);
                break;
            }
            flip(current, ++frame_num);
            current ^= 1;

            /* draw the same frame on the other buffer too so both match */
            buf = framebuf[current];
            switch (state) {
            case STATE_MENU:
                render_menu(buf, selected, mod_installed());
                break;
            case STATE_RESULT:
                render_result(buf, result_ok, result_msg);
                break;
            case STATE_ABOUT:
                render_about(buf);
                break;
            }
            dirty = false;
        }

        flip(current, ++frame_num);
        current ^= 1;
    }
}
