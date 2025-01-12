#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>

struct gamepad_data {
    struct i2c_client *client;
    struct input_dev *input;
    struct delayed_work work;
    u16 buttons;
    u16 prev_buttons;
    u8 joy1_x;
    u8 joy1_y;
    u8 joy2_x;
    u8 joy2_y;
    unsigned int poll_interval;
    int axis_min;
    int axis_max;
    int axis_fuzz;
    int axis_flat;
};

static int gamepad_read(struct i2c_client *client, u8 *data, int len)
{
    static struct i2c_msg msg = {
        .flags = I2C_M_RD,
    };

    msg.addr = client->addr;
    msg.len = len;
    msg.buf = data;

    return i2c_transfer(client->adapter, &msg, 1);
}

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
    data->buttons = (buf[1] << 8) | buf[0];
    data->joy1_x = buf[2];
    data->joy1_y = buf[3];
    data->joy2_x = buf[4];
    data->joy2_y = buf[5];
}

static void gamepad_work(struct work_struct *work)
{
    struct gamepad_data *data = container_of(work, struct gamepad_data, work.work);
    struct input_dev *input = data->input;
    u8 buf[6];
    u16 changed;

    if (likely(gamepad_read(data->client, buf, sizeof(buf)) >= 0)) {
        process_gamepad_data(data, buf);

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
    }

    schedule_delayed_work(&data->work, msecs_to_jiffies(data->poll_interval));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5,14,0)
static int gamepad_probe(struct i2c_client *client, const struct i2c_device_id *id)
#else
static int gamepad_probe(struct i2c_client *client)
#endif
{
    struct gamepad_data *data;
    struct input_dev *input;
    int error;
    u32 props_poll_ms = 10;
    u32 props_min = 0;
    u32 props_max = 255;
    u32 props_fuzz = 0;
    u32 props_flat = 0;

    dev_info(&client->dev, "Probing topper3-gamepad at address 0x%02x\n", client->addr);

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dev_err(&client->dev, "I2C functionality not supported\n");
        return -ENXIO;
    }

    data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
    input = devm_input_allocate_device(&client->dev);
    if (!data || !input) {
        dev_err(&client->dev, "Failed to allocate memory\n");
        return -ENOMEM;
    }

    dev_info(&client->dev, "Memory allocated successfully\n");

    data->client = client;
    data->input = input;

    of_property_read_u32(client->dev.of_node, "poll-interval-ms", &props_poll_ms);
    of_property_read_u32(client->dev.of_node, "axis-minimum", &props_min);
    of_property_read_u32(client->dev.of_node, "axis-maximum", &props_max);
    of_property_read_u32(client->dev.of_node, "axis-fuzz", &props_fuzz);
    of_property_read_u32(client->dev.of_node, "axis-flat", &props_flat);

    data->poll_interval = props_poll_ms;
    data->axis_min = props_min;
    data->axis_max = props_max;
    data->axis_fuzz = props_fuzz;
    data->axis_flat = props_flat;

    INIT_DELAYED_WORK(&data->work, gamepad_work);

    input->name = "PS3 Controller";
    input->id.bustype = BUS_USB;
    input->id.vendor = 0x054c;
    input->id.product = 0x0268;
    input->id.version = 0x0110;

    __set_bit(EV_KEY, input->evbit);
    __set_bit(EV_ABS, input->evbit);

    {
        int i;
        for (i = 0; i < 16; i++)
            __set_bit(BTN_TRIGGER_HAPPY1 + i, input->keybit);
    }

    {
        const unsigned int axes[] = { ABS_X, ABS_Y, ABS_RX, ABS_RY };
        size_t i;
        for (i = 0; i < ARRAY_SIZE(axes); i++) {
            input_set_abs_params(input, axes[i],
                               data->axis_min, data->axis_max,
                               data->axis_fuzz, data->axis_flat);
        }
    }

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

module_i2c_driver(gamepad_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("othermod");
MODULE_DESCRIPTION("DPI Topper 3 Gamepad Driver for Raspberry Pi");
MODULE_VERSION("1.0");
MODULE_ALIAS("i2c:topper3-gamepad");
