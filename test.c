static const u8 *aes_iv = (u8 *)CEPH_AES_IV;

static struct crypto_blkcipher *ceph_crypto_alloc_cipher(void)
{
    return crypto_alloc_blkcipher("cbc(aes)", 0, CRYPTO_ALG_ASYNC);
}

static int sgfs_encryption(const void *key, int key_len, void *dst, size_t *dst_len, const void *src, size_t src_len){
    int ret;
    int ivsize;
    char pad[48];
    size_t zero_padding = (0x10 - (src_len & 0x0f));
    struct scatterlist sg_in[2], sg_out[1];
    struct crypto_blkcipher *tfm = ceph_crypto_alloc_cipher();
    struct blkcipher_desc desc = { .tfm = tfm, .flags = 0 };
    void *iv;
    if (IS_ERR(tfm))
        return PTR_ERR(tfm);

    memset(pad, zero_padding, zero_padding);

    *dst_len = src_len + zero_padding;
    crypto_blkcipher_setkey((void *)tfm, key, key_len);
    sg_init_table(sg_in, 2);
    sg_set_buf(&sg_in[0], src, src_len);
    sg_set_buf(&sg_in[1], pad, zero_padding);
    sg_init_table(sg_out, 1);
    sg_set_buf(sg_out, dst,*dst_len);
    iv = crypto_blkcipher_crt(tfm)->iv;
    ivsize = crypto_blkcipher_ivsize(tfm);
    memcpy(iv, aes_iv, ivsize);
    ret = crypto_blkcipher_encrypt(&desc, sg_out, sg_in,
                                     src_len + zero_padding);
    crypto_free_blkcipher(tfm);
    if (ret < 0)
        printk("Error in key encryption : %d\n", ret);
    return 0;
}

static int sgfs_unlink(struct inode *dir, struct dentry *dentry)
{
    int err, key_len;
    struct dentry *lower_dentry;
    struct inode *lower_dir_inode = sgfs_lower_inode(dir);
    struct dentry *lower_dir_dentry;
    struct path lower_path;
    struct path abs_path;
    char *parent_dir = NULL;
    char *key = NULL;
    char *filename = NULL;
    struct file *f_read;
    struct file *f_write;
    char *buff3 = NULL;
    char *buff4 = NULL;
    char *path = NULL;
    char *key = NULL;
    
    path = kmalloc(sizeof(char)*PATH_MAX, GFP_KERNEL);
    if(!path){
        printk("Insufficient memory\n");
        return -ENOMEM;
    }
    key = (char*)kmalloc(sizeof(char)*20,GFP_KERNEL);
    if(!key){
        printk("Insufficient memory\n");
        return -ENOMEM;
    }
    filename = kmalloc(sizeof(char)*PATH_MAX, GFP_KERNEL);
    if(!filename){
        printk("Insufficient memory\n");
        return -ENOMEM;
    }

    sgfs_get_lower_path(dentry, &abs_path);
    path = d_path(&abs_path,filename,PATH_MAX);
    printk("path is %s\n",path);
    printk("Filename is %s\n",dentry->d_iname);
    parent_dir = (char*)kmalloc(sizeof(char)*strlen(dentry->d_parent->d_iname),GFP_KERNEL);
    strcpy(parent_dir,dentry->d_parent->d_iname);

    strcpy(key,SGFS_SB(dir->i_sb)->enckey);
    key_len = strlen(key);
    printk("Key is %s\n",key);

    if(key_len == 0){

        printk("Hi I am in unlink removing\n");
        printk(" parent d_iname is %s\n", dentry->d_parent->d_iname);
    
        printk(" parent d_iname is %s\n", parent_dir);
    
        if(!strcmp(parent_dir,".sg")){
            printk("I am directly unlinking\n");
            sgfs_get_lower_path(dentry, &lower_path);
            lower_dentry = lower_path.dentry;
            dget(lower_dentry);
            lower_dir_dentry = lock_parent(lower_dentry);
    
            err = vfs_unlink(lower_dir_inode, lower_dentry, NULL);
    
            if (err == -EBUSY && lower_dentry->d_flags & DCACHE_NFSFS_RENAMED)
                err = 0;
            if (err) {
                unlock_dir(lower_dir_dentry);
                dput(lower_dentry);
                sgfs_put_lower_path(dentry, &lower_path);
                printk("Could not unlink the file error is %d\n", err);
                return err;
            }
            fsstack_copy_attr_times(dir, lower_dir_inode);
            fsstack_copy_inode_size(dir, lower_dir_inode);
            set_nlink(d_inode(dentry),
                  sgfs_lower_inode(d_inode(dentry))->i_nlink);
            d_inode(dentry)->i_ctime = dir->i_ctime;
            d_drop(dentry);
            unlock_dir(lower_dir_dentry);
            dput(lower_dentry);
            sgfs_put_lower_path(dentry, &lower_path); /* this is needed, else LTP fails (VFS won't do it) */
            printk("Successfully deleted the file\n");
            return 0;
        }
    
        else{
            printk("I am unlinking after encryption\n");
            // char *buff5 = (char*)kmalloc(sizeof(char)*PAGE_SIZE,GFP_KERNEL);
            key = (char*)kmalloc(sizeof(char)*10,GFP_KERNEL);
            size_t dst_len;
            mm_segment_t oldfs1;
            mm_segment_t oldfs2;
            int bytes1;
            int bytes2;
            int curr_read_size = 0;
            int curr_write_size = 0;
            char *fname = kmalloc(sizeof(char)*PATH_MAX,GFP_KERNEL);
            char *abs_path = kmalloc(sizeof(char)*100,GFP_KERNEL);
            char *year = kmalloc(sizeof(char)*4,GFP_KERNEL);
            char *month = kmalloc(sizeof(char)*2,GFP_KERNEL);
            char *day = kmalloc(sizeof(char)*2,GFP_KERNEL);
            char *hrs = kmalloc(sizeof(char)*2,GFP_KERNEL);
            char *mins = kmalloc(sizeof(char)*2,GFP_KERNEL);
            int c, fsize, temp_size;
            struct inode *Inode;
    
    
    
            struct timeval time;
            struct rtc_time tm;
            unsigned long local_time;
            char uid[5];
    
            uid_t curr_user;
            struct user_struct *us = current_user();
             curr_user = get_uid(us)->uid.val;
             printk("curr id is %d\n",curr_user);
    
            c = snprintf(uid,5,"%04d",curr_user);
            strcat(fname,uid);
            strcat(fname,"-");
            
            do_gettimeofday(&time);
            local_time = (u32)(time.tv_sec - (sys_tz.tz_minuteswest * 60));
            rtc_time_to_tm(local_time, &tm);
    
            c = snprintf(year,5,"%0ld",tm.tm_year + 2016);
            strcat(fname,year);
            strcat(fname,"-");
            c = snprintf(month,3,"%02d",tm.tm_mon);
            strcat(fname,month);
            strcat(fname,"-");
            c = snprintf(day,3,"%02d",tm.tm_mday + 1);
            strcat(fname,day);
            strcat(fname,"-");
            c = snprintf(hrs,3,"%02d",tm.tm_hour);
            strcat(fname,hrs);
            strcat(fname,":");
            c = snprintf(mins,3,"%02d",tm.tm_min);
            strcat(fname,mins);
            strcat(fname,"-");
            strcat(fname,dentry->d_iname);
            printk("filename is %s\n",fname);
    
            
    
            f_read = filp_open(path, O_RDONLY, 0);
            if (!f_read || IS_ERR(f_read)) {
            printk("Reading file 1 is failed\n");
            printk("wrapfs_read_file err %d\n", (int) PTR_ERR(f_read));
            return -(int)PTR_ERR(f_read);  // or do something else
            }
    
            Inode = f_read->f_path.dentry->d_inode;
            fsize = i_size_read(Inode);
            printk("file size is %d\n",fsize);
            strcpy(abs_path,"/usr/src/hw2-vanugu/hw2/sgfs/.sg/");
            strcat(abs_path,fname);
    
            f_write = filp_open(abs_path, O_WRONLY | O_CREAT, 0644);
            if(!f_write || IS_ERR(f_write)){
                printk("Wrapfs_write_file_4 err %d\n", (int)PTR_ERR(f_write));
                return -(int)PTR_ERR(f_write);
            }
    
    
            while(fsize > 0){
    
                buff3 = (char*)kmalloc(sizeof(char)*PAGE_SIZE,GFP_KERNEL);
                buff4 = (char*)kmalloc(sizeof(char)*PAGE_SIZE,GFP_KERNEL);
                if(fsize > PAGE_SIZE){
                    temp_size = PAGE_SIZE;
                    fsize = fsize - temp_size;
                }
                else {
                    temp_size = fsize;
                    fsize = 0;
                }
                f_read->f_pos = curr_read_size;
                oldfs1 = get_fs();
                set_fs(KERNEL_DS);
                bytes1 = vfs_read(f_read, buff3,temp_size, &f_read->f_pos);
                set_fs(oldfs1);
                printk("data is %s\n",buff3);
                printk("Bytes read is %d\n",bytes1);
    
                f_write->f_pos = curr_write_size;
                oldfs2 = get_fs();
                set_fs(KERNEL_DS);
                bytes2 = vfs_write(f_write, buff3, bytes1, &f_write->f_pos);
                set_fs(oldfs2);
                printk("%d\n",bytes2);
    
                printk("Successfully written the file\n");
                kfree(buff3);
                kfree(buff4);
                printk("successfully freed the buffer\n");
    
                curr_read_size += bytes1;
                curr_write_size += bytes2;
    
            }
    
    
            filp_close(f_read,NULL);
            filp_close(f_write,NULL);
            kfree(key);
    
    
            sgfs_get_lower_path(dentry, &lower_path);
            lower_dentry = lower_path.dentry;
            dget(lower_dentry);
            lower_dir_dentry = lock_parent(lower_dentry);
    
            err = vfs_unlink(lower_dir_inode, lower_dentry, NULL);
    
            /*
             * Note: unlinking on top of NFS can cause silly-renamed files.
             * Trying to delete such files results in EBUSY from NFS
             * below.  Silly-renamed files will get deleted by NFS later on, so
             * we just need to detect them here and treat such EBUSY errors as
             * if the upper file was successfully deleted.
             */
            if (err == -EBUSY && lower_dentry->d_flags & DCACHE_NFSFS_RENAMED)
                err = 0;
            if (err){
                unlock_dir(lower_dir_dentry);
                dput(lower_dentry);
                sgfs_put_lower_path(dentry, &lower_path);
                printk("Couldn't unlink the file error is %d", err);
                return err;
            }
            fsstack_copy_attr_times(dir, lower_dir_inode);
            fsstack_copy_inode_size(dir, lower_dir_inode);
            set_nlink(d_inode(dentry),
                  sgfs_lower_inode(d_inode(dentry))->i_nlink);
            d_inode(dentry)->i_ctime = dir->i_ctime;
            d_drop(dentry);
            unlock_dir(lower_dir_dentry);
            dput(lower_dentry);
            sgfs_put_lower_path(dentry, &lower_path); /* this is needed, else LTP fails (VFS won't do it) */
            printk("Successfully moved the file to the recycle bin after encryption\n");
            return 0;
        }
        
    }

    else {
        
        printk("Hi I am in unlink removing\n");
        printk(" parent d_iname is %s\n", dentry->d_parent->d_iname);

        printk(" parent d_iname is %s\n", parent_dir);

        if(!strcmp(parent_dir,".sg")){
            printk("I am directly unlinking\n");
            sgfs_get_lower_path(dentry, &lower_path);
            lower_dentry = lower_path.dentry;
            dget(lower_dentry);
            lower_dir_dentry = lock_parent(lower_dentry);

            err = vfs_unlink(lower_dir_inode, lower_dentry, NULL);

            /*
             * Note: unlinking on top of NFS can cause silly-renamed files.
             * Trying to delete such files results in EBUSY from NFS
             * below.  Silly-renamed files will get deleted by NFS later on, so
             * we just need to detect them here and treat such EBUSY errors as
             * if the upper file was successfully deleted.
             */
            if (err == -EBUSY && lower_dentry->d_flags & DCACHE_NFSFS_RENAMED)
                err = 0;
            if (err) {
                unlock_dir(lower_dir_dentry);
                dput(lower_dentry);
                sgfs_put_lower_path(dentry, &lower_path);
                printk("Could not unlink the file error is %d\n", err);
                return err;
            }
            fsstack_copy_attr_times(dir, lower_dir_inode);
            fsstack_copy_inode_size(dir, lower_dir_inode);
            set_nlink(d_inode(dentry),
                  sgfs_lower_inode(d_inode(dentry))->i_nlink);
            d_inode(dentry)->i_ctime = dir->i_ctime;
            d_drop(dentry);
            unlock_dir(lower_dir_dentry);
            dput(lower_dentry);
            sgfs_put_lower_path(dentry, &lower_path); /* this is needed, else LTP fails (VFS won't do it) */
            printk("Successfully deleted the file\n");
            return 0;
        }

        else{
            printk("I am unlinking after encryption\n");
            struct file *f_read = NULL;
            struct file *f_write = NULL;
            char *buff3 = NULL;
            char *buff4 = NULL;
            // char *buff5 = (char*)kmalloc(sizeof(char)*PAGE_SIZE,GFP_KERNEL);
            char *key = (char*)kmalloc(sizeof(char)*10,GFP_KERNEL);
            size_t key_len, src_len;
            size_t dst_len;
            mm_segment_t oldfs1;
            mm_segment_t oldfs2;
            int bytes1;
            int bytes2;
            int curr_read_size = 0;
            int curr_write_size = 0;
            char *fname = kmalloc(sizeof(char)*PATH_MAX,GFP_KERNEL);
            char *abs_path = kmalloc(sizeof(char)*100,GFP_KERNEL);
            char *year = kmalloc(sizeof(char)*4,GFP_KERNEL);
            char *month = kmalloc(sizeof(char)*2,GFP_KERNEL);
            char *day = kmalloc(sizeof(char)*2,GFP_KERNEL);
            char *hrs = kmalloc(sizeof(char)*2,GFP_KERNEL);
            char *mins = kmalloc(sizeof(char)*2,GFP_KERNEL);
            int c, fsize, temp_size;
            struct inode *Inode;



            struct timeval time;
            struct tm tm;
            unsigned long local_time;
            char uid[5];

            uid_t curr_user;
            struct user_struct *us = current_user();
             curr_user = get_uid(us)->uid.val;
             printk("curr id is %d\n",curr_user);

            c = snprintf(uid,5,"%04d",curr_user);
            strcat(fname,uid);
            strcat(fname,"-");
            
            do_gettimeofday(&time);
            local_time = (u32)(time.tv_sec - (sys_tz.tz_minuteswest * 60));
            rtc_time_to_tm(local_time, &tm);

            c = snprintf(year,5,"%04d",tm.tm_year + 2016);
            strcat(fname,year);
            strcat(fname,"-");
            c = snprintf(month,3,"%02d",tm.tm_mon);
            strcat(fname,month);
            strcat(fname,"-");
            c = snprintf(day,3,"%02d",tm.tm_mday + 1);
            strcat(fname,day);
            strcat(fname,"-");
            c = snprintf(hrs,3,"%02d",tm.tm_hour);
            strcat(fname,hrs);
            strcat(fname,":");
            c = snprintf(mins,3,"%02d",tm.tm_min);
            strcat(fname,mins);
            strcat(fname,"-");
            strcat(fname,dentry->d_iname);
            strcat(fname,".enc");
            printk("filename is %s\n",fname);

            

            f_read = filp_open(path, O_RDONLY, 0);
            if (!f_read || IS_ERR(f_read)) {
            printk("Reading file 1 is failed\n");
            printk("wrapfs_read_file err %d\n", (int) PTR_ERR(f_read));
            return -(int)PTR_ERR(f_read);  // or do something else
            }

            Inode = f_read->f_path.dentry->d_inode;
            fsize = i_size_read(Inode);
            printk("file size is %d\n",fsize);
            strcpy(abs_path,"/usr/src/hw2-vanugu/hw2/sgfs/.sg/");
            strcat(abs_path,fname);

            f_write = filp_open(abs_path, O_WRONLY | O_CREAT, 0644);
            if(!f_write || IS_ERR(f_write)){
                printk("Wrapfs_write_file_4 err %d\n", (int)PTR_ERR(f_write));
                return -(int)PTR_ERR(f_write);
            }


            while(fsize > 0){

                buff3 = (char*)kmalloc(sizeof(char)*PAGE_SIZE,GFP_KERNEL);
                buff4 = (char*)kmalloc(sizeof(char)*PAGE_SIZE,GFP_KERNEL);
                if(fsize > PAGE_SIZE - 16){
                    temp_size = PAGE_SIZE - 16;
                    fsize = fsize - temp_size;
                }
                else {
                    temp_size = fsize;
                    fsize = 0;
                }
                f_read->f_pos = curr_read_size;
                oldfs1 = get_fs();
                set_fs(KERNEL_DS);
                bytes1 = vfs_read(f_read, buff3,temp_size, &f_read->f_pos);
                set_fs(oldfs1);
                printk("data is %s\n",buff3);
                printk("Bytes read is %d\n",bytes1);

                printk("About to encrypt the file\n");
                src_len = bytes1;
                err = sgfs_encryption(key,key_len,buff4,&dst_len,buff3,src_len);
                if(err < 0){
                    printk("Encryption failed\n");
                    kfree(buff3);
                    kfree(buff4);
                    kfree(key);
                    return err;
                }
                printk("Encryption_success\n");
                printk("Encrypted buff is %s\n",buff4);

                f_write->f_pos = curr_write_size;
                oldfs2 = get_fs();
                set_fs(KERNEL_DS);
                bytes2 = vfs_write(f_write, buff4, (int)dst_len, &f_write->f_pos);
                set_fs(oldfs2);
                printk("%d\n",bytes2);

                printk("Successfully written the file\n");
                kfree(buff3);
                kfree(buff4);
                printk("successfully freed the buffer\n");

                curr_read_size += bytes1;
                curr_write_size += bytes2;

            }


            filp_close(f_read,NULL);
            filp_close(f_write,NULL);
            kfree(key);


            sgfs_get_lower_path(dentry, &lower_path);
            lower_dentry = lower_path.dentry;
            dget(lower_dentry);
            lower_dir_dentry = lock_parent(lower_dentry);

            err = vfs_unlink(lower_dir_inode, lower_dentry, NULL);

            /*
             * Note: unlinking on top of NFS can cause silly-renamed files.
             * Trying to delete such files results in EBUSY from NFS
             * below.  Silly-renamed files will get deleted by NFS later on, so
             * we just need to detect them here and treat such EBUSY errors as
             * if the upper file was successfully deleted.
             */
            if (err == -EBUSY && lower_dentry->d_flags & DCACHE_NFSFS_RENAMED)
                err = 0;
            if (err){
                unlock_dir(lower_dir_dentry);
                dput(lower_dentry);
                sgfs_put_lower_path(dentry, &lower_path);
                printk("Couldn't unlink the file error is %d", err);
                return err;
            }
            fsstack_copy_attr_times(dir, lower_dir_inode);
            fsstack_copy_inode_size(dir, lower_dir_inode);
            set_nlink(d_inode(dentry),
                  sgfs_lower_inode(d_inode(dentry))->i_nlink);
            d_inode(dentry)->i_ctime = dir->i_ctime;
            d_drop(dentry);
            unlock_dir(lower_dir_dentry);
            dput(lower_dentry);
            sgfs_put_lower_path(dentry, &lower_path); /* this is needed, else LTP fails (VFS won't do it) */
            printk("Successfully moved the file to the recycle bin after encryption\n");
            return 0;
        }
    }
}