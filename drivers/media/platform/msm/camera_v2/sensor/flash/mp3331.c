/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/export.h>
#include "msm_camera_io_util.h"
#include "msm_led_flash.h"
#ifdef VENDOR_EDIT
/*OPPO 2014-08-01 hufeng add for flash engineer mode test*/
#include <linux/proc_fs.h>
#endif

/*xiongxing@EXP.BaseDrv.Camera, 2016-03-16, add for project 15399*/
#include <soc/oppo/oppo_project.h>
/* Add by TangTao@Camera 20170928 for the second flash resource */
#include "../msm_sensor.h"
#include "../cci/msm_cci.h"
#ifdef VENDOR_EDIT
/* xianglie.liu 2014-09-05 add for add project name */
//#include <mach/oppo_project.h>
/*hufeng 2014-11-03 add foraviod led off twice*/
static struct mutex mp3331_flash_mode_lock;
/*OPPO 2014-08-01 hufeng add for flash engineer mode test*/
struct delayed_work mp3331_led_blink_work;
volatile static bool mp3331_blink_work = false;
bool mp3331_blink_test_status;
/*zhengrong.zhang 2014-11-08 Add for open flash problem in status bar problem when camera opening*/
extern bool camera_power_status;

/*Xiongxing@Prd6.BaseDrv.Camera, 20170221, add for different led*/
#define FLASH_OLD 0
#define FLASH_NEW 1
static int flash_age;

#endif

#define FLASH_NAME "mp3331"

#define CONFIG_MSMB_CAMERA_DEBUG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define mp3331_DBG(fmt, args...) pr_err(fmt, ##args)
#else
#define mp3331_DBG(fmt, args...)
#endif


static struct msm_led_flash_ctrl_t fctrl;
static struct i2c_driver mp3331_i2c_driver;

static struct msm_camera_i2c_reg_array mp3331_init_array[] = {
        {0x01, 0xA0},
        {0x02, 0x0B},
        {0x03, 0xF0},
        {0x04, 0xFF},
        {0x05, 0x14},
};

static struct msm_camera_i2c_reg_array mp3331_off_array[] = {
	{0x01, 0xE0},
};

static struct msm_camera_i2c_reg_array mp3331_release_array[] = {
	{0x01, 0xE0},
};
#ifdef OPPO_CMCC_TEST
/* xianglie.liu add for cmcc */
static struct msm_camera_i2c_reg_array mp3331_low_array[] = {
	{0x01, 0x14},
	{0x0A, 0x02},
};
#else
static struct msm_camera_i2c_reg_array mp3331_low_array[] = {
	{0x01, 0x14},
	{0x0A, 0x03},
};
#endif


static struct msm_camera_i2c_reg_array mp3331_high_array[] = {
	{0x01, 0x16},
	{0x03, 0xA0},
	{0x06, 0x12},
};

static const struct of_device_id mp3331_i2c_trigger_dt_match[] = {
	{.compatible = "mp3331"},
	{}
};

MODULE_DEVICE_TABLE(of, mp3331_i2c_trigger_dt_match);
static const struct i2c_device_id mp3331_i2c_id[] = {
	{FLASH_NAME, (kernel_ulong_t)&fctrl},
	{ }
};

static void msm_mp3331_led_torch_brightness_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	if (value > LED_OFF) {
		if(fctrl.func_tbl->flash_led_low)
			fctrl.func_tbl->flash_led_low(&fctrl);
	} else {
		if(fctrl.func_tbl->flash_led_off)
			fctrl.func_tbl->flash_led_off(&fctrl);
	}
};

static struct led_classdev msm_torch_led = {
	.name			= "torch-light",
	.brightness_set	= msm_mp3331_led_torch_brightness_set,
	.brightness		= LED_OFF,
};

static int32_t msm_mp3331_torch_create_classdev(struct device *dev ,
				void *data)
{
	int rc;
	msm_mp3331_led_torch_brightness_set(&msm_torch_led, LED_OFF);
	rc = led_classdev_register(dev, &msm_torch_led);
	if (rc) {
		pr_err("Failed to register led dev. rc = %d\n", rc);
		return rc;
	}

	return 0;
};

int msm_flash_mp3331_led_init(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	mp3331_DBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);

	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->init_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}
	return rc;
}

int msm_flash_mp3331_led_release(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;
	mp3331_DBG("%s:%d called\n", __func__, __LINE__);
	if (!fctrl) {
		pr_err("%s:%d fctrl NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);
	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->release_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}
	return 0;
}

int msm_flash_mp3331_led_off(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;
	mp3331_DBG("%s:%d called\n", __func__, __LINE__);

	if (!fctrl) {
		pr_err("%s:%d fctrl NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->off_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);

	return rc;
}

int msm_flash_mp3331_led_low(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	mp3331_DBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;


	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_HIGH);

	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->low_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}

	return rc;
}

int msm_flash_mp3331_led_high(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	mp3331_DBG("%s:%d called\n", __func__, __LINE__);

	flashdata = fctrl->flashdata;

	power_info = &flashdata->power_info;

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_HIGH);

	if (fctrl->flash_i2c_client && fctrl->reg_setting) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write_table(
			fctrl->flash_i2c_client,
			fctrl->reg_setting->high_setting);
		if (rc < 0)
			pr_err("%s:%d failed\n", __func__, __LINE__);
	}

	return rc;
}
#ifdef VENDOR_EDIT
/*OPPO 2014-08-01 hufeng add for flash engineer mode test*/
struct regulator *mp3331_vreg;
volatile static int led_test_mode;/*use volatile to insure the value can be updated realtime*/
static int msm_mp3331_led_cci_test_init(void)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	//struct regulator *vreg;
	mp3331_DBG("%s:%d called\n", __func__, __LINE__);
	flashdata = fctrl.flashdata;
	power_info = &flashdata->power_info;

#ifdef VENDOR_EDIT
/* xianglie.liu 2014-09-05 Add for mp3331 14043 and 14045 use gpio-vio */
/* zhengrong.zhang 2014-11-08 Add for gpio contrl mp3331 */
	if (camera_power_status) {
		mp3331_DBG("%s:%d camera already power up\n", __func__, __LINE__);
		return rc;
	}
	msm_flash_led_init(&fctrl);

    /*xiongxing@EXP.BaseDrv.Camera, 2016-03-16, add for 15399*/
    if( is_project(OPPO_15399) ){
        if (power_info->cam_vreg != NULL && power_info->num_vreg>0)
    	{
    		msm_camera_config_single_vreg(&fctrl.pdev->dev,
    			power_info->cam_vreg,
    			&mp3331_vreg,1);
    	}

    	gpio_set_value_cansleep(
		    power_info->gpio_conf->gpio_num_info->
		    gpio_num[SENSOR_GPIO_VIO],
		    GPIO_OUT_HIGH);

    }else{
    	if (power_info->cam_vreg != NULL && power_info->num_vreg>0)
    	{
    		msm_camera_config_single_vreg(&fctrl.pdev->dev,
    			power_info->cam_vreg,
    			&mp3331_vreg,1);
    	}else{
    		gpio_set_value_cansleep(
    			power_info->gpio_conf->gpio_num_info->
    			gpio_num[SENSOR_GPIO_VIO],
    			GPIO_OUT_HIGH);
    	}
    }
#endif
	/*Added by Jinshui.Liu@Camera 20150908 start to delay for hardware to be prepared*/
	msleep(5);
	mp3331_DBG("%s exit\n", __func__);
	return rc;
}
static int msm_mp3331_led_cci_test_off(void)
{
	int rc = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	flashdata = fctrl.flashdata;
	power_info = &flashdata->power_info;
	mp3331_DBG("%s:%d called\n", __func__, __LINE__);
	//if (led_test_mode == 2)
	//	cancel_delayed_work_sync(&mp3331_led_blink_work);
	if (fctrl.flash_i2c_client && fctrl.reg_setting)
	{
		int i = 0;
		//torch current 375.74mA
		rc = fctrl.flash_i2c_client->i2c_func_tbl->i2c_write(
				fctrl.flash_i2c_client, 0x0A,
				0x06, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0)
			pr_err("%s:%d write failed\n", __func__, __LINE__);

		rc = fctrl.flash_i2c_client->i2c_func_tbl->i2c_write(
				fctrl.flash_i2c_client, 0x01,
				0xE0, MSM_CAMERA_I2C_BYTE_DATA);
		if (rc < 0)
			pr_err("%s:%d write failed\n", __func__, __LINE__);

		gpio_set_value_cansleep(
			power_info->gpio_conf->gpio_num_info->
			gpio_num[SENSOR_GPIO_FL_EN],
			GPIO_OUT_LOW);

		gpio_set_value_cansleep(
			power_info->gpio_conf->gpio_num_info->
			gpio_num[SENSOR_GPIO_FL_NOW],
			GPIO_OUT_LOW);

		for (i = 0; i <= 3; i++)
			usleep(750);
	}

#ifdef VENDOR_EDIT
/* xianglie.liu 2014-09-18 Add for mp3331 14043 and 14045 use gpio-vio */
/* zhengrong.zhang 2014-11-08 Add for gpio contrl mp3331 */
	if (camera_power_status) {
		mp3331_DBG("%s:%d camera already power up\n", __func__, __LINE__);
		return rc;
	}

    /*xiongxing@EXP.BaseDrv.Camera, 2016-03-16, add for 15399*/
    if( is_project(OPPO_15399) ){
        if (power_info->cam_vreg != NULL && power_info->num_vreg>0)
    	{
    		msm_camera_config_single_vreg(&fctrl.pdev->dev,
    			power_info->cam_vreg,
    			&mp3331_vreg,0);
    	}

    	gpio_set_value_cansleep(
		    power_info->gpio_conf->gpio_num_info->
		    gpio_num[SENSOR_GPIO_VIO],
		    GPIO_OUT_LOW);

    }else{
    	if (power_info->cam_vreg != NULL && power_info->num_vreg>0)
    	{
    		msm_camera_config_single_vreg(&fctrl.pdev->dev,
    			power_info->cam_vreg,
    			&mp3331_vreg,0);
    	} else {
    		gpio_set_value_cansleep(
    			power_info->gpio_conf->gpio_num_info->
    			gpio_num[SENSOR_GPIO_VIO],
    			GPIO_OUT_LOW);
    	}
    }
	rc = msm_camera_request_gpio_table(
		power_info->gpio_conf->cam_gpio_req_tbl,
		power_info->gpio_conf->cam_gpio_req_tbl_size, 0);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		//return rc;
	}

/* xianglie.liu 2014-09-19 Add for fix ftm mode cannot sleep */
	/* CCI deInit */
	if (fctrl.flash_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = fctrl.flash_i2c_client->i2c_func_tbl->i2c_util(
			fctrl.flash_i2c_client, 1);
		if (rc < 0) {
			pr_err("%s cci_init failed\n", __func__);
		}
	}
#endif
	return rc;
}
static void msm_mp3331_led_cci_test_blink_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	if (mp3331_blink_test_status)
	{
		msm_flash_led_low(&fctrl);
	}
	else
	{
		msm_flash_led_off(&fctrl);
	}
	mp3331_blink_test_status = !mp3331_blink_test_status;
	schedule_delayed_work(dwork, msecs_to_jiffies(1100));
}

static int msm_mp3331_led_cci_test_torch(struct msm_led_flash_ctrl_t *fctrl)
{
	int rc = 0;
       int i = 0;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;

	mp3331_DBG("%s:%d called\n", __func__, __LINE__);
	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;

	if (power_info->gpio_conf->gpio_num_info->
	     valid[SENSOR_GPIO_FL_EN] == 1)
	   gpio_set_value_cansleep(
            power_info->gpio_conf->gpio_num_info->
            gpio_num[SENSOR_GPIO_FL_EN],
            GPIO_OUT_HIGH);

	   gpio_set_value_cansleep(
            power_info->gpio_conf->gpio_num_info->
            gpio_num[SENSOR_GPIO_FL_NOW],
            GPIO_OUT_LOW);

         //torch mode enable, disable hardware pin
         rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write(
           fctrl->flash_i2c_client, 0x01,
           0x14, MSM_CAMERA_I2C_BYTE_DATA);
         if (rc < 0)
            pr_err("%s:%d write failed\n", __func__, __LINE__);

         for (i = 0; i <= 3; i++)
               usleep(750);

         //torch current 190 mA
         rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_write(
           fctrl->flash_i2c_client, 0x0A,
           0x03, MSM_CAMERA_I2C_BYTE_DATA);
         if (rc < 0)
            pr_err("%s:%d write failed\n", __func__, __LINE__);

	return rc;
}

/* zhengrong.zhang 2014-11-08 Add for flash proc read */
static ssize_t mp3331_flash_proc_read(struct file *filp, char __user *buff,
                        	size_t len, loff_t *data)
{
    char value[2] = {0};

    snprintf(value, sizeof(value), "%d", led_test_mode);
    return simple_read_from_buffer(buff, len, data, value,1);
}

static ssize_t mp3331_flash_proc_write(struct file *filp, const char __user *buff,
                        	size_t len, loff_t *data)
{
	char buf[8] = {0};
	int new_mode = 0;
	int i = 0;
	if (len > 8)
		len = 8;
	if (copy_from_user(buf, buff, len)) {
		pr_err("proc write error.\n");
		return -EFAULT;
	}
	new_mode = simple_strtoul(buf, NULL, 10);
	if (new_mode == led_test_mode) {
		pr_err("the same mode as old\n");
		return len;
	}
	/*forbid any operation when camera is running*/
	if (led_test_mode == 5) {
		if (new_mode == 0) {
			mp3331_DBG("%s camera is running, will return, new_mode %d\n", __func__, new_mode);
			return len;
		} else if (new_mode > 0 && new_mode <= 4){
			//mp3331_DBG("%s wait for camera release......\n", __func__);
			while (led_test_mode != 6 && i < 51) {
				i++;
				msleep(30);
			}
			mp3331_DBG("%s wait for camera release done, new_mode %d, i = %d\n", __func__, new_mode, i);
			if (i > 50) {
				led_test_mode = new_mode;
				return len;
			}
		}
	}
	switch (new_mode) {
	case 0:
		mutex_lock(&mp3331_flash_mode_lock);
		if (led_test_mode > 0 && led_test_mode <= 3) {
			msm_mp3331_led_cci_test_off();
			led_test_mode = 0;
		}

		if (mp3331_blink_work) {
			cancel_delayed_work_sync(&mp3331_led_blink_work);
			mp3331_blink_work = false;
		}
		mutex_unlock(&mp3331_flash_mode_lock);
		break;
	case 1:
		mutex_lock(&mp3331_flash_mode_lock);
		msm_mp3331_led_cci_test_init();
		led_test_mode = 1;
		mutex_unlock(&mp3331_flash_mode_lock);
		msm_mp3331_led_cci_test_torch(&fctrl);
		break;
	case 2:
		mutex_lock(&mp3331_flash_mode_lock);
		if (!mp3331_blink_work) {
			msm_mp3331_led_cci_test_init();
			schedule_delayed_work(&mp3331_led_blink_work, msecs_to_jiffies(50));
			mp3331_blink_work = true;
			led_test_mode = 2;
		}
		mutex_unlock(&mp3331_flash_mode_lock);
		break;
	case 3:
		mutex_lock(&mp3331_flash_mode_lock);
		msm_mp3331_led_cci_test_init();
		led_test_mode = 3;
		mutex_unlock(&mp3331_flash_mode_lock);
		msm_flash_led_high(&fctrl);
		break;
	default:
#ifdef VENDOR_EDIT
/*OPPO hufeng 2014-09-02 add for individual flashlight*/
		mutex_lock(&mp3331_flash_mode_lock);
		led_test_mode = new_mode;
		mutex_unlock(&mp3331_flash_mode_lock);
#endif
		pr_err("invalid mode %d\n", led_test_mode);
		break;
	}
	return len;
}
static const struct file_operations led_test_fops = {
    .owner		= THIS_MODULE,
    .read		= mp3331_flash_proc_read,
    .write		= mp3331_flash_proc_write,
};
static int mp3331_flash_proc_init(struct msm_led_flash_ctrl_t *flash_ctl)
{
	int ret=0;
	struct proc_dir_entry *proc_entry;

	INIT_DELAYED_WORK(&mp3331_led_blink_work, msm_mp3331_led_cci_test_blink_work);
	proc_entry = proc_create_data( "qcom_flash", 0666, NULL,&led_test_fops, (void*)&fctrl);
	if (proc_entry == NULL)
	{
		ret = -ENOMEM;
	  	pr_err("[%s]: Error! Couldn't create qcom_flash proc entry\n", __func__);
	}
	return ret;
}
#endif /* VENDOR_EDIT */
static int msm_flash_mp3331_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	int rc = 0 ;
	mp3331_DBG("%s entry\n", __func__);
	if (!id) {
		pr_err("msm_flash_mp3331_i2c_probe: id is NULL");
		id = mp3331_i2c_id;
	}
	rc = msm_flash_i2c_probe(client, id);

	flashdata = fctrl.flashdata;
	power_info = &flashdata->power_info;

	rc = msm_camera_request_gpio_table(
		power_info->gpio_conf->cam_gpio_req_tbl,
		power_info->gpio_conf->cam_gpio_req_tbl_size, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}

	if (fctrl.pinctrl_info.use_pinctrl == true) {
		pr_err("%s:%d PC:: flash pins setting to active state",
				__func__, __LINE__);
		rc = pinctrl_select_state(fctrl.pinctrl_info.pinctrl,
				fctrl.pinctrl_info.gpio_state_active);
		if (rc)
			pr_err("%s:%d cannot set pin to active state",
					__func__, __LINE__);
	}

	if (!rc)
		msm_mp3331_torch_create_classdev(&(client->dev),NULL);
	return rc;
}

static int msm_flash_mp3331_i2c_remove(struct i2c_client *client)
{
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	int rc = 0 ;
	mp3331_DBG("%s entry\n", __func__);
	flashdata = fctrl.flashdata;
	power_info = &flashdata->power_info;

	rc = msm_camera_request_gpio_table(
		power_info->gpio_conf->cam_gpio_req_tbl,
		power_info->gpio_conf->cam_gpio_req_tbl_size, 0);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		return rc;
	}

	if (fctrl.pinctrl_info.use_pinctrl == true) {
		rc = pinctrl_select_state(fctrl.pinctrl_info.pinctrl,
				fctrl.pinctrl_info.gpio_state_suspend);
		if (rc)
			pr_err("%s:%d cannot set pin to suspend state",
				__func__, __LINE__);
	}
	return rc;
}


static struct i2c_driver mp3331_i2c_driver = {
	.id_table = mp3331_i2c_id,
	.probe  = msm_flash_mp3331_i2c_probe,
	.remove = msm_flash_mp3331_i2c_remove,
	.driver = {
		.name = FLASH_NAME,
		.owner = THIS_MODULE,
		.of_match_table = mp3331_i2c_trigger_dt_match,
	},
};
#ifdef VENDOR_EDIT
/*OPPO hufeng 2014-07-24 add for flash cci driver*/
static struct msm_camera_i2c_fn_t msm_sensor_cci_func_tbl = {
	.i2c_read = msm_camera_cci_i2c_read,
	.i2c_read_seq = msm_camera_cci_i2c_read_seq,
	.i2c_write = msm_camera_cci_i2c_write,
	.i2c_write_table = msm_camera_cci_i2c_write_table,
	.i2c_write_seq_table = msm_camera_cci_i2c_write_seq_table,
	.i2c_write_table_w_microdelay =
		msm_camera_cci_i2c_write_table_w_microdelay,
	.i2c_util = msm_sensor_cci_i2c_util,
	.i2c_write_conf_tbl = msm_camera_cci_i2c_write_conf_tbl,
};
extern int32_t msm_led_get_dt_data(struct device_node *of_node,
		struct msm_led_flash_ctrl_t *fctrl);
extern int msm_flash_pinctrl_init(struct msm_led_flash_ctrl_t *ctrl);
static int mp3331_id_match(struct platform_device *pdev,
	const void *data)
{
	int rc = 0;
	int have_read_id_flag = 0;
	struct msm_led_flash_ctrl_t *fctrl =
		(struct msm_led_flash_ctrl_t *)data;
	struct msm_camera_i2c_client *flash_i2c_client= NULL;
	struct msm_camera_slave_info *slave_info;
	const char *sensor_name;
	uint16_t chipid = 0;
	struct device_node *of_node = pdev->dev.of_node;
	struct msm_camera_cci_client *cci_client = NULL;
	struct msm_camera_sensor_board_info *flashdata = NULL;
	struct msm_camera_power_ctrl_t *power_info = NULL;
	struct regulator *vreg;

	if (!of_node) {
		pr_err("of_node NULL\n");
		goto probe_failure;
	}
	fctrl->pdev = pdev;

	rc = msm_led_get_dt_data(pdev->dev.of_node, fctrl);
	if (rc < 0) {
		pr_err("%s failed line %d rc = %d\n", __func__, __LINE__, rc);
		return rc;
	}

	flashdata = fctrl->flashdata;
	power_info = &flashdata->power_info;

    msm_flash_pinctrl_init(fctrl);

	/* Assign name for sub device */
	snprintf(fctrl->msm_sd.sd.name, sizeof(fctrl->msm_sd.sd.name),
			"%s", fctrl->flashdata->sensor_name);

	/* Set device type as Platform*/
	fctrl->flash_device_type = MSM_CAMERA_PLATFORM_DEVICE;

	if (NULL == fctrl->flash_i2c_client) {
		pr_err("%s flash_i2c_client NULL\n",
			__func__);
		rc = -EFAULT;
	}

	fctrl->flash_i2c_client->cci_client = kzalloc(sizeof(
		struct msm_camera_cci_client), GFP_KERNEL);
	if (!fctrl->flash_i2c_client->cci_client) {
		pr_err("%s failed line %d kzalloc failed\n",
			__func__, __LINE__);
		return rc;
	}

	cci_client = fctrl->flash_i2c_client->cci_client;
	cci_client->cci_subdev = msm_cci_get_subdev();
	cci_client->cci_i2c_master = fctrl->cci_i2c_master;
	cci_client->i2c_freq_mode = I2C_FAST_MODE;

	if (fctrl->flashdata->slave_info->sensor_slave_addr)
		cci_client->sid =
			fctrl->flashdata->slave_info->sensor_slave_addr >> 1;
	cci_client->retries = 3;
	cci_client->id_map = 0;

	if (!fctrl->flash_i2c_client->i2c_func_tbl)
		fctrl->flash_i2c_client->i2c_func_tbl =
			&msm_sensor_cci_func_tbl;

	flash_i2c_client = fctrl->flash_i2c_client;
	slave_info = fctrl->flashdata->slave_info;
	sensor_name = fctrl->flashdata->sensor_name;

	if (!flash_i2c_client || !slave_info || !sensor_name) {
		pr_err("%s:%d failed: %p %p %p\n",
			__func__, __LINE__, flash_i2c_client, slave_info,
			sensor_name);
		return -EINVAL;
	}

	msleep(1);

       pr_err("%s:num_vreg:%d \n", __func__,power_info->num_vreg>0);

     msm_flash_led_init(fctrl);
    /*xiongxing@EXP.BaseDrv.Camera, 2016-03-16, add for 15399*/
    if( is_project(OPPO_15399) ){
        if (power_info->cam_vreg != NULL && power_info->num_vreg>0)
    	{
    		msm_camera_config_single_vreg(&fctrl->pdev->dev,
    			power_info->cam_vreg,
    			&vreg,1);
    	}
    	gpio_set_value_cansleep(
		    power_info->gpio_conf->gpio_num_info->
		    gpio_num[SENSOR_GPIO_VIO],
		    GPIO_OUT_HIGH);

    }else{
    	if (power_info->cam_vreg != NULL && power_info->num_vreg>0)
    	{
    		msm_camera_config_single_vreg(&fctrl->pdev->dev,
    			power_info->cam_vreg,
    			&vreg,1);
    	} else {
    		gpio_set_value_cansleep(
    			power_info->gpio_conf->gpio_num_info->
    			gpio_num[SENSOR_GPIO_VIO],
    			GPIO_OUT_HIGH);
    	}
    }
	msleep(3);

	rc = flash_i2c_client->i2c_func_tbl->i2c_read(
		flash_i2c_client, slave_info->sensor_id_reg_addr,
		&chipid,MSM_CAMERA_I2C_BYTE_DATA);

	if (rc < 0)
	{
		have_read_id_flag = 0;
		pr_err("mp3331_id_match i2c_read failed\n");
	}
	else
		have_read_id_flag = 1;
	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_EN],
		GPIO_OUT_LOW);

	gpio_set_value_cansleep(
		power_info->gpio_conf->gpio_num_info->
		gpio_num[SENSOR_GPIO_FL_NOW],
		GPIO_OUT_LOW);
    /*xiongxing@EXP.BaseDrv.Camera, 2016-03-16, add for 15399*/
    if( is_project(OPPO_15399) ){
        if (power_info->cam_vreg != NULL && power_info->num_vreg>0)
    	{
    		msm_camera_config_single_vreg(&fctrl->pdev->dev,
    			power_info->cam_vreg,
    			&vreg,0);
    	}

    	gpio_set_value_cansleep(
		    power_info->gpio_conf->gpio_num_info->
		    gpio_num[SENSOR_GPIO_VIO],
		    GPIO_OUT_LOW);

    }else{
    	if (power_info->cam_vreg != NULL && power_info->num_vreg>0)
    	{
    		msm_camera_config_single_vreg(&fctrl->pdev->dev,
    			power_info->cam_vreg,
    			&vreg,0);
    	} else {
    		gpio_set_value_cansleep(
    			power_info->gpio_conf->gpio_num_info->
    			gpio_num[SENSOR_GPIO_VIO],
    			GPIO_OUT_LOW);
    	}
    }
	rc = msm_camera_request_gpio_table(
		power_info->gpio_conf->cam_gpio_req_tbl,
		power_info->gpio_conf->cam_gpio_req_tbl_size, 0);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
		//return rc;
	}


	if (fctrl->flash_device_type == MSM_CAMERA_PLATFORM_DEVICE) {
		rc = fctrl->flash_i2c_client->i2c_func_tbl->i2c_util(
			fctrl->flash_i2c_client, MSM_CCI_RELEASE);
		if (rc < 0)
			pr_err("cci_deinit failed\n");
	}

	if (0 == have_read_id_flag)
		goto probe_failure;

	pr_err("%s: read id: 0x%x expected id 0x%x:\n", __func__, chipid,
		slave_info->sensor_id);

	chipid &= 0x00FF;

	if (chipid != slave_info->sensor_id) {
		pr_err("MP3331_id_match chip id doesnot match\n");
		goto probe_failure;
	}
	pr_err("%s: probe success\n", __func__);
	rc = msm_led_flash_create_v4lsubdev(pdev, fctrl);
	if(rc < 0)
	{
		pr_err("%s msm_led_flash_create_v4lsubdev failed\n", __func__);
		return rc;
	}

	return 0;

probe_failure:
	pr_err("%s probe failed\n", __func__);
	return -1;
}
static const struct of_device_id mp3331_trigger_dt_match[] =
{
	{.compatible = FLASH_NAME, .data = &fctrl},
	{}
};

#ifdef VENDOR_EDIT
/*Xiongxing@Prd6.BaseDrv.Camera, 20170221, add for different led*/
static int mp3331_request_led_gpio_state(int pin) {
	int state = -1;
	//int err = 0;
	if (gpio_is_valid(pin)) {
		state = gpio_get_value(pin);
	} else {
		pr_err("gpio is invalid\n");
	}

	return state;
}

static ssize_t mp3331_flash_led_age_read(struct file *filp, char __user *buff,
                        	size_t len, loff_t *data) {
    char value[2] = {0};
    snprintf(value, sizeof(value), "%d", flash_age);
    return simple_read_from_buffer(buff, len, data, value, 1);
}

static const struct file_operations led_age_proc = {
    .owner		= THIS_MODULE,
    .read		= mp3331_flash_led_age_read,
};
#endif

static int msm_flash_mp3331_platform_probe(struct platform_device *pdev)
{
	int rc;
	const struct of_device_id *match;
#ifdef VENDOR_EDIT
/*OPPO 2014-11-11 zhengrong.zhang add for torch can't use when open subcamera after boot*/
	struct msm_camera_power_ctrl_t *power_info = NULL;
#endif
	struct proc_dir_entry *proc_entry;

	mp3331_DBG("%s entry\n", __func__);
	match = of_match_device(mp3331_trigger_dt_match, &pdev->dev);
	if (!match)
	{
		pr_err("%s, of_match_device failed!\n", __func__);
		return -EFAULT;
	}
	mp3331_DBG("%s of_match_device success\n", __func__);
#ifdef VENDOR_EDIT
	rc = mp3331_id_match(pdev, match->data);
	if(rc < 0)
		return rc;
#else
	rc = msm_flash_probe(pdev, match->data);
#endif
#ifdef VENDOR_EDIT
/*OPPO hufeng 2014-09-02 add for individual flashlight*/
	mutex_init(&mp3331_flash_mode_lock);
/*OPPO 2014-08-01 hufeng add for flash engineer mode test*/
	mp3331_flash_proc_init(&fctrl);

/*OPPO 2014-11-11 zhengrong.zhang add for torch can't use when open subcamera after boot*/
	power_info = &fctrl.flashdata->power_info;
	rc = msm_camera_request_gpio_table(
    	power_info->gpio_conf->cam_gpio_req_tbl,
    	power_info->gpio_conf->cam_gpio_req_tbl_size, 1);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
	}
	rc = msm_camera_request_gpio_table(
    	power_info->gpio_conf->cam_gpio_req_tbl,
    	power_info->gpio_conf->cam_gpio_req_tbl_size, 0);
	if (rc < 0) {
		pr_err("%s: request gpio failed\n", __func__);
	}
#endif

#ifdef VENDOR_EDIT
/*Xiongxing@Prd6.BaseDrv.Camera, 20170221, add for different led*/
	flash_age = FLASH_OLD;
	//get gpio50 state
	flash_age = mp3331_request_led_gpio_state(952);
	if (flash_age >= 0) {
		proc_entry = proc_create_data( "flash_age", 0666, NULL, &led_age_proc, NULL);
		if (proc_entry == NULL)	{
			rc = -ENOMEM;
			pr_err("[%s]: Error! Couldn't create qcom_flash proc entry\n", __func__);
		}
	}

	pr_err("flash_age %d", flash_age);
#endif
	return rc;
}


static struct platform_driver mp3331_platform_driver =
{
	.probe = msm_flash_mp3331_platform_probe,
	.driver =
	{
		.name = FLASH_NAME,
		.owner = THIS_MODULE,
		.of_match_table = mp3331_trigger_dt_match,
	},
};
#endif
static int __init msm_flash_mp3331_init(void)
{
#ifdef VENDOR_EDIT
/*OPPO hufeng 2014-07-24 add for flash cci probe*/
	int32_t rc = 0;
#endif
	mp3331_DBG("%s entry\n", __func__);
#ifdef VENDOR_EDIT
/*OPPO hufeng 2014-07-24 add for flash cci probe*/
	rc = platform_driver_register(&mp3331_platform_driver);
	mp3331_DBG("%s after entry\n", __func__);
	if (!rc)
		return rc;
	pr_debug("%s:%d rc %d\n", __func__, __LINE__, rc);
#endif
	return i2c_add_driver(&mp3331_i2c_driver);
}

static void __exit msm_flash_mp3331_exit(void)
{
	mp3331_DBG("%s entry\n", __func__);
#ifdef VENDOR_EDIT
/*OPPO hufeng 2014-07-24 add for flash cci probe*/
	platform_driver_unregister(&mp3331_platform_driver);
#endif
	i2c_del_driver(&mp3331_i2c_driver);
	return;
}


static struct msm_camera_i2c_client mp3331_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static struct msm_camera_i2c_reg_setting mp3331_init_setting = {
	.reg_setting = mp3331_init_array,
	.size = ARRAY_SIZE(mp3331_init_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting mp3331_off_setting = {
	.reg_setting = mp3331_off_array,
	.size = ARRAY_SIZE(mp3331_off_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting mp3331_release_setting = {
	.reg_setting = mp3331_release_array,
	.size = ARRAY_SIZE(mp3331_release_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting mp3331_low_setting = {
	.reg_setting = mp3331_low_array,
	.size = ARRAY_SIZE(mp3331_low_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_camera_i2c_reg_setting mp3331_high_setting = {
	.reg_setting = mp3331_high_array,
	.size = ARRAY_SIZE(mp3331_high_array),
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
	.data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.delay = 0,
};

static struct msm_led_flash_reg_t mp3331_regs = {
	.init_setting = &mp3331_init_setting,
	.off_setting = &mp3331_off_setting,
	.low_setting = &mp3331_low_setting,
	.high_setting = &mp3331_high_setting,
	.release_setting = &mp3331_release_setting,
};

static struct msm_flash_fn_t mp3331_func_tbl = {
	.flash_get_subdev_id = msm_led_i2c_trigger_get_subdev_id,
	.flash_led_config = msm_led_i2c_trigger_config,
#ifdef VENDOR_EDIT
/*OPPO 2014-07-24 modify for flash cci driver*/
	.flash_led_init = msm_flash_led_init,
	.flash_led_release = msm_flash_led_release,
	.flash_led_off = msm_flash_led_off,
	.flash_led_low = msm_flash_led_low,
	.flash_led_high = msm_flash_led_high,
#else
	.flash_led_init = msm_flash_mp3331_led_init,
	.flash_led_release = msm_flash_mp3331_led_release,
	.flash_led_off = msm_flash_mp3331_led_off,
	.flash_led_low = msm_flash_mp3331_led_low,
	.flash_led_high = msm_flash_mp3331_led_high,
#endif
};

static struct msm_led_flash_ctrl_t fctrl = {
	.flash_i2c_client = &mp3331_i2c_client,
	.reg_setting = &mp3331_regs,
	.func_tbl = &mp3331_func_tbl,
};

module_init(msm_flash_mp3331_init);
module_exit(msm_flash_mp3331_exit);
MODULE_DESCRIPTION("mp3331 FLASH");
MODULE_LICENSE("GPL v2");
