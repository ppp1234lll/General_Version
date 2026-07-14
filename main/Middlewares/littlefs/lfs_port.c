#include "lfs.h"
#include "lfs_port.h"
#include "bsp.h"

/**
 * lfs与底层flash读数据接口
 * @param  c
 * @param  block  块编号
 * @param  off    块内偏移地址
 * @param  buffer 用于存储读取到的数据
 * @param  size   要读取的字节数
 * @return
 */
static int lfs_deskio_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{    
    sf_ReadBuffer((uint8_t *)buffer, (block*(c->block_size))+off, size);
    return 0;
}

 
/**
 * lfs与底层flash写数据接口
 * @param  c
 * @param  block  块编号
 * @param  off    块内偏移地址
 * @param  buffer 待写入的数据
 * @param  size   待写入数据的大小
 * @return
 */
static int lfs_deskio_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    sf_WriteBuffer((uint8_t *)buffer, (block*(c->block_size))+off, size);
    return 0;
}
 
/**
 * lfs与底层flash擦除接口
 * @param  c
 * @param  block 块编号
 * @return
 */
static int lfs_deskio_erase(const struct lfs_config *c, lfs_block_t block)
{
    sf_EraseSector(block * c->block_size);
    return 0;
}
 
static int lfs_deskio_sync(const struct lfs_config *c)
{
    return 0;
}
 
// configuration of the filesystem is provided by this struct
struct lfs_config cfg =
{
    // block device operations
    .read  = lfs_deskio_read,
    .prog  = lfs_deskio_prog,
    .erase = lfs_deskio_erase,
    .sync  = lfs_deskio_sync,

    // block device configuration
    .read_size = 256,           // 最小读取单元大小
    .prog_size = 256,           // 最小编程单元大小,
    .block_size = 4096,         // 擦除块的大小, 初始化时按实际 Flash 参数更新
    .block_count = 512,         // 块设备的块数量, 初始化时按实际 Flash 参数更新

    .cache_size = 256,
    .lookahead_size = 256,  // lookahead 缓冲区的大小,必须是 8 的倍数
    .block_cycles = 500,    // 每次擦除块的次数,默认值为 500
};

// variables used by the filesystem
lfs_t         g_lfs_t;

// entry point
int lfs_init_function(void) 
{
    SFLASH_T *flash_info = (SFLASH_T *)bsp_GetSFlashInfo();
    lfs_file_t  lfs_file;

    if ((flash_info != NULL) &&
        (flash_info->SectorSize != 0) &&
        (flash_info->TotalSize >= flash_info->SectorSize))
    {
        cfg.block_size  = flash_info->SectorSize;
        cfg.block_count = (flash_info->TotalSize-1024*1024) / flash_info->SectorSize; // 预留1M空间
    }
    else
    {
        cfg.block_size = 4096;
        cfg.block_count = 512;
        return -1;
    }

    /* 挂载设备 */
    int err = lfs_mount(&g_lfs_t, &cfg);

    if (err) 
    {
        err = lfs_format(&g_lfs_t, &cfg);
        err = lfs_mount(&g_lfs_t, &cfg);
        if (err) 
        {
            /* 格式化flash */
            sf_EraseChip();

            err = lfs_format(&g_lfs_t, &cfg);
            err = lfs_mount(&g_lfs_t, &cfg);
        }
  }
    
    // read current count
    uint32_t boot_count = 0;
    lfs_file_open(&g_lfs_t, &lfs_file, "boot_count", LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_read(&g_lfs_t, &lfs_file, &boot_count, sizeof(boot_count));

    // update boot count
    boot_count += 1;

    lfs_file_rewind(&g_lfs_t, &lfs_file);
    lfs_file_write(&g_lfs_t, &lfs_file, &boot_count, sizeof(boot_count));
    
  printf("boot_count: %d\n", boot_count);
    // remember the storage is not updated until the file is closed successfully
    lfs_file_close(&g_lfs_t, &lfs_file);
    return 0;
}



int lfs_test(void)
{
    lfs_t lfs;
    lfs_file_t file;
    lfs_dir_t dir;
    struct lfs_info info;
    int err;
    
    // 创建一个名为test的文件向文件中写入"1234"4个字节数据
    lfs_file_open(&lfs, &file, "test", LFS_O_CREAT | LFS_O_RDWR);
    lfs_file_write(&lfs, &file, "1234", 4);
    lfs_file_close(&lfs, &file);
    
    // 这时虽然打开文件时也使用了LFS_O_CREAT标志但是并不会创建一个新的文件也不会报错，在加入LFS_O_EXCL标志后才会报错
    // LFS_O_RDONLY 标志表示以只读打开文件
    // LFS_O_WRONLY 标志表示以只写打开文件
    // LFS_O_RDWR 标志表示以可读可写打开文件，等价于 LFS_O_RDONLY | LFS_O_WRONLY
    // LFS_O_CREAT 打开文件时如果文件不存在就创建新文件并打开，如果存在将读写指针定位到文件开头打开文件
    // LFS_O_EXCL  打开文件时如果文件不存在就创建新文件并打开，如果存在就报错
    // LFS_O_TRUNC 打开一个已有文件并将文件大小设置为0
    // LFS_O_APPEND 打开一个已有文件并将文件的读写指针设置到文件最后
    lfs_file_open(&lfs, &file, "test", LFS_O_CREAT | LFS_O_RDWR);
    lfs_file_write(&lfs, &file, "abc", 3);
    lfs_file_sync(&lfs, &file); // 这时会见内存中的缓存数据写入到Flash中，这时文件内容为"abc4"

    // LFS_SEEK_SET 用绝对位置设置文件的读写指针（用相对用文件开头的位置设置读写指针）
    // LFS_SEEK_CUR 用相对于当前的位置位置设置读写指针
    // LFS_SEEK_END 用相对用文件末尾的位置设置读写指针
    lfs_file_seek(&lfs, &file, 0, LFS_SEEK_SET); // 文件指针返回到文件开头
    lfs_file_write(&lfs, &file, "1", 1);
    lfs_file_sync(&lfs, &file); // 这时文件内容为"1bc4"

    lfs_file_seek(&lfs, &file, 0, LFS_SEEK_END); // 文件指针设置到文件最后
    lfs_file_write(&lfs, &file, "5", 1);
    lfs_file_sync(&lfs, &file); // 这时文件内容为"1bc45"

    // 文件指针设置到相对于当前位置-2，1bc45| --> 1bc|45
    lfs_file_seek(&lfs, &file, -2, LFS_SEEK_CUR);
    lfs_file_write(&lfs, &file, "d", 1);
    lfs_file_sync(&lfs, &file); // 这时文件内容为"1bcd5"
    lfs_file_close(&lfs, &file);

// 对test文件设置一个时间和一个日期的自定义属性，在删除文件时也会删除
#define FILE_TIME_TYPE 1
#define FILE_DATE_TYPE 2
    lfs_setattr(&lfs, "test", FILE_TIME_TYPE, "12:00:00", 8);
    lfs_setattr(&lfs, "test", FILE_DATE_TYPE, "2023-1-1", 8);

    // 在根目录下创建了一个名为abc的目录
    // 在abc目录下创建了一个名为test的文件，当前有两个test文件一个在根目录一个在abc目录中
    lfs_mkdir(&lfs, "abc");
    lfs_dir_open(&lfs, &dir, "abc");
    lfs_file_open(&lfs, &file, "test",LFS_O_CREAT | LFS_O_RDWR);
    lfs_file_close(&lfs, &file);
    lfs_dir_close(&lfs, &dir);

    // 遍历根目录下的内容，会递归遍历根目录下的目录里的内容
    // 同时每个目录都会遍历到一个"."和一个".."的文件夹表示当前文件夹和返回上一个文件夹的路径
    lfs_dir_open(&lfs, &dir, ".");
    while (1)
    {
        err = lfs_dir_read(&lfs, &dir, &info);
        if (err < 0)
                break;
    }
    lfs_dir_close(&lfs, &dir);

    // release any resources we were using
    err = lfs_unmount(&lfs);
    
    return err;
}














