#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <linux/kernel.h>
#include<linux/slab.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/page.h>
#include <linux/uaccess.h>
#include <linux/uidgid.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/namei.h>
#include <linux/crypto.h>

struct myfs_compressor {
	int compr_type;
	struct crypto_comp *cc;
	//struct mutex *comp_mutex;
	// struct mutex *decomp_mutex;
	// const char *name;
	const char *compressor_name;
};
int compress_buffer(char * in_buf, int in_len,char *out_buf, int out_len, int *compressor_type )
{
	int err;
	const char *cmp_alg = "deflate";
	struct crypto_comp tfm;
	struct myfs_compressor *compr = myfs_compressor[*compressor_type];
	if (*compressor_type == NULL)
	{
		printk("\nERROR : No compression type present ");
		goto no_compress;
	}
	tfm = crypto_alloc_comp(cmp_alg,0,0);
    err = crypto_comp_compress(tfm, in_buf, in_len, out_buf,&out_len);
    if (err < 0)
    {
    	printk("\nERROR : Failure in compression");
    	return -1;
    }
    printk("\n the compressed data is : %s ",out_buf);
    return 0;
    
	no_compress:
	memcpy(out_buf,in_buf,in_len);
	*out_len = in_len;
	*compressor_type = NULL;
    
}