#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <linux/i2c-dev.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>

#define I2C_BUS      "/dev/i2c-1"
#define GSL_ADDR     0x40

#define REG_DATA     0x80
#define REG_RESET    0xE0
#define REG_CLOCK    0xE4
#define REG_POWER    0xBC
#define REG_STATUS   0xB0
#define REG_ID       0xFC

#define STATUS_OK    0x5A5A5A5AUL
#define MAX_FINGERS  10
#define DATA_LEN     44

#define SCREEN_MAX_X 800
#define SCREEN_MAX_Y 480

/* ---- jitter filter config ---- */
#define DEAD_ZONE   8    /* if movement from previous frame is within this
range on both axes, average rather than emit raw */
#define AVG_WINDOW  8    /* number of recent samples to average over */

/* ---- panel offset correction ---- */
#define OFFSET_X    0    /* shift touch frame horizontally; positive = right */
#define OFFSET_Y    0    /* shift touch frame vertically;   positive = down  */

static volatile int running = 1;
static int i2c_fd = -1;
static int ui_fd  = -1;

static void on_sigint(int sig) { (void)sig; running = 0; }

static void msleep(int ms)
{
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* ---- I2C access ---- */

static int gsl_write(uint8_t reg, const uint8_t *data, size_t len)
{
    uint8_t buf[1 + 32];
    buf[0] = reg;
    memcpy(&buf[1], data, len);
    if (write(i2c_fd, buf, len + 1) != (ssize_t)(len + 1)) {
        fprintf(stderr, "write reg 0x%02x failed: %m\n", reg);
        return -1;
    }
    return 0;
}

static int gsl_write_u8(uint8_t reg, uint8_t val) { return gsl_write(reg, &val, 1); }

static int gsl_write_u32(uint8_t reg, uint32_t val)
{
    uint8_t buf[4] = {
        (uint8_t)(val), (uint8_t)(val >> 8),
        (uint8_t)(val >> 16), (uint8_t)(val >> 24)
    };
    return gsl_write(reg, buf, 4);
}

static int gsl_read(uint8_t reg, uint8_t *data, size_t len)
{
    if (write(i2c_fd, &reg, 1) != 1) {
        fprintf(stderr, "set address pointer 0x%02x failed: %m\n", reg);
        return -1;
    }
    if (read(i2c_fd, data, len) != (ssize_t)len) {
        fprintf(stderr, "read reg 0x%02x failed: %m\n", reg);
        return -1;
    }
    return 0;
}

static uint32_t gsl_read_u32(uint8_t reg)
{
    uint8_t buf[4] = {0};
    gsl_read(reg, buf, 4);
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | ((uint32_t)buf[3] << 24);
}

struct fw_entry { uint32_t offset; uint32_t val; };

static int load_firmware(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "fopen %s: %m\n", path); return -1; }

    struct fw_entry entry;
    int count = 0;
    while (fread(&entry, sizeof(entry), 1, f) == 1) {
        if (gsl_write_u32((uint8_t)entry.offset, entry.val) < 0) {
            fprintf(stderr, "firmware load failed at entry %d\n", count);
            fclose(f);
            return -1;
        }
        count++;
    }
    fclose(f);
    printf("loaded %d firmware entries from %s\n", count, path);
    return 0;
}

static int gsl_setup(const char *fw_path)
{
    uint32_t chip_id = gsl_read_u32(REG_ID);
    printf("chip id: 0x%08x\n", chip_id);

    gsl_write_u8(REG_RESET, 0x88); msleep(20);
    gsl_write_u8(REG_DATA, MAX_FINGERS); msleep(20);
    gsl_write_u8(REG_CLOCK, 0x04); msleep(20);
    gsl_write_u8(REG_RESET, 0x00); msleep(20);

    gsl_write_u8(REG_RESET, 0x88); msleep(20);
    gsl_write_u8(REG_CLOCK, 0x04); msleep(20);
    gsl_write_u8(REG_POWER, 0x00); msleep(20);

    if (load_firmware(fw_path) < 0)
        return -1;

    gsl_write_u8(REG_RESET, 0x00);
    msleep(30);

    uint32_t status = gsl_read_u32(REG_STATUS);
    printf("status: 0x%08x (expect 0x%08lx)\n", status, STATUS_OK);
    return status == STATUS_OK ? 0 : -1;
}

/* ---- uinput virtual touchscreen ---- */

static int emit(int fd, int type, int code, int value)
{
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type  = type;
    ev.code  = code;
    ev.value = value;
    return write(fd, &ev, sizeof(ev)) == sizeof(ev) ? 0 : -1;
}

static int uinput_setup_axis(int fd, int code, int min, int max)
{
    struct uinput_abs_setup s;
    memset(&s, 0, sizeof(s));
    s.code            = code;
    s.absinfo.minimum = min;
    s.absinfo.maximum = max;
    return ioctl(fd, UI_ABS_SETUP, &s);
}

static int create_uinput_device(void)
{
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) { fprintf(stderr, "open /dev/uinput: %m\n"); return -1; }

    ioctl(fd, UI_SET_EVBIT,  EV_SYN);
    ioctl(fd, UI_SET_EVBIT,  EV_KEY);
    ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
    ioctl(fd, UI_SET_EVBIT,  EV_ABS);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_SLOT);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
    ioctl(fd, UI_SET_PROPBIT, INPUT_PROP_DIRECT);

    uinput_setup_axis(fd, ABS_MT_SLOT,        0, MAX_FINGERS - 1);
    uinput_setup_axis(fd, ABS_MT_TRACKING_ID, 0, 65535);
    uinput_setup_axis(fd, ABS_MT_POSITION_X,  0, SCREEN_MAX_X - 1);
    uinput_setup_axis(fd, ABS_MT_POSITION_Y,  0, SCREEN_MAX_Y - 1);

    struct uinput_setup usetup;
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_I2C;
    usetup.id.vendor  = 0x1680;
    usetup.id.product = 0x1680;
    strcpy(usetup.name, "GSL1680 Touchscreen");
    ioctl(fd, UI_DEV_SETUP, &usetup);

    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        fprintf(stderr, "UI_DEV_CREATE: %m\n");
        close(fd);
        return -1;
    }
    return fd;
}

/* ---- per-slot jitter filter ---- */

static int buf_x    [MAX_FINGERS][AVG_WINDOW];
static int buf_y    [MAX_FINGERS][AVG_WINDOW];
static int buf_idx  [MAX_FINGERS];
static int buf_count[MAX_FINGERS];
static int prev_x   [MAX_FINGERS];
static int prev_y   [MAX_FINGERS];

static void filter_reset(int slot)
{
    buf_idx[slot]   = 0;
    buf_count[slot] = 0;
}

/*
 * Feed a raw sample into the filter for this slot.
 *
 * The dead zone is compared against the previous frame's raw position,
 * not a static anchor. This means:
 *   - held still: frame-to-frame jitter is small, stays in dead zone,
 *     gets averaged away
 *   - any drag (fast or slow): each frame's movement is compared to the
 *     last frame, so the buffer resets every frame and the exact position
 *     is emitted with no lag or stepping
 *
 * prev_x/prev_y are updated unconditionally every frame so the comparison
 * is always against the most recent raw read.
 */
static void filter_slot(int slot, int x, int y, int *out_x, int *out_y)
{
    int dx = x - prev_x[slot];
    int dy = y - prev_y[slot];

    prev_x[slot] = x;
    prev_y[slot] = y;

    if (dx > DEAD_ZONE || dx < -DEAD_ZONE || dy > DEAD_ZONE || dy < -DEAD_ZONE) {
        /* moved more than dead zone from last frame: emit exact position */
        filter_reset(slot);
        *out_x = x;
        *out_y = y;
    } else {
        /* within dead zone: accumulate and emit rolling average */
        buf_x[slot][buf_idx[slot]] = x;
        buf_y[slot][buf_idx[slot]] = y;
        buf_idx[slot] = (buf_idx[slot] + 1) % AVG_WINDOW;
        if (buf_count[slot] < AVG_WINDOW)
            buf_count[slot]++;

        long sx = 0, sy = 0;
        for (int i = 0; i < buf_count[slot]; i++) {
            sx += buf_x[slot][i];
            sy += buf_y[slot][i];
        }
        *out_x = (int)(sx / buf_count[slot]);
        *out_y = (int)(sy / buf_count[slot]);
    }
}

/* ---- frame processing ---- */

static void process_frame(const uint8_t *buf)
{
    static int slot_active     [MAX_FINGERS] = {0};
    static int next_tracking_id              = 1;
    static int prev_any_touch                = 0;

    int frame_active[MAX_FINGERS] = {0};
    int frame_x     [MAX_FINGERS] = {0};
    int frame_y     [MAX_FINGERS] = {0};

    int touched = buf[0];
    if (touched > MAX_FINGERS)
        touched = MAX_FINGERS;

    for (int i = 0; i < touched; i++) {
        const uint8_t *p = &buf[4 + i * 4];
        if ((p[1] & 0xf0) >> 4)   /* softbutton, not a finger */
            continue;
        int id = (p[3] & 0xf0) >> 4;
        if (id < 1 || id > MAX_FINGERS)
            continue;
        int slot = id - 1;
        frame_active[slot] = 1;
        frame_x[slot] = (p[0] | (p[1] << 8)) & 0xfff;
        frame_y[slot] = (p[2] | (p[3] << 8)) & 0xfff;
    }

    int any_touch = 0;
    for (int slot = 0; slot < MAX_FINGERS; slot++) {
        if (frame_active[slot]) {
            any_touch = 1;
            emit(ui_fd, EV_ABS, ABS_MT_SLOT, slot);

            if (!slot_active[slot]) {
                /* first appearance: seed prev position and reset buffer,
                 * then emit exact position (average of one sample = itself) */
                filter_reset(slot);
                prev_x[slot] = frame_x[slot];
                prev_y[slot] = frame_y[slot];
                emit(ui_fd, EV_ABS, ABS_MT_TRACKING_ID, next_tracking_id++);
            }

            int fx, fy;
            filter_slot(slot, frame_x[slot], frame_y[slot], &fx, &fy);

            fx += OFFSET_X;
            fy += OFFSET_Y;
            if (fx < 0) fx = 0;
            if (fy < 0) fy = 0;
            if (fx > SCREEN_MAX_X - 1) fx = SCREEN_MAX_X - 1;
            if (fy > SCREEN_MAX_Y - 1) fy = SCREEN_MAX_Y - 1;

            emit(ui_fd, EV_ABS, ABS_MT_POSITION_X, fx);
            emit(ui_fd, EV_ABS, ABS_MT_POSITION_Y, fy);

        } else if (slot_active[slot]) {
            /* finger lifted: clear filter state for this slot */
            filter_reset(slot);
            emit(ui_fd, EV_ABS, ABS_MT_SLOT, slot);
            emit(ui_fd, EV_ABS, ABS_MT_TRACKING_ID, -1);
        }

        slot_active[slot] = frame_active[slot];
    }

    if (any_touch != prev_any_touch) {
        emit(ui_fd, EV_KEY, BTN_TOUCH, any_touch);
        prev_any_touch = any_touch;
    }

    emit(ui_fd, EV_SYN, SYN_REPORT, 0);
}

/* ---- main ---- */

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s <firmware.fw>\n", argv[0]);
        return 1;
    }

    signal(SIGINT, on_sigint);

    i2c_fd = open(I2C_BUS, O_RDWR);
    if (i2c_fd < 0) { fprintf(stderr, "open %s: %m\n", I2C_BUS); return 1; }

    if (ioctl(i2c_fd, I2C_SLAVE, GSL_ADDR) < 0) {
        fprintf(stderr, "set slave address 0x%02x: %m\n", GSL_ADDR);
        return 1;
    }

    if (gsl_setup(argv[1]) < 0) {
        fprintf(stderr, "chip did not report OK status, aborting\n");
        return 1;
    }

    ui_fd = create_uinput_device();
    if (ui_fd < 0)
        return 1;

    printf("virtual touchscreen created, ctrl-c to stop\n");
    while (running) {
        uint8_t buf[DATA_LEN];
        if (gsl_read(REG_DATA, buf, DATA_LEN) < 0)
            break;
        process_frame(buf);
        msleep(15);
    }

    ioctl(ui_fd, UI_DEV_DESTROY);
    close(ui_fd);
    close(i2c_fd);
    return 0;
}
