#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define GAMEPAD_DATA_SIZE 9
#define MAX_READ_RETRIES 3

#define I2C_CMD_BRIGHT 0x10
#define I2C_CMD_CRC 0x20
#define I2C_BRIGHT_DISABLE 8

static u16 crc_table[256];

struct gamepad_data {
    struct i2c_client *client;
    struct input_dev *input;
    struct backlight_device *backlight;
    struct delayed_work work;
    struct mutex lock;
    u16 buttons;
    u16 prev_buttons;
    u8 joy1_x;
    u8 joy1_y;
    u8 joy2_x;
    u8 joy2_y;
    u8 brightness;
    unsigned int poll_interval;
    int axis_min;
    int axis_max;
    int axis_fuzz;
    int axis_flat;
    bool crc_enabled;
};

static void generate_crc_table(void)
{
    const u16 poly = 0x1021;

    for (u16 i = 0; i < 256; i++) {
        u16 crc = i << 8;
        for (u8 j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ poly;
            } else {
                crc = crc << 1;
            }
        }
        crc_table[i] = crc;
    }
}

static u16 calculate_crc(const u8 *data)
{
    u16 crc = 0xFFFF;

    for (int i = 0; i < 7; i++) {
        u8 index = (crc >> 8) ^ data[i];
        crc = (crc << 8) ^ crc_table[index];
    }

    return crc;
}

static bool verify_crc(const u8 *buf)
{
    u16 received_crc = buf[7] | (buf[8] << 8);
    u16 calculated_crc = calculate_crc(buf);

    return received_crc == calculated_crc;
}

static int send_brightness_command(struct i2c_client *client, u8 value)
{
    u8 cmd[2] = { I2C_CMD_BRIGHT, value };
    int ret;

    ret = i2c_master_send(client, cmd, sizeof(cmd));
    if (ret < 0)
        return ret;

    return 0;
}

static int send_crc_command(struct i2c_client *client, bool enable)
{
    u8 cmd[2] = { I2C_CMD_CRC, enable ? 1 : 0 };
    int ret;

    ret = i2c_master_send(client, cmd, sizeof(cmd));
    if (ret < 0)
        return ret;

    return 0;
}

static int gamepad_bl_update_status(struct backlight_device *bl)
{
    struct gamepad_data *data = bl_get_data(bl);
    int brightness = bl->props.brightness;
    int ret;

    mutex_lock(&data->lock);

    if (bl->props.power != FB_BLANK_UNBLANK || bl->props.state & BL_CORE_FBBLANK) {
        ret = send_brightness_command(data->client, I2C_BRIGHT_DISABLE);
    } else {
        ret = send_brightness_command(data->client, brightness);
        if (ret == 0)
            data->brightness = brightness;
    }

    mutex_unlock(&data->lock);

    return ret;
}

static int gamepad_bl_get_brightness(struct backlight_device *bl)
{
    struct gamepad_data *data = bl_get_data(bl);
    return data->brightness;
}

static const struct backlight_ops gamepad_backlight_ops = {
    .update_status = gamepad_bl_update_status,
    .get_brightness = gamepad_bl_get_brightness,
};

static inline void report_button_changes(struct input_dev *input, u16 changed, u16 buttons)
{
    while (changed) {
        int bit = __builtin_ctz(changed);
        input_report_key(input, BTN_TRIGGER_HAPPY1 + bit, (buttons >> bit) & 1);
        changed &= ~(1 << bit);
    }
}

static inline void process_gamepad_data(struct gamepad_data *data, const u8 *buf)
{
    data->buttons = buf[0] | (buf[1] << 8);
    data->joy1_x = buf[2];
    data->joy1_y = buf[3];
    data->joy2_x = buf[4];
    data->joy2_y = buf[5];
}

static void update_brightness_from_device(struct gamepad_data *data, u8 status)
{
    u8 new_brightness = status & 0x07;

    if (new_brightness != data->brightness) {
        data->brightness = new_brightness;
        data->backlight->props.brightness = new_brightness;
    }
}

static void gamepad_work(struct work_struct *work)
{
    struct gamepad_data *data = container_of(work, struct gamepad_data, work.work);
    struct input_dev *input = data->input;
    u8 buf[GAMEPAD_DATA_SIZE];
    u16 changed;
    int ret;
    int retries = 0;

    do {
        ret = i2c_master_recv(data->client, buf, sizeof(buf));
        if (ret < 0) {
            dev_err_ratelimited(&data->client->dev, "I2C read failed: %d\n", ret);
            goto reschedule;
        }

        if (data->crc_enabled && !verify_crc(buf)) {
            retries++;
            if (retries >= MAX_READ_RETRIES) {
                dev_err_ratelimited(&data->client->dev, "CRC verification failed after %d retries\n",
                                    MAX_READ_RETRIES);
                goto reschedule;
            }
            udelay(100);
            continue;
        }

        break;
    } while (retries < MAX_READ_RETRIES);

    process_gamepad_data(data, buf);
    update_brightness_from_device(data, buf[6]);

    changed = data->prev_buttons ^ data->buttons;
    if (changed) {
        report_button_changes(input, changed, data->buttons);
        data->prev_buttons = data->buttons;
    }

    input_report_abs(input, ABS_X, data->joy1_x);
    input_report_abs(input, ABS_Y, data->joy1_y);
    input_report_abs(input, ABS_RX, data->joy2_x);
    input_report_abs(input, ABS_RY, data->joy2_y);

    input_sync(input);

reschedule:
    schedule_delayed_work(&data->work, msecs_to_jiffies(data->poll_interval));
}

static int gamepad_probe(struct i2c_client *client)
{
    struct gamepad_data *data;
    struct input_dev *input;
    struct backlight_device *backlight;
    struct backlight_properties props;
    int error;
    u8 test_buf[GAMEPAD_DATA_SIZE];
    u32 props_poll_ms = 10;
    u32 props_min = 0;
    u32 props_max = 255;
    u32 props_fuzz = 0;
    u32 props_flat = 0;
    bool crc_enable = true;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
        return -ENXIO;

    if (i2c_master_recv(client, test_buf, sizeof(test_buf)) < 0)
        return -ENODEV;

    data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
    input = devm_input_allocate_device(&client->dev);
    if (!data || !input)
        return -ENOMEM;

    data->client = client;
    data->input = input;
    mutex_init(&data->lock);

    of_property_read_u32(client->dev.of_node, "poll-interval-ms", &props_poll_ms);
    of_property_read_u32(client->dev.of_node, "axis-minimum", &props_min);
    of_property_read_u32(client->dev.of_node, "axis-maximum", &props_max);
    of_property_read_u32(client->dev.of_node, "axis-fuzz", &props_fuzz);
    of_property_read_u32(client->dev.of_node, "axis-flat", &props_flat);
    crc_enable = !of_property_read_bool(client->dev.of_node, "disable-crc");

    data->poll_interval = props_poll_ms;
    data->axis_min = props_min;
    data->axis_max = props_max;
    data->axis_fuzz = props_fuzz;
    data->axis_flat = props_flat;
    data->crc_enabled = crc_enable;
    data->brightness = (test_buf[6] & 0x07);

    if (send_crc_command(client, data->crc_enabled) < 0)
        dev_warn(&client->dev, "Failed to configure CRC\n");

    memset(&props, 0, sizeof(props));
    props.type = BACKLIGHT_RAW;
    props.max_brightness = 7;
    props.brightness = data->brightness;
    props.power = FB_BLANK_UNBLANK;

    backlight = devm_backlight_device_register(&client->dev, "topper3-backlight",
                                               &client->dev, data,
                                               &gamepad_backlight_ops, &props);
    if (IS_ERR(backlight))
        return PTR_ERR(backlight);

    data->backlight = backlight;

    INIT_DELAYED_WORK(&data->work, gamepad_work);

    input->name = "PS3 Controller";
    input->id.bustype = BUS_USB;
    input->id.vendor = 0x054c;
    input->id.product = 0x0268;
    input->id.version = 0x0110;

    __set_bit(EV_KEY, input->evbit);
    __set_bit(EV_ABS, input->evbit);

    for (int i = 0; i < 16; i++)
        __set_bit(BTN_TRIGGER_HAPPY1 + i, input->keybit);

    input_set_abs_params(input, ABS_X, data->axis_min, data->axis_max, data->axis_fuzz, data->axis_flat);
    input_set_abs_params(input, ABS_Y, data->axis_min, data->axis_max, data->axis_fuzz, data->axis_flat);
    input_set_abs_params(input, ABS_RX, data->axis_min, data->axis_max, data->axis_fuzz, data->axis_flat);
    input_set_abs_params(input, ABS_RY, data->axis_min, data->axis_max, data->axis_fuzz, data->axis_flat);

    input_set_drvdata(input, data);
    i2c_set_clientdata(client, data);

    error = input_register_device(input);
    if (error)
        return error;

    schedule_delayed_work(&data->work, msecs_to_jiffies(data->poll_interval));

    return 0;
}

static void gamepad_remove(struct i2c_client *client)
{
    struct gamepad_data *data = i2c_get_clientdata(client);
    cancel_delayed_work_sync(&data->work);
}

static const struct i2c_device_id gamepad_id[] = {
    { "topper3-gamepad", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, gamepad_id);

#ifdef CONFIG_OF
static const struct of_device_id gamepad_dt_ids[] = {
    { .compatible = "othermod,topper3-gamepad" },
    { }
};
MODULE_DEVICE_TABLE(of, gamepad_dt_ids);
#endif

static struct i2c_driver gamepad_driver = {
    .driver = {
        .name = "topper3-gamepad",
        .of_match_table = of_match_ptr(gamepad_dt_ids),
    },
    .probe = gamepad_probe,
    .remove = gamepad_remove,
    .id_table = gamepad_id,
};

static int __init gamepad_init(void)
{
    generate_crc_table();
    return i2c_add_driver(&gamepad_driver);
}

static void __exit gamepad_exit(void)
{
    i2c_del_driver(&gamepad_driver);
}

module_init(gamepad_init);
module_exit(gamepad_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("othermod");
MODULE_DESCRIPTION("DPI Topper 3 Gamepad Driver for Raspberry Pi");
MODULE_VERSION("1.2");
MODULE_ALIAS("i2c:topper3-gamepad");
