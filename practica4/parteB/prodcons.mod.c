#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif


static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x13c49cc2, "_copy_from_user" },
	{ 0x8c8569cb, "kstrtoint" },
	{ 0x6bd0e573, "down_interruptible" },
	{ 0xf23fcb99, "__kfifo_in" },
	{ 0xcf2a6966, "up" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0xa19b956, "__stack_chk_fail" },
	{ 0x13d0adf7, "__kfifo_out" },
	{ 0x656e4a6e, "snprintf" },
	{ 0x6b10bee1, "_copy_to_user" },
	{ 0x139f2189, "__kfifo_alloc" },
	{ 0x7e1fa8eb, "__register_chrdev" },
	{ 0xd2dd7d3, "__class_create" },
	{ 0x975b3f8c, "device_create" },
	{ 0x122c3a7e, "_printk" },
	{ 0xdb760f52, "__kfifo_free" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0x91c216f5, "class_destroy" },
	{ 0x67d81e0e, "device_destroy" },
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x43296262, "module_layout" },
};

MODULE_INFO(depends, "");

